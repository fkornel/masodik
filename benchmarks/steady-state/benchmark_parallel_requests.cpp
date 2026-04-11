#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_BENCHMARK(benchmark_parallel_requests) {
    masodik::benchmarks::LocalBenchmarkServer server{29131};
    masodik::http::HttpClient client;
    std::atomic<int> successful_requests = 0;
    constexpr int worker_count = 3;

    const auto started_at = std::chrono::steady_clock::now();
    std::vector<std::thread> workers;
    for (int index = 0; index < worker_count; ++index) {
        workers.emplace_back([&] {
            const auto response = client.post_json(
                server.service_config().chat_url,
                R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Parhuzamos benchmark fut."}],"max_tokens":16})",
                std::chrono::minutes{10});
            if (response.status_code == 200) {
                ++successful_requests;
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    const auto duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count();
    std::cout << "parallel_requests_success=" << successful_requests.load() << '\n';
    std::cout << "parallel_requests_expected_audit_records=" << worker_count << '\n';
    std::cout << "parallel_requests_total_ms=" << duration_ms << '\n';
    return 0;
}
