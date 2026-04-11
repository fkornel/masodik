#include "test_registry.hpp"

#include <stdexcept>

#include "masodik/api/openai_contract.hpp"

MASODIK_TEST(openai_contract_parses_minimal_chat_completion_request) {
    const auto request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia"}]})");

    masodik::tests::require(request.model == "gpt-oss-20b-int4-ov", "A model azonosito nem vart.");
    masodik::tests::require(request.messages.size() == 1U, "Pontosan egy uzenet vart.");
    masodik::tests::require(request.messages.front().content == "Szia", "Az uzenet tartalom nem vart.");
}

MASODIK_TEST(openai_contract_serializes_health_and_model_payloads) {
    const auto health_json = masodik::api::to_json(
        {.ready = true,
         .state = "ready",
         .active_model = "gpt-oss-20b-int4-ov",
         .active_profile = "local-b580",
         .active_streams = 1,
         .peak_active_streams = 2,
         .total_stream_failures = 3,
         .last_disconnect_error = 10054});
    const auto models_json = masodik::api::to_json(
        std::vector<masodik::api::ModelCard>{{.id = "gpt-oss-20b-int4-ov", .object = "model", .owned_by = "local"}});

    masodik::tests::require(health_json.find("\"state\":\"ready\"") != std::string::npos, "A health JSON nem vart.");
    masodik::tests::require(health_json.find("\"active_streams\":1") != std::string::npos, "A health JSON nem tartalmaz active_streams mezot.");
    masodik::tests::require(
        health_json.find("\"peak_active_streams\":2") != std::string::npos,
        "A health JSON nem tartalmaz peak_active_streams mezot.");
    masodik::tests::require(
        health_json.find("\"total_stream_failures\":3") != std::string::npos,
        "A health JSON nem tartalmaz total_stream_failures mezot.");
    masodik::tests::require(
        health_json.find("\"last_disconnect_error\":10054") != std::string::npos,
        "A health JSON nem tartalmaz last_disconnect_error mezot.");
    masodik::tests::require(
        models_json.find("\"id\":\"gpt-oss-20b-int4-ov\"") != std::string::npos, "A models JSON nem vart.");
}

MASODIK_TEST(openai_contract_parses_openai_content_parts_array) {
    const auto request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":[{"type":"text","text":"Hello"},{"type":"text","text":"vilag"}]}]})");

    masodik::tests::require(request.messages.size() == 1U, "Pontosan egy uzenet vart.");
    masodik::tests::require(
        request.messages.front().content == "Hello\nvilag",
        "A text-part tomb flattenelt tartalma nem vart.");
}

MASODIK_TEST(openai_contract_accepts_assistant_null_content_in_history) {
    const auto request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"assistant","content":null,"tool_calls":[{"id":"call_1","type":"function","function":{"name":"exec","arguments":"{\"command\":\"pwd\"}"}}]},{"role":"user","content":"Szia"}]})");

    masodik::tests::require(request.messages.size() == 2U, "Pontosan ket uzenet vart.");
    masodik::tests::require(request.messages.front().role == "assistant", "Az elso uzenet role nem vart.");
    masodik::tests::require(request.messages.front().content.empty(), "A null assistant contentnek ures stringge kell lapulnia.");
    masodik::tests::require(request.messages.front().tool_calls.size() == 1U, "Az assistant history tool_calls tombjet meg kell orizni.");
    masodik::tests::require(request.messages.front().tool_calls.front().function.name == "exec", "A history tool_call neve nem vart.");
    masodik::tests::require(
        request.messages.front().tool_calls.front().function.arguments == "{\"command\":\"pwd\"}",
        "A history tool_call arguments mezojet nyers stringkent meg kell orizni.");
    masodik::tests::require(request.messages.back().content == "Szia", "A user uzenet tartalma nem vart.");
}

MASODIK_TEST(openai_contract_rejects_unsupported_content_parts_as_bad_request) {
    try {
        static_cast<void>(masodik::api::parse_chat_completion_request(
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":[{"type":"image_url","image_url":"https://example.invalid/image.png"}]}]})"));
    } catch (const std::invalid_argument&) {
        return;
    }

    throw std::runtime_error("A nem tamogatott content partnak invalid_argument hibaval kell elbuknia.");
}

MASODIK_TEST(openai_contract_rejects_user_null_content) {
    try {
        static_cast<void>(masodik::api::parse_chat_completion_request(
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":null}]} )"));
    } catch (const std::invalid_argument&) {
        return;
    }

    throw std::runtime_error("A user null contentnek tovabbra is invalid_argument hibaval kell elbuknia.");
}

MASODIK_TEST(openai_contract_parses_tools_and_named_tool_choice) {
    const auto request = masodik::api::parse_chat_completion_request(
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Kerj tool hivast."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":{"type":"function","function":{"name":"exec"}}})");

    masodik::tests::require(request.tools.size() == 1U, "Pontosan egy tool definicio vart.");
    masodik::tests::require(request.tools.front().function.name == "exec", "A tool nev nem vart.");
    masodik::tests::require(
        request.tools.front().function.parameters_json.find("\"required\":[\"command\"]") != std::string::npos,
        "A tool schema nem lett kompakt JSON-kent megorizve.");
    masodik::tests::require(
        request.tool_choice.kind == masodik::api::ToolChoiceKind::named_function,
        "A tool_choice nem named_function allapotba kerult.");
    masodik::tests::require(request.tool_choice.function_name == "exec", "A named tool_choice neve nem vart.");
}

MASODIK_TEST(openai_contract_rejects_unknown_named_tool_choice) {
    try {
        static_cast<void>(masodik::api::parse_chat_completion_request(
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Kerj tool hivast."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}}}}}],"tool_choice":{"type":"function","function":{"name":"missing"}}})"));
    } catch (const std::invalid_argument&) {
        return;
    }

    throw std::runtime_error("Az ismeretlen named tool_choice-nak invalid_argument hibaval kell elbuknia.");
}

MASODIK_TEST(openai_contract_serializes_stream_chunks_and_done_sentinel) {
    const auto role_chunk = masodik::api::make_chat_completion_chunk_response(
        "chatcmpl-stream-1",
        "gpt-oss-20b-int4-ov",
        0,
        "assistant",
        {});
    const auto content_chunk = masodik::api::make_chat_completion_chunk_response(
        "chatcmpl-stream-1",
        "gpt-oss-20b-int4-ov",
        0,
        std::string{},
        "Szia");
    const auto terminal_chunk = masodik::api::make_terminal_chat_completion_chunk_response(
        "chatcmpl-stream-1",
        "gpt-oss-20b-int4-ov",
        0,
        "stop");

    const auto role_json = masodik::api::to_json(role_chunk);
    const auto content_json = masodik::api::to_json(content_chunk);
    const auto terminal_json = masodik::api::to_json(terminal_chunk);
    const auto sse_payload = masodik::api::to_sse_json_event(content_json);
    const auto done_payload = masodik::api::to_sse_done_event();

    masodik::tests::require(
        role_json.find("\"object\":\"chat.completion.chunk\"") != std::string::npos,
        "A role chunk nem chat.completion.chunk objektum.");
    masodik::tests::require(
        role_json.find("\"role\":\"assistant\"") != std::string::npos,
        "A role chunk nem tartalmaz assistant deltát.");
    masodik::tests::require(
        content_json.find("\"content\":\"Szia\"") != std::string::npos,
        "A content chunk nem tartalmazza a delta content mezot.");
    masodik::tests::require(
        terminal_json.find("\"finish_reason\":\"stop\"") != std::string::npos,
        "A terminalis chunk nem tartalmaz finish_reason mezot.");
    masodik::tests::require(
        sse_payload.find("data: {") != std::string::npos,
        "Az SSE payload nem data: prefixszel kezdodik.");
    masodik::tests::require(done_payload == "data: [DONE]\n\n", "A [DONE] sentinel nem vart.");
}

MASODIK_TEST(openai_contract_serializes_non_stream_and_stream_tool_calls) {
    masodik::api::ChatCompletionResponse response;
    response.id = "chatcmpl-tool";
    response.model = "gpt-oss-20b-int4-ov";
    response.choices.push_back(
        {.index = 0,
         .message =
             {.role = "assistant",
              .content = {},
              .tool_calls = {{.id = "call_1",
                              .type = "function",
                              .function = {.name = "exec", .arguments = "{\"command\":\"dir\"}"},
                              .index = 0}}},
         .finish_reason = "tool_calls"});

    const auto response_json = masodik::api::to_json(response);
    const auto chunk = masodik::api::make_terminal_chat_completion_chunk_response(
        "chatcmpl-tool",
        "gpt-oss-20b-int4-ov",
        0,
        "tool_calls",
        {{.index = 0,
          .id = "call_1",
          .type = "function",
          .function = {.name = "exec", .arguments = "{\"command\":\"dir\"}"}}});
    const auto chunk_json = masodik::api::to_json(chunk);

    masodik::tests::require(
        response_json.find("\"tool_calls\":[") != std::string::npos,
        "A non-stream tool calling valaszbol hianyzik a tool_calls tomb.");
    masodik::tests::require(
        response_json.find("\"content\":null") != std::string::npos,
        "A non-stream tool calling valaszban a content mezot nullkent kell serializalni.");
    masodik::tests::require(
        response_json.find("\"finish_reason\":\"tool_calls\"") != std::string::npos,
        "A non-stream tool calling finish_reason nem vart.");
    masodik::tests::require(
        chunk_json.find("\"tool_calls\":[") != std::string::npos,
        "A streamelt tool-call chunkbol hianyzik a tool_calls delta tomb.");
    masodik::tests::require(
        chunk_json.find("\"arguments\":\"{\\\"command\\\":\\\"dir\\\"}\"") != std::string::npos,
        "A streamelt tool-call chunk nem tartalmazza az arguments delta tartalmat.");
}
