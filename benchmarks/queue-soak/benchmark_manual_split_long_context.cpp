#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

namespace {

std::string read_fixture_body(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error("A manual split long-context benchmark fixture nem nyithato meg: " + path.string());
    }
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

}

MASODIK_BENCHMARK(benchmark_manual_split_long_context) {
    const char* model_root = std::getenv("MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT");
    if (model_root == nullptr || std::string_view{model_root}.empty()) {
        std::cout << "manual_split_long_context=SKIP model_root_missing\n";
        return 0;
    }

    masodik::benchmarks::LocalBenchmarkServer server{
        29138,
        masodik::benchmarks::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        model_root};

    const auto fixture_path =
        masodik::benchmarks::find_repo_root() / "benchmarks" / "fixtures" / "manual-split-long-context-request.json";
    const auto request_body = read_fixture_body(fixture_path);

    masodik::http::HttpClient client;
    const auto pre_health = client.get(server.service_config().health_url, std::chrono::seconds{30});
    if (pre_health.status_code != 200 || pre_health.body.find("\"split_mode\":true") == std::string::npos) {
        std::cout << "manual_split_long_context_health=FAIL preflight\n";
        return 1;
    }

    const auto started_at = std::chrono::steady_clock::now();
    const auto response = client.post_json(server.service_config().chat_url, request_body, std::chrono::minutes{10});
    const auto duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count();
    const auto post_health = client.get(server.service_config().health_url, std::chrono::seconds{30});

    std::cout << "manual_split_long_context_status=" << response.status_code << '\n';
    std::cout << "manual_split_long_context_ms=" << duration_ms << '\n';
    std::cout << "manual_split_long_context_body_chars=" << response.body.size() << '\n';
    std::cout << "manual_split_long_context_placeholder="
              << (response.body.find("Placeholder valasz") != std::string::npos ? "true" : "false") << '\n';
    std::cout << "manual_split_long_context_health_status=" << post_health.status_code << '\n';
    std::cout << "manual_split_long_context_handoff_consumed="
              << (post_health.body.find("\"handoff_state\":\"consumed\"") != std::string::npos ? "true" : "false") << '\n';
    return response.status_code == 200 &&
                   post_health.status_code == 200 &&
                   response.body.find("Placeholder valasz") == std::string::npos
               ? 0
               : 1;
}
