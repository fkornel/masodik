#include <chrono>
#include <future>
#include <string>
#include <thread>

#include "openai_route_test_support.hpp"
#include "streaming_test_support.hpp"
#include "test_registry.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_TEST(mixed_streaming_requests_surface_queue_health_and_preserve_responses) {
    auto base_config = masodik::runtime::build_service_config(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        masodik::config::default_paths::default_model_root,
        masodik::tests::find_repo_root());
    auto route_support = masodik::tests::make_route_support(base_config);
    masodik::tests::ScopedEnvVar stream_delay{"MASODIK_TEST_STREAM_CHUNK_DELAY_MILLIS", "150"};

    masodik::tests::ConfigurableInProcessServerFixture fixture{29134, {}, route_support};
    masodik::http::HttpClient client;

    auto streaming_request = std::async(std::launch::async, [&fixture] {
        return masodik::tests::post_raw_http(
            fixture.service_config().chat_url,
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Indits elnyujtott streamet mixed-load teszthez."}],"stream":true,"max_tokens":256})",
            std::chrono::minutes{5});
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{250});

    auto json_request = std::async(std::launch::async, [&fixture, &client] {
        return client.post_json(
            fixture.service_config().chat_url,
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Adj normal JSON valaszt mixed-load mellett."}],"stream":false,"max_tokens":64})",
            std::chrono::minutes{5});
    });

    bool observed_mixed_queue = false;
    bool observed_degraded_health = false;
    bool observed_health_stream_fields = false;
    std::string health_body_during_load;
    for (int attempt = 0; attempt < 40; ++attempt) {
        const auto queue_snapshot_during_load = route_support->queue_metrics->snapshot();
        const auto health = client.get(fixture.service_config().health_url, std::chrono::seconds{15});
        health_body_during_load = health.body;
        observed_mixed_queue = observed_mixed_queue ||
                               (queue_snapshot_during_load.active_streams >= 1U && queue_snapshot_during_load.queued_requests >= 1U);
        observed_degraded_health = observed_degraded_health ||
                                   (health.body.find("\"state\":\"degraded\"") != std::string::npos);
        observed_health_stream_fields = observed_health_stream_fields ||
                                        (health.body.find("\"active_streams\":") != std::string::npos &&
                                         health.body.find("\"peak_active_streams\":") != std::string::npos &&
                                         health.body.find("\"total_stream_failures\":") != std::string::npos &&
                                         health.body.find("\"last_disconnect_error\":") != std::string::npos);
        if (observed_mixed_queue && observed_degraded_health && observed_health_stream_fields) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{150});
    }

    const auto streaming_response = streaming_request.get();
    const auto json_response = json_request.get();
    const auto final_health = client.get(fixture.service_config().health_url, std::chrono::seconds{15});
    const auto queue_snapshot = route_support->queue_metrics->snapshot();

    masodik::tests::require(observed_mixed_queue, "Mixed-load alatt a scheduler nem mutatta a vart stream+queue allapotot.");
    masodik::tests::require(observed_degraded_health, "Mixed-load alatt a health nem mutatta a vart degraded allapotot.");
    masodik::tests::require(observed_health_stream_fields, "Mixed-load alatt a health nem vitte ki a vart stream diagnosztikai mezoket.");
    masodik::tests::require(
        health_body_during_load.find("\"peak_active_streams\":") != std::string::npos,
        "Mixed-load health alatt nem latszott a peak_active_streams mezo.");
    masodik::tests::require(streaming_response.status_code == 200, "A mixed-load streaming keres nem adott 200-at.");
    masodik::tests::require(
        streaming_response.body.find("data: [DONE]") != std::string::npos,
        "A mixed-load streaming valasz nem zarta le a streamet.");
    masodik::tests::require(json_response.status_code == 200, "A mixed-load JSON keres nem adott 200-at.");
    masodik::tests::require(
        json_response.body.find("\"object\":\"chat.completion\"") != std::string::npos,
        "A mixed-load JSON valasz nem a vart OpenAI payload.");
    masodik::tests::require(
        final_health.body.find("\"state\":\"ready\"") != std::string::npos,
        "Mixed-load utan a health nem allt vissza ready allapotra.");
    masodik::tests::require(
        final_health.body.find("\"queue_depth\":0") != std::string::npos,
        "Mixed-load utan a health queue_depth mezonel nem nullazodott.");
    masodik::tests::require(queue_snapshot.total_stream_failures == 0U, "Mixed-load alatt nem vart stream failure tortent.");
}
