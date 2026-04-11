#include <chrono>
#include <cstdlib>
#include <iostream>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"
#include "masodik/split/split_artifact_builder.hpp"

MASODIK_BENCHMARK(benchmark_manual_split_generated_artifact) {
    const char* source_model_root = std::getenv("MASODIK_MANUAL_SPLIT_SOURCE_MODEL_ROOT");
    if (source_model_root == nullptr || std::string_view{source_model_root}.empty()) {
        std::cout << "manual_split_generated_artifact=SKIP source_model_root_missing\n";
        return 0;
    }

    const auto repo_root = masodik::benchmarks::find_repo_root();
    const auto output_root = repo_root / "build" / "generated-manual-split-benchmark";

    const auto generation_started_at = std::chrono::steady_clock::now();
    const auto artifact = masodik::split::build_split_artifact({
        .source_model_root = source_model_root,
        .output_root = output_root,
    });
    const auto generation_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - generation_started_at);

    const auto startup_started_at = std::chrono::steady_clock::now();
    masodik::benchmarks::LocalBenchmarkServer server{
        29138,
        repo_root / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        artifact.artifact_root};
    const auto startup_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startup_started_at);

    masodik::http::HttpClient client;
    const auto health = client.get(server.service_config().health_url, std::chrono::seconds{30});
    const bool split_ready =
        health.status_code == 200 &&
        health.body.find("\"split_mode\":true") != std::string::npos &&
        health.body.find("\"stage_1_state\":\"ready\"") != std::string::npos &&
        health.body.find("\"stage_2_state\":\"ready\"") != std::string::npos;

    std::cout << "manual_split_generated_artifact_root=" << artifact.artifact_root.string() << '\n';
    std::cout << "manual_split_generated_artifact_ms=" << generation_duration.count() << '\n';
    std::cout << "manual_split_generated_artifact_startup_ms=" << startup_duration.count() << '\n';
    std::cout << "manual_split_generated_artifact_split_ready=" << (split_ready ? "true" : "false") << '\n';
    return split_ready ? 0 : 1;
}
