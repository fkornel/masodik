#include <chrono>
#include <cstdlib>
#include <iostream>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_BENCHMARK(benchmark_manual_split_handoff_latency) {
    const char* model_root = std::getenv("MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT");
    if (model_root == nullptr || std::string_view{model_root}.empty()) {
        std::cout << "manual_split_handoff_latency=SKIP model_root_missing\n";
        return 0;
    }

    masodik::benchmarks::LocalBenchmarkServer server{
        29139,
        masodik::benchmarks::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        model_root};

    masodik::http::HttpClient client;
    const auto pre_health = client.get(server.service_config().health_url, std::chrono::seconds{30});
    if (pre_health.status_code != 200 || pre_health.body.find("\"split_mode\":true") == std::string::npos) {
        std::cout << "manual_split_handoff_latency_health=FAIL preflight\n";
        return 1;
    }

    constexpr std::string_view request_body =
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Merj egy rovid split handoff latency valaszt."}],"stream":false,"max_tokens":48})";

    constexpr int iterations = 3;
    long long total_duration_ms = 0;
    for (int index = 0; index < iterations; ++index) {
        const auto started_at = std::chrono::steady_clock::now();
        const auto response = client.post_json(server.service_config().chat_url, request_body, std::chrono::minutes{5});
        const auto duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count();
        if (response.status_code != 200) {
            std::cout << "manual_split_handoff_latency_status=" << response.status_code << '\n';
            return 1;
        }
        if (response.body.find("Placeholder valasz") != std::string::npos) {
            std::cout << "manual_split_handoff_placeholder=true\n";
            return 1;
        }
        total_duration_ms += duration_ms;
    }
    const auto post_health = client.get(server.service_config().health_url, std::chrono::seconds{30});

    std::cout << "manual_split_handoff_iterations=" << iterations << '\n';
    std::cout << "manual_split_handoff_avg_ms=" << (total_duration_ms / iterations) << '\n';
    std::cout << "manual_split_handoff_placeholder=false\n";
    std::cout << "manual_split_handoff_health_status=" << post_health.status_code << '\n';
    std::cout << "manual_split_handoff_last_state="
              << (post_health.body.find("\"handoff_state\":\"consumed\"") != std::string::npos ? "consumed" : "unexpected")
              << '\n';
    return post_health.status_code == 200 ? 0 : 1;
}
