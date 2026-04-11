#include <stdexcept>

#include "test_registry.hpp"

#include "masodik/api/openai_contract.hpp"

MASODIK_TEST(openai_request_audit_derives_content_auto_required_and_named_records) {
    const auto content_only_request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia"}]})");
    const auto auto_request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Mi van a workspace-ben? AGENT.md is van benne?"}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto"})");
    const auto required_request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Olvasd ki az AGENT.md-t."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"required"})");
    const auto named_request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Listazd a workspace tartalmat."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":{"type":"function","function":{"name":"exec"}}})");

    const auto content_record = masodik::api::derive_request_audit_record(
        content_only_request,
        "chatreq-local-1",
        masodik::api::RequestDecisionPath::content_only,
        masodik::api::RequestValidationOutcome::accepted);
    const auto auto_record = masodik::api::derive_request_audit_record(
        auto_request,
        "chatreq-local-2",
        masodik::api::RequestDecisionPath::content_only,
        masodik::api::RequestValidationOutcome::accepted);
    const auto required_record = masodik::api::derive_request_audit_record(
        required_request,
        "chatreq-local-3",
        masodik::api::RequestDecisionPath::tool_call_required,
        masodik::api::RequestValidationOutcome::accepted);
    const auto named_record = masodik::api::derive_request_audit_record(
        named_request,
        "chatreq-local-4",
        masodik::api::RequestDecisionPath::tool_call_named,
        masodik::api::RequestValidationOutcome::accepted);

    masodik::tests::require(!content_record.tools_present, "A content-only rekord nem jelezhet tools_present=true erteket.");
    masodik::tests::require(
        content_record.tool_choice.kind == masodik::api::ToolChoiceSummaryKind::absent,
        "A content-only rekord tool_choice_kind mezoje nem absent.");
    masodik::tests::require(
        auto_record.tool_choice.kind == masodik::api::ToolChoiceSummaryKind::auto_select,
        "Az auto rekord tool_choice_kind mezoje nem auto.");
    masodik::tests::require(
        required_record.decision_path == masodik::api::RequestDecisionPath::tool_call_required,
        "A required rekord decision_path mezoje nem tool_call_required.");
    masodik::tests::require(
        named_record.tool_choice.function_name == "exec",
        "A named rekord tool_choice_name mezoje nem exec.");
}

MASODIK_TEST(openai_request_audit_derives_invalid_named_tool_choice_from_payload) {
    const auto invalid_record = masodik::api::derive_request_audit_record_from_payload(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Nezd meg a workspace tartalmat egy megfelelo toollal."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":{"type":"function","function":{"name":"workspace_inspector"}}})",
        "chatreq-local-invalid",
        masodik::api::RequestDecisionPath::tool_call_named,
        masodik::api::RequestValidationOutcome::tool_schema_error);

    masodik::tests::require(invalid_record.tools_present, "Az invalid named tool-choice rekordnak tools_present=true erteket kell kapnia.");
    masodik::tests::require(invalid_record.tool_count == 1U, "Az invalid named tool-choice rekord tool_count mezoje nem vart.");
    masodik::tests::require(
        invalid_record.tool_choice.kind == masodik::api::ToolChoiceSummaryKind::named_function,
        "Az invalid named tool-choice rekord tool_choice_kind mezoje nem named_function.");
    masodik::tests::require(
        invalid_record.tool_choice.function_name == "workspace_inspector",
        "Az invalid named tool-choice rekord tool_choice_name mezoje nem vart.");
    masodik::tests::require(
        invalid_record.validation_outcome == masodik::api::RequestValidationOutcome::tool_schema_error,
        "Az invalid named tool-choice rekord validation_outcome mezoje nem tool_schema_error.");
}

MASODIK_TEST(openai_request_audit_truncates_message_preview) {
    const std::string long_message(220, 'a');
    const auto preview = masodik::api::truncate_request_audit_preview(long_message);
    masodik::tests::require(preview.size() == 163U, "A request-audit preview hossza nem vart a truncalas utan.");
    masodik::tests::require(preview.ends_with("..."), "A request-audit preview nem ellipszissel zardik.");
}
