#include <chrono>
#include <iostream>

#include "benchmark_registry.hpp"

#include "benchmark_support.hpp"

MASODIK_BENCHMARK(benchmark_restart_stability) {
    long long total_duration_ms = 0;
    for (auto iteration = 0; iteration < 2; ++iteration) {
        const auto started_at = std::chrono::steady_clock::now();
        masodik::benchmarks::LocalBenchmarkServer server{
            static_cast<unsigned short>(29130 + iteration)};
        const auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at);
        total_duration_ms += duration.count();
    }

    std::cout << "restart_stability_total_ms=" << total_duration_ms << '\n';
    return 0;
}
