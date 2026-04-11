// T050 [US3] — Split diagnostics benchmark: meri a /health lekerdezesek es a
// split-specifikus mezok kiertekelesebõl adodo overhead-et steady-state allapotban.

#include <chrono>
#include <cstdlib>
#include <iostream>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_BENCHMARK(benchmark_manual_split_diagnostics_overhead) {
    const char* model_root = std::getenv("MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT");
    if (model_root == nullptr || std::string_view{model_root}.empty()) {
        std::cout << "manual_split_diagnostics_overhead=SKIP model_root_missing\n";
        return 0;
    }

    masodik::benchmarks::LocalBenchmarkServer server{
        29149,
        masodik::benchmarks::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        model_root};

    masodik::http::HttpClient client;

    // Elofeltetelek ellenorzese
    const auto pre_health = client.get(server.service_config().health_url, std::chrono::seconds{30});
    if (pre_health.status_code != 200 || pre_health.body.find("\"split_mode\":true") == std::string::npos) {
        std::cout << "manual_split_diagnostics_health=FAIL preflight\n";
        return 1;
    }

    // Merjuk a /health endpoint valaszidejet split modban
    constexpr int iterations = 50;
    long long total_health_us = 0;
    int successful_calls = 0;

    for (int index = 0; index < iterations; ++index) {
        const auto started_at = std::chrono::steady_clock::now();
        const auto response = client.get(server.service_config().health_url, std::chrono::seconds{10});
        const auto duration_us =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started_at).count();

        if (response.status_code == 200) {
            total_health_us += duration_us;
            ++successful_calls;
        }
    }

    if (successful_calls == 0) {
        std::cout << "manual_split_diagnostics_overhead=FAIL no_successful_calls\n";
        return 1;
    }

    const auto avg_health_us = total_health_us / successful_calls;

    // Ellenorizzuk, hogy a split mezok jelen vannak a valaszban
    const auto final_health = client.get(server.service_config().health_url, std::chrono::seconds{10});
    const bool has_split_fields =
        final_health.body.find("\"stage_1_state\"") != std::string::npos &&
        final_health.body.find("\"stage_2_state\"") != std::string::npos &&
        final_health.body.find("\"handoff_state\"") != std::string::npos &&
        final_health.body.find("\"memory_summary\"") != std::string::npos &&
        final_health.body.find("\"last_failure_scope\"") != std::string::npos &&
        final_health.body.find("\"last_failure_summary\"") != std::string::npos;

    std::cout << "manual_split_diagnostics_iterations=" << iterations << '\n';
    std::cout << "manual_split_diagnostics_successful=" << successful_calls << '\n';
    std::cout << "manual_split_diagnostics_avg_health_us=" << avg_health_us << '\n';
    std::cout << "manual_split_diagnostics_all_split_fields=" << (has_split_fields ? "true" : "false") << '\n';
    return has_split_fields ? 0 : 1;
}
