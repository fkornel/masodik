#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_BENCHMARK(benchmark_long_queue) {
    masodik::benchmarks::LocalBenchmarkServer server{29132};
    masodik::http::HttpClient client;
    std::atomic<int> successful_requests = 0;

    std::vector<std::thread> workers;
    for (int index = 0; index < 6; ++index) {
        workers.emplace_back([&] {
            const auto response = client.post_json(
                server.service_config().chat_url,
                R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Hosszu queue benchmark."}],"max_tokens":16})",
                std::chrono::minutes{10});
            if (response.status_code == 200) {
                ++successful_requests;
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    const auto health = client.get(server.service_config().health_url, std::chrono::seconds{30});
    std::cout << "long_queue_success=" << successful_requests.load() << '\n';
    std::cout << "long_queue_health=" << health.body << '\n';
    return 0;
}
