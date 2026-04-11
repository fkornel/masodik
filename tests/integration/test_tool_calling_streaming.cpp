#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "in_process_server_fixture.hpp"
#include "streaming_test_support.hpp"
#include "test_registry.hpp"

#include "masodik/config/json_loader.hpp"

namespace {

std::vector<std::string> split_sse_payloads(const std::string_view body) {
    std::vector<std::string> payloads;
    std::size_t cursor = 0;
    while (cursor < body.size()) {
        const auto separator = body.find("\n\n", cursor);
        const auto event_end = separator == std::string_view::npos ? body.size() : separator;
        const auto event = body.substr(cursor, event_end - cursor);
        std::string payload;
        std::size_t line_start = 0;
        while (line_start < event.size()) {
            const auto line_end = event.find('\n', line_start);
            const auto token_end = line_end == std::string_view::npos ? event.size() : line_end;
            const auto line = event.substr(line_start, token_end - line_start);
            if (line.rfind("data: ", 0) == 0) {
                payload += std::string{line.substr(6)};
            }
            if (line_end == std::string_view::npos) {
                break;
            }
            line_start = line_end + 1;
        }
        if (!payload.empty() && payload != "[DONE]") {
            payloads.push_back(std::move(payload));
        }
        if (separator == std::string_view::npos) {
            break;
        }
        cursor = separator + 2;
    }
    return payloads;
}

}  // namespace

MASODIK_TEST(tool_calling_streaming_preserves_delta_ordering_and_argument_assembly) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "tool-stream-01", .bind_port = 29127});
    masodik::tests::InProcessServerFixture host{fixture.profile_path, 29127, fixture.root_path};

    const auto response = masodik::tests::post_raw_http(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Deritsd ki a nyitott PR-eket egy elerheto toollal."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis shell parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto","stream":true,"max_tokens":96})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "A streaming tool-calling keres nem adott 200-as valaszt.");
    masodik::tests::require(
        response.raw_headers.find("Content-Type: text/event-stream") != std::string::npos,
        "A streaming tool-calling valasz fejlece nem text/event-stream.");
    masodik::tests::require(
        response.body.find("data: [DONE]") != std::string::npos,
        "A streaming tool-calling valasz nem tartalmazta a [DONE] lezarat.");

    const auto payloads = split_sse_payloads(response.body);
    masodik::tests::require(payloads.size() >= 3U, "A streaming tool-calling valasz nem adott eleg SSE payloadot.");

    std::string assembled_arguments;
    bool saw_assistant_role = false;
    bool saw_tool_name = false;
    bool saw_terminal_tool_calls = false;
    for (const auto& payload : payloads) {
        const auto root = masodik::config::JsonLoader::parse(payload);
        masodik::tests::require(root.is_object(), "Az SSE payload nem JSON objektum.");
        const auto& choice = masodik::config::require_member(root.as_object(), "choices").as_array().front().as_object();
        const auto& delta = masodik::config::require_member(choice, "delta");
        masodik::tests::require(delta.is_object(), "Az SSE payload delta mezoje nem objektum.");
        const auto& delta_object = delta.as_object();
        if (const auto* role = masodik::config::find_member(delta_object, "role"); role != nullptr && role->is_string()) {
            saw_assistant_role = role->as_string() == "assistant";
        }
        if (const auto* tool_calls = masodik::config::find_member(delta_object, "tool_calls");
            tool_calls != nullptr && tool_calls->is_array()) {
            for (const auto& tool_call_value : tool_calls->as_array()) {
                const auto& tool_call = tool_call_value.as_object();
                masodik::tests::require(
                    masodik::config::get_number(tool_call, "index") == 0.0,
                    "A streamelt tool-call delta indexe nem 0.");
                if (const auto* function_value = masodik::config::find_member(tool_call, "function");
                    function_value != nullptr && function_value->is_object()) {
                    const auto& function = function_value->as_object();
                    if (const auto* name = masodik::config::find_member(function, "name"); name != nullptr && name->is_string()) {
                        saw_tool_name = name->as_string() == "exec";
                    }
                    if (const auto* arguments = masodik::config::find_member(function, "arguments");
                        arguments != nullptr && arguments->is_string()) {
                        assembled_arguments += arguments->as_string();
                    }
                }
            }
        }
        if (const auto* finish_reason = masodik::config::find_member(choice, "finish_reason");
            finish_reason != nullptr && finish_reason->is_string()) {
            saw_terminal_tool_calls = finish_reason->as_string() == "tool_calls";
        }
    }

    masodik::tests::require(saw_assistant_role, "A streamelt tool-calling valaszbol hianyzik a kezdo assistant delta.");
    masodik::tests::require(saw_tool_name, "A streamelt tool-calling deltak nem tartalmaztak exec function nevet.");
    masodik::tests::require(
        assembled_arguments.find("gh pr list --json number,title,state") != std::string::npos,
        "A streamelt tool-calling arguments deltai nem alltak ossze a vart parancsra.");
    masodik::tests::require(
        saw_terminal_tool_calls,
        "A streamelt tool-calling terminalis finish_reason nem tool_calls.");
}
