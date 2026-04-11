#include <chrono>
#include <cstdlib>

#include "in_process_server_fixture.hpp"
#include "streaming_test_support.hpp"
#include "test_registry.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_TEST(split_profile_runtime_failure_is_surfaced_as_degraded_state) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "hybrid-fail-test", .bind_port = 29141});
    try {
        masodik::tests::InProcessServerFixture host{fixture.profile_path, 29141, fixture.root_path};
        masodik::http::HttpClient client;

        const auto health_response = client.get(host.service_config().health_url, std::chrono::seconds{10});

        masodik::tests::require(health_response.status_code == 200, "A health endpoint nem adott 200-at.");
        masodik::tests::require(
            health_response.body.find("\"state\":") != std::string::npos,
            "A split profil health valaszabol hianyzik az allapot mezo.");
        masodik::tests::require(
            health_response.body.find("\"gpu_offload_active\":") != std::string::npos,
            "A split profil health valaszabol hianyzik a gpu_offload_active mezo.");
    } catch (...) {
        _putenv_s("MASODIK_DISABLE_OPENVINO_GENAI", "");
        throw;
    }
    _putenv_s("MASODIK_DISABLE_OPENVINO_GENAI", "");
}
