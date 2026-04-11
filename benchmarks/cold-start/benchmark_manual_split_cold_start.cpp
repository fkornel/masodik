#include <chrono>
#include <cstdlib>
#include <iostream>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_BENCHMARK(benchmark_manual_split_cold_start) {
    const char* model_root = std::getenv("MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT");
    if (model_root == nullptr || std::string_view{model_root}.empty()) {
        std::cout << "manual_split_cold_start=SKIP model_root_missing\n";
        return 0;
    }

    const auto started_at = std::chrono::steady_clock::now();
    masodik::benchmarks::LocalBenchmarkServer server{
        29137,
        masodik::benchmarks::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        model_root};
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at);
    masodik::http::HttpClient client;
    const auto health = client.get(server.service_config().health_url, std::chrono::seconds{30});
    const bool split_ready =
        health.status_code == 200 &&
        health.body.find("\"split_mode\":true") != std::string::npos &&
        health.body.find("\"stage_1_state\":\"ready\"") != std::string::npos &&
        health.body.find("\"stage_2_state\":\"ready\"") != std::string::npos &&
        health.body.find("\"fallback_detected\":false") != std::string::npos;
    std::cout << "manual_split_cold_start_ms=" << duration.count() << '\n';
    std::cout << "manual_split_profile=" << server.service_config().active_profile << '\n';
    std::cout << "manual_split_cold_start_split_ready=" << (split_ready ? "true" : "false") << '\n';
    return split_ready ? 0 : 1;
}
