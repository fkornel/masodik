#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "test_registry.hpp"

#include "masodik/api/openai_contract.hpp"
#include "masodik/diagnostics/logger.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/server/openai_error_mapper.hpp"

MASODIK_TEST(request_audit_foundation_serializes_request_audit_record_and_tool_choice_summary) {
    const auto request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Kerd le a statuszt."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"required"})");

    const auto summary = masodik::api::summarize_tool_choice(request);
    masodik::tests::require(
        summary.kind == masodik::api::ToolChoiceSummaryKind::required,
        "A tool-choice summary nem required allapotba kerult.");

    masodik::api::RequestAuditRecord record;
    record.request_id = "chatreq-local-17";
    record.stream = request.stream;
    record.tools_present = masodik::api::has_tool_definitions(request);
    record.tool_count = request.tools.size();
    record.tool_choice = summary;
    record.decision_path = masodik::api::RequestDecisionPath::tool_call_required;
    record.validation_outcome = masodik::api::RequestValidationOutcome::accepted;
    record.truncated_message_preview = "Kerd le a statuszt.";

    const auto record_json = masodik::api::to_json(record);
    masodik::tests::require(
        record_json.find("\"tool_choice_kind\":\"required\"") != std::string::npos,
        "A request-audit JSON-bol hianyzik a required tool_choice_kind.");
    masodik::tests::require(
        record_json.find("\"decision_path\":\"tool_call_required\"") != std::string::npos,
        "A request-audit JSON-bol hianyzik a tool_call_required decision_path.");
    masodik::tests::require(
        record_json.find("\"validation_outcome\":\"accepted\"") != std::string::npos,
        "A request-audit JSON-bol hianyzik az accepted validation_outcome.");
}

MASODIK_TEST(request_audit_foundation_classifies_decision_path_and_validation_outcome) {
    const auto content_only_request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia"}]})");
    const auto auto_tool_request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Futtasd a statusz ellenorzest a megfelelo toollal."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto"})");

    masodik::tests::require(
        masodik::genai::classify_request_decision_path(content_only_request) ==
            masodik::api::RequestDecisionPath::content_only,
        "A content-only keres nem content_only decision pathot kapott.");
    masodik::tests::require(
        masodik::genai::classify_request_decision_path(auto_tool_request) ==
            masodik::api::RequestDecisionPath::tool_call_auto,
        "Az auto tool keres nem tool_call_auto decision pathot kapott.");

    const auto agent_md_request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"check your AGENT.md"}],"tools":[{"type":"function","function":{"name":"read_file","description":"Olvass helyi fajlt path alapjan","parameters":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}}}],"tool_choice":"auto"})");
    const auto ssh_request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"open ssh for 192.168.1.10"}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis shell parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto"})");

    masodik::tests::require(
        masodik::genai::classify_request_decision_path(agent_md_request) ==
            masodik::api::RequestDecisionPath::tool_call_auto,
        "Az AGENT.md jellegu keres nem tool_call_auto decision pathot kapott.");
    masodik::tests::require(
        masodik::genai::classify_request_decision_path(ssh_request) ==
            masodik::api::RequestDecisionPath::tool_call_auto,
        "Az SSH jellegu keres nem tool_call_auto decision pathot kapott.");

    const std::invalid_argument parser_error{"A messages tomb nem lehet ures."};
    const std::invalid_argument tool_schema_error{"A tool_choice.function.name nem egyezik egyetlen meghirdetett tool nevével sem."};
    const std::invalid_argument model_error{"A kert model nem egyezik a helyi aktiv modellel."};
    const std::logic_error readiness_error{"A szolgaltatas nem ready."};

    masodik::tests::require(
        masodik::server::classify_validation_outcome(parser_error) ==
            masodik::api::RequestValidationOutcome::parser_error,
        "Az altalanos parser hiba nem parser_error validation_outcome lett.");
    masodik::tests::require(
        masodik::server::classify_validation_outcome(tool_schema_error, {.tool_call_related = true}) ==
            masodik::api::RequestValidationOutcome::tool_schema_error,
        "A tool schema hiba nem tool_schema_error validation_outcome lett.");
    masodik::tests::require(
        masodik::server::classify_validation_outcome(model_error) ==
            masodik::api::RequestValidationOutcome::model_selection_error,
        "A model mismatch nem model_selection_error validation_outcome lett.");
    masodik::tests::require(
        masodik::server::classify_validation_outcome(readiness_error) ==
            masodik::api::RequestValidationOutcome::runtime_readiness_error,
        "A not-ready hiba nem runtime_readiness_error validation_outcome lett.");
}

MASODIK_TEST(request_audit_foundation_formats_structured_logger_lines) {
    const auto temp_log_path = std::filesystem::temp_directory_path() / "masodik-request-audit-foundation.log";
    std::filesystem::remove(temp_log_path);

    {
        auto logger = masodik::diagnostics::Logger::create_file_logger(temp_log_path);
        logger.request_audit(
            {.request_id = "chatreq-test-1",
             .stream = false,
             .tools_present = true,
             .tool_count = 1,
             .tool_choice = {.kind = masodik::api::ToolChoiceSummaryKind::named_function, .function_name = "exec"},
             .decision_path = masodik::api::RequestDecisionPath::tool_call_named,
             .validation_outcome = masodik::api::RequestValidationOutcome::accepted,
             .truncated_message_preview = "Olvasd ki az AGENT.md-t."});
    }

    std::ifstream log_stream{temp_log_path};
    const std::string content{std::istreambuf_iterator<char>{log_stream}, std::istreambuf_iterator<char>{}};
    log_stream.close();
    std::filesystem::remove(temp_log_path);

    masodik::tests::require(
        content.find("request_audit {") != std::string::npos,
        "A logger nem request_audit markerrel irta ki az audit rekordot.");
    masodik::tests::require(
        content.find("\"tool_choice_kind\":\"named_function\"") != std::string::npos,
        "A logger nem irta ki a named_function tool-choice summaryt.");
    masodik::tests::require(
        content.find("\"decision_path\":\"tool_call_named\"") != std::string::npos,
        "A logger nem irta ki a tool_call_named decision pathot.");
}
