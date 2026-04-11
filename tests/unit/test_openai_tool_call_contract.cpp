#include "test_registry.hpp"

#include <stdexcept>

#include "masodik/api/openai_contract.hpp"

MASODIK_TEST(openai_tool_call_contract_parses_required_tool_choice_and_exec_schema) {
    const auto request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Futtasd a statusz ellenorzest."}],"tools":[{"type":"function","function":{"name":"exec","description":"Parancs futtatasa","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"required"})");

    masodik::tests::require(request.tools.size() == 1U, "Pontosan egy tool definicio vart.");
    masodik::tests::require(
        request.tool_choice.kind == masodik::api::ToolChoiceKind::required,
        "A tool_choice nem required allapotba kerult.");
    masodik::tests::require(
        request.tools.front().function.parameters_json.find("\"command\"") != std::string::npos,
        "A tool schema nem tartalmazza a command mezot.");
}

MASODIK_TEST(openai_tool_call_contract_rejects_duplicate_tool_names) {
    try {
        static_cast<void>(masodik::api::parse_chat_completion_request(
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Sorold fel a PR-eket."}],"tools":[{"type":"function","function":{"name":"exec","description":"Elso","parameters":{"type":"object","properties":{"command":{"type":"string"}}}}},{"type":"function","function":{"name":"exec","description":"Masodik","parameters":{"type":"object","properties":{"command":{"type":"string"}}}}}]})"));
    } catch (const std::invalid_argument&) {
        return;
    }

    throw std::runtime_error("A duplikalt tool neveknek invalid_argument hibaval kell elbukniuk.");
}

MASODIK_TEST(openai_tool_call_contract_serializes_non_stream_tool_call_payload) {
    masodik::api::ChatCompletionResponse response;
    response.id = "chatcmpl-tool-contract";
    response.model = "gpt-oss-20b-int4-ov";
    response.choices.push_back(
        {.index = 0,
         .message =
             {.role = "assistant",
              .content = {},
              .tool_calls = {{.id = "call_exec_0",
                              .type = "function",
                              .function = {.name = "exec", .arguments = "{\"command\":\"git status --short\"}"},
                              .index = 0}}},
         .finish_reason = "tool_calls"});

    const auto response_json = masodik::api::to_json(response);

    masodik::tests::require(
        response_json.find("\"content\":null") != std::string::npos,
        "A tool-calling non-stream payloadban a content mezot nullkent kell serializalni.");
    masodik::tests::require(
        response_json.find("\"tool_calls\":[") != std::string::npos,
        "A tool-calling non-stream payloadbol hianyzik a tool_calls tomb.");
    masodik::tests::require(
        response_json.find("\"finish_reason\":\"tool_calls\"") != std::string::npos,
        "A tool-calling non-stream payload finish_reason mezoje nem vart.");
}
