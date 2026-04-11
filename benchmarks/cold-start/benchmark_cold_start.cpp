#include <chrono>
#include <iostream>

#include "benchmark_registry.hpp"

#include "benchmark_support.hpp"

MASODIK_BENCHMARK(benchmark_cold_start) {
    const auto started_at = std::chrono::steady_clock::now();
    masodik::benchmarks::LocalBenchmarkServer server{29126};
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at);

    std::cout << "cold_start_ms=" << duration.count() << '\n';
    return 0;
}
