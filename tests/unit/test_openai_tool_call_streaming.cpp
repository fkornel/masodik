#include "test_registry.hpp"

#include "masodik/api/openai_contract.hpp"

MASODIK_TEST(openai_tool_call_streaming_serializes_indexed_deltas) {
    masodik::api::ChatCompletionChunkDelta delta;
    delta.tool_calls.push_back(
        {.index = 0,
         .id = "call_exec_0",
         .type = "function",
         .function = {.name = "exec", .arguments = "{\"command\":\"git "}});

    const auto chunk = masodik::api::make_chat_completion_chunk_response(
        "chatcmpl-tool-stream",
        "gpt-oss-20b-int4-ov",
        0,
        std::move(delta));
    const auto json = masodik::api::to_json(chunk);

    masodik::tests::require(
        json.find("\"tool_calls\":[") != std::string::npos,
        "A streaming tool-call delta payloadbol hianyzik a tool_calls tomb.");
    masodik::tests::require(
        json.find("\"index\":0") != std::string::npos,
        "A streaming tool-call delta payloadbol hianyzik az index mezo.");
    masodik::tests::require(
        json.find("\"name\":\"exec\"") != std::string::npos,
        "A streaming tool-call delta payloadbol hianyzik a function.name mezo.");
}

MASODIK_TEST(openai_tool_call_streaming_serializes_terminal_finish_reason_without_duplicate_delta) {
    const auto terminal = masodik::api::make_terminal_chat_completion_chunk_response(
        "chatcmpl-tool-stream",
        "gpt-oss-20b-int4-ov",
        0,
        "tool_calls");
    const auto terminal_json = masodik::api::to_json(terminal);

    masodik::tests::require(
        terminal_json.find("\"finish_reason\":\"tool_calls\"") != std::string::npos,
        "A terminalis tool-call chunk finish_reason mezoje nem vart.");
    masodik::tests::require(
        terminal_json.find("\"tool_calls\":[") == std::string::npos,
        "A terminalis tool-call chunk nem duplikalhatja kotelezoen a delta tombot.");
}
