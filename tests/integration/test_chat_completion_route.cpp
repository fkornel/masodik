#include <chrono>

#include "in_process_server_fixture.hpp"
#include "streaming_test_support.hpp"
#include "test_registry.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_TEST(chat_completion_route_returns_openai_shaped_payload) {
    auto& host = masodik::tests::shared_in_process_server();
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia pipeline"}]})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "A chat endpoint nem adott 200-as valaszt.");
    masodik::tests::require(
        response.body.find("\"object\":\"chat.completion\"") != std::string::npos, "A chat payload alakja nem vart.");
    masodik::tests::require(
        response.body.find("\"content\":\"") != std::string::npos, "A chat valasz nem tartalmaz assistant content mezot.");
    masodik::tests::require(
        response.body.find("Placeholder valasz") == std::string::npos, "A chat valasz nem lehet placeholder mod.");
}

MASODIK_TEST(chat_completion_route_accepts_openai_content_parts_array) {
    auto& host = masodik::tests::shared_in_process_server();
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":[{"type":"text","text":"Hello OpenClaw"}]}]})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "A content-part tombbel kuldott chat keres nem adott 200-at.");
    masodik::tests::require(
        response.body.find("\"object\":\"chat.completion\"") != std::string::npos,
        "A content-part tombbel kuldott valasz nem OpenAI alaku.");
}

MASODIK_TEST(chat_completion_route_accepts_assistant_null_content_history) {
    auto& host = masodik::tests::shared_in_process_server();
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"assistant","content":null,"tool_calls":[{"id":"call_1","type":"function","function":{"name":"exec","arguments":"{\"command\":\"gh pr list\"}"}}]},{"role":"user","content":"Folytasd innen."}]})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "Az assistant null content historyvel kuldott chat keres nem adott 200-at.");
    masodik::tests::require(
        response.body.find("\"object\":\"chat.completion\"") != std::string::npos,
        "Az assistant null content historyvel kuldott valasz nem OpenAI alaku.");
}

MASODIK_TEST(chat_completion_route_rejects_unsupported_content_parts_with_400) {
    auto& host = masodik::tests::shared_in_process_server();
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":[{"type":"image_url","image_url":"https://example.invalid/image.png"}]}]})",
        std::chrono::seconds{30});

    masodik::tests::require(response.status_code == 400, "A nem tamogatott content partnak 400-as valaszt kell adnia.");
    masodik::tests::require(
        response.body.find("\"code\":\"bad_request\"") != std::string::npos,
        "A nem tamogatott content part hibaja nem validacios hibakent jelent meg.");
}

MASODIK_TEST(chat_completion_route_streams_sse_when_requested_and_preserves_json_parity) {
    auto& host = masodik::tests::shared_in_process_server();
    masodik::http::HttpClient client;

    const auto streaming_response = masodik::tests::post_raw_http(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia streaming"}],"stream":true,"max_tokens":32})",
        std::chrono::minutes{10});

    masodik::tests::require(streaming_response.status_code == 200, "A stream=true keres nem adott 200-as valaszt.");
    masodik::tests::require(
        streaming_response.raw_headers.find("Content-Type: text/event-stream") != std::string::npos,
        "A stream=true valasz fejlece nem text/event-stream.");
    masodik::tests::require(
        streaming_response.body.find("\"object\":\"chat.completion.chunk\"") != std::string::npos,
        "A stream=true valasz nem chunk payloadokat adott vissza.");
    masodik::tests::require(
        streaming_response.body.find("\"role\":\"assistant\"") != std::string::npos,
        "A stream valaszbol hianyzik a kezdo assistant delta.");
    masodik::tests::require(
        streaming_response.body.find("data: ") < streaming_response.body.find("data: [DONE]"),
        "A stream esemenyek sorrendje nem vart.");
    masodik::tests::require(
        streaming_response.body.find("data: [DONE]") != std::string::npos,
        "A stream valasz nem tartalmazta a [DONE] lezarat.");

    const auto json_response = masodik::tests::post_raw_http(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia streaming"}],"stream":false,"max_tokens":32})",
        std::chrono::minutes{10});

    masodik::tests::require(json_response.status_code == 200, "A stream=false parity keres nem adott 200-at.");
    masodik::tests::require(
        json_response.raw_headers.find("Content-Type: application/json") != std::string::npos,
        "A stream=false valasz fejlece nem application/json.");
    masodik::tests::require(
        json_response.body.find("\"object\":\"chat.completion\"") != std::string::npos,
        "A stream=false valasz nem a vart chat.completion objektum.");
    masodik::tests::require(
        json_response.body.find("data: [DONE]") == std::string::npos,
        "A stream=false valasz nem tartalmazhat SSE lezarat.");
}
