#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_BENCHMARK(benchmark_manual_split_stability_proof) {
    const char* model_root = std::getenv("MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT");
    if (model_root == nullptr || std::string_view{model_root}.empty()) {
        std::cout << "manual_split_stability_proof=SKIP model_root_missing\n";
        return 0;
    }

    masodik::benchmarks::LocalBenchmarkServer server{
        29141,
        masodik::benchmarks::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        model_root};

    masodik::http::HttpClient client;
    constexpr std::string_view request_body =
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Adj stabilitasi proof statuszt a ketstage-es split futasrol."}],"stream":false,"max_tokens":64})";

    constexpr int iterations = 3;
    long long total_duration_ms = 0;
    for (int index = 0; index < iterations; ++index) {
        const auto started_at = std::chrono::steady_clock::now();
        const auto response = client.post_json(server.service_config().chat_url, request_body, std::chrono::minutes{5});
        total_duration_ms +=
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count();
        if (response.status_code != 200 || response.body.find("Placeholder valasz") != std::string::npos) {
            std::cout << "manual_split_stability_proof_status=" << response.status_code << '\n';
            std::cout << "manual_split_stability_proof_placeholder="
                      << (response.body.find("Placeholder valasz") != std::string::npos ? "true" : "false") << '\n';
            return 1;
        }
    }

    const auto post_health = client.get(server.service_config().health_url, std::chrono::seconds{30});
    std::cout << "manual_split_stability_proof_iterations=" << iterations << '\n';
    std::cout << "manual_split_stability_proof_avg_ms=" << (total_duration_ms / iterations) << '\n';
    std::cout << "manual_split_stability_proof_compiled="
              << (post_health.body.find("\"request_path_mode\":\"compiled\"") != std::string::npos ? "true" : "false")
              << '\n';
    std::cout << "manual_split_stability_proof_stage_1_execution_devices="
              << (post_health.body.find("\"stage_1_execution_devices\":\"") != std::string::npos ? "present" : "missing")
              << '\n';
    std::cout << "manual_split_stability_proof_stage_2_execution_devices="
              << (post_health.body.find("\"stage_2_execution_devices\":\"") != std::string::npos ? "present" : "missing")
              << '\n';
    std::cout << "manual_split_stability_proof_last_split_request_id="
              << (post_health.body.find("\"last_split_request_id\":\"") != std::string::npos ? "present" : "missing")
              << '\n';
    return post_health.status_code == 200 ? 0 : 1;
}

