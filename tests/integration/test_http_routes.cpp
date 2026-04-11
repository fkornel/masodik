#include <chrono>

#include "in_process_server_fixture.hpp"
#include "test_registry.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_TEST(http_routes_serve_health_and_models_payloads) {
    auto& host = masodik::tests::shared_in_process_server();
    masodik::http::HttpClient client;

    const auto health_response = client.get(host.service_config().health_url, std::chrono::seconds{3});
    const auto models_response = client.get(host.service_config().models_url, std::chrono::seconds{3});

    masodik::tests::require(health_response.status_code == 200, "A health endpoint nem adott 200-as valaszt.");
    masodik::tests::require(health_response.body.find("\"state\":\"ready\"") != std::string::npos, "A health body nem vart.");
    masodik::tests::require(models_response.status_code == 200, "A models endpoint nem adott 200-as valaszt.");
    masodik::tests::require(
        models_response.body.find("\"id\":\"gpt-oss-20b-int4-ov\"") != std::string::npos,
        "A models payload nem tartalmazza a vart modellazonositot.");
}
