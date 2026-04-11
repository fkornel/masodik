#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "openai_route_test_support.hpp"
#include "streaming_test_support.hpp"
#include "test_registry.hpp"

#include "masodik/diagnostics/logger.hpp"

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream{path};
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

std::string extract_request_id(std::string_view raw_headers) {
    constexpr std::string_view header_name = "X-Request-Id: ";
    const auto start = raw_headers.find(header_name);
    if (start == std::string_view::npos) {
        throw std::runtime_error("A valasz headerek nem tartalmaztak X-Request-Id mezot.");
    }

    const auto value_start = start + header_name.size();
    const auto value_end = raw_headers.find("\r\n", value_start);
    return std::string{raw_headers.substr(value_start, value_end - value_start)};
}

void require_log_contains(const std::string& log_content, std::string_view needle, std::string_view message) {
    masodik::tests::require(log_content.find(needle) != std::string::npos, message);
}

}  // namespace

MASODIK_TEST(request_audit_logging_records_non_stream_streaming_and_validation_failure_routes) {
    const auto log_path = std::filesystem::temp_directory_path() / "masodik-request-audit-integration.log";
    std::filesystem::remove(log_path);

    std::string non_stream_request_id;
    std::string required_request_id;
    std::string streaming_request_id;
    std::string validation_failure_request_id;
    {
        auto route_support = masodik::tests::make_route_support(
            masodik::runtime::build_service_config(
                masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
                masodik::config::default_paths::default_model_root,
                masodik::tests::find_repo_root()));
        route_support->request_audit_logger = std::make_shared<masodik::diagnostics::Logger>(
            masodik::diagnostics::Logger::create_file_logger(log_path));
        route_support->request_audit_prefix = "chatreq-integration";

        masodik::tests::ConfigurableInProcessServerFixture host{29128, {}, route_support};

        const auto non_stream_response = masodik::tests::post_raw_http(
            host.service_config().chat_url,
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia"}]})",
            std::chrono::minutes{10},
            "application/json");
        masodik::tests::require(non_stream_response.status_code == 200, "A non-stream request audit teszt nem adott 200-at.");
        non_stream_request_id = extract_request_id(non_stream_response.raw_headers);

        const auto streaming_response = masodik::tests::post_raw_http(
            host.service_config().chat_url,
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Sorold fel a nyitott PR-eket a helyi repositoryhoz."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto","stream":true})",
            std::chrono::minutes{10});
        masodik::tests::require(streaming_response.status_code == 200, "A streaming request audit teszt nem adott 200-at.");
        streaming_request_id = extract_request_id(streaming_response.raw_headers);

        const auto required_response = masodik::tests::post_raw_http(
            host.service_config().chat_url,
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Olvasd ki, mi van az AGENT.md-ben a workspace-bol."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"required"})",
            std::chrono::minutes{10},
            "application/json");
        masodik::tests::require(required_response.status_code == 200, "A required request audit teszt nem adott 200-at.");
        required_request_id = extract_request_id(required_response.raw_headers);

        const auto validation_failure_response = masodik::tests::post_raw_http(
            host.service_config().chat_url,
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Nezd meg a workspace tartalmat egy megfelelo toollal."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":{"type":"function","function":{"name":"workspace_inspector"}}})",
            std::chrono::minutes{10},
            "application/json");
        masodik::tests::require(validation_failure_response.status_code == 400, "A validation-failure request audit teszt nem adott 400-at.");
        validation_failure_request_id = extract_request_id(validation_failure_response.raw_headers);
    }

    const auto log_content = read_file(log_path);
    std::filesystem::remove(log_path);

    require_log_contains(log_content, "request_audit {", "A request-audit logger nem a vart markerrel irta ki a rekordokat.");
    require_log_contains(log_content, non_stream_request_id, "A non-stream request_id nem jelent meg a request-audit logban.");
    require_log_contains(log_content, "\"decision_path\":\"content_only\"", "A non-stream request-audit log nem tartalmazta a content_only decision pathot.");
    require_log_contains(log_content, "\"validation_outcome\":\"accepted\"", "A non-stream request-audit log nem tartalmazta az accepted validation_outcome mezot.");

    require_log_contains(log_content, streaming_request_id, "A streaming request_id nem jelent meg a request-audit logban.");
    require_log_contains(log_content, "\"stream\":true", "A streaming request-audit log nem tartalmazta a stream=true mezot.");
    require_log_contains(log_content, "\"tool_choice_kind\":\"auto\"", "A streaming request-audit log nem tartalmazta az auto tool_choice_kind mezot.");
    require_log_contains(log_content, "\"decision_path\":\"tool_call_auto\"", "A streaming request-audit log nem tartalmazta a tool_call_auto decision pathot.");
    require_log_contains(log_content, "\"tools_present\":true", "A request-audit log nem tartalmazta a tools_present=true mezot.");
    require_log_contains(log_content, "\"tool_count\":1", "A request-audit log nem tartalmazta a tool_count=1 mezot.");

    require_log_contains(log_content, required_request_id, "A required request_id nem jelent meg a request-audit logban.");
    require_log_contains(log_content, "\"tool_choice_kind\":\"required\"", "A required request-audit log nem tartalmazta a required tool_choice_kind mezot.");
    require_log_contains(log_content, "\"decision_path\":\"tool_call_required\"", "A required request-audit log nem tartalmazta a tool_call_required decision pathot.");

    require_log_contains(log_content, validation_failure_request_id, "A validation-failure request_id nem jelent meg a request-audit logban.");
    require_log_contains(log_content, "\"tool_choice_name\":\"workspace_inspector\"", "A validation-failure request-audit log nem tartalmazta a named tool_choice_name mezot.");
    require_log_contains(log_content, "\"validation_outcome\":\"tool_schema_error\"", "A validation-failure request-audit log nem tartalmazta a tool_schema_error validation_outcome mezot.");
}
