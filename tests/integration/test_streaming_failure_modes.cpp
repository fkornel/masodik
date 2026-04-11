#include <chrono>
#include <functional>
#include <memory>

#include "openai_route_test_support.hpp"
#include "streaming_test_support.hpp"
#include "test_registry.hpp"

MASODIK_TEST(streaming_failure_modes_return_json_before_stream_when_runtime_not_ready) {
    masodik::tests::ConfigurableInProcessServerFixture fixture{
        29130,
        [](masodik::runtime::ServiceConfig& config) {
            config.runtime_dependencies.openvino_genai_dll_exists = false;
            config.runtime_dependencies.openvino_genai_c_dll_exists = false;
        },
        {},
        false};

    const auto response = masodik::tests::post_raw_http(
        fixture.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia"}],"stream":true,"max_tokens":32})",
        std::chrono::minutes{2});

    masodik::tests::require(response.status_code == 503, "A not-ready stream keresnek 503-as JSON hibat kell adnia.");
    masodik::tests::require(
        response.raw_headers.find("Content-Type: application/json") != std::string::npos,
        "A not-ready stream keres nem JSON hibavalasz lett.");
    masodik::tests::require(
        response.body.find("\"code\":\"service_unavailable\"") != std::string::npos,
        "A not-ready stream hiba nem service_unavailable kodot adott.");
    masodik::tests::require(
        response.body.find("data: ") == std::string::npos,
        "A not-ready stream hiba nem nyithat SSE streamet.");
}

MASODIK_TEST(streaming_failure_modes_return_json_before_stream_for_model_mismatch_and_timeout) {
    masodik::tests::ConfigurableInProcessServerFixture mismatch_fixture{29131};

    const auto mismatch_response = masodik::tests::post_raw_http(
        mismatch_fixture.service_config().chat_url,
        R"({"model":"mas-modell","messages":[{"role":"user","content":"Szia"}],"stream":true,"max_tokens":32})",
        std::chrono::minutes{2});

    masodik::tests::require(mismatch_response.status_code == 400, "A modell mismatch stream keresnek 400-at kell adnia.");
    masodik::tests::require(
        mismatch_response.raw_headers.find("Content-Type: application/json") != std::string::npos,
        "A modell mismatch stream keres nem JSON hiba lett.");
    masodik::tests::require(
        mismatch_response.body.find("\"code\":\"bad_request\"") != std::string::npos,
        "A modell mismatch hiba nem bad_request kodot adott.");

    auto timeout_config = masodik::runtime::build_service_config(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        masodik::config::default_paths::default_model_root,
        masodik::tests::find_repo_root());
    timeout_config.serving_profile.timeout_seconds = 1;
    timeout_config.serving_profile.queue_policy.max_queue_wait_seconds = 31;
    auto timeout_route_support = masodik::tests::make_route_support(timeout_config);
    masodik::tests::ConfigurableInProcessServerFixture timeout_fixture{
        29132,
        [](masodik::runtime::ServiceConfig& config) {
            config.serving_profile.timeout_seconds = 1;
            config.serving_profile.queue_policy.max_queue_wait_seconds = 31;
        },
        timeout_route_support};
    auto blocking_lease = timeout_route_support->request_scheduler->acquire(std::chrono::seconds{60});

    const auto timeout_response = masodik::tests::post_raw_http(
        timeout_fixture.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia"}],"stream":true,"max_tokens":1})",
        std::chrono::minutes{2});

    masodik::tests::require(timeout_response.status_code == 503, "A timeout-budget stream keresnek 503-at kell adnia.");
    masodik::tests::require(
        timeout_response.raw_headers.find("Content-Type: application/json") != std::string::npos,
        "A timeout-budget stream keres nem JSON hiba lett.");
    masodik::tests::require(
        timeout_response.body.find("\"code\":\"runtime_error\"") != std::string::npos,
        "A timeout-budget stream hiba nem runtime_error kodot adott.");
}

MASODIK_TEST(streaming_failure_modes_emit_terminal_sse_and_cleanup_after_generator_failure) {
    auto base_config = masodik::runtime::build_service_config(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        masodik::config::default_paths::default_model_root,
        masodik::tests::find_repo_root());
    auto route_support = masodik::tests::make_route_support(base_config);
    masodik::tests::ScopedEnvVar stream_failure_mode{"MASODIK_TEST_STREAM_FAILURE_MODE", "after_first_chunk"};

    masodik::tests::ConfigurableInProcessServerFixture fixture{29133, {}, route_support};

    const auto response = masodik::tests::post_raw_http(
        fixture.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Generalj tobb stream chunkot."}],"stream":true,"max_tokens":128})",
        std::chrono::minutes{2});

    masodik::tests::require(response.status_code == 200, "A generator-hibas stream keresnek is 200-as SSE valaszt kell adnia.");
    masodik::tests::require(
        response.raw_headers.find("Content-Type: text/event-stream") != std::string::npos,
        "A generator-hibas stream keres nem SSE headert adott.");
    masodik::tests::require(
        response.body.find("\"object\":\"chat.completion.chunk\"") != std::string::npos,
        "A generator-hibas stream valaszbol hianyzik a chunk payload.");
    masodik::tests::require(
        response.body.find("\"code\":\"runtime_error\"") != std::string::npos,
        "A generator-hibas stream nem kuldott terminalis runtime_error esemenyt.");
    masodik::tests::require(
        response.body.find("\"terminal\":true") != std::string::npos,
        "A generator-hibas stream nem terminalis SSE hibaval zart.");
    masodik::tests::require(
        response.body.find("data: [DONE]") != std::string::npos,
        "A generator-hibas stream nem zarta le a valaszfolyamot.");

    const auto queue_snapshot = route_support->queue_metrics->snapshot();
    masodik::tests::require(queue_snapshot.active_streams == 0U, "A generatorhiba utan nem maradhat aktiv stream.");
    masodik::tests::require(queue_snapshot.running_requests == 0U, "A generatorhiba utan nem maradhat aktiv request lease.");
    masodik::tests::require(queue_snapshot.total_stream_failures == 1U, "A generatorhiba pontosan egy stream failure-t kell jelentsen.");
}
