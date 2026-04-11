#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_BENCHMARK(benchmark_streaming_mixed_load) {
    _putenv_s("MASODIK_TEST_STREAM_CHUNK_DELAY_MILLIS", "100");
    masodik::benchmarks::LocalBenchmarkServer server{29133};
    masodik::http::HttpClient client;

    std::atomic<int> successful_streams = 0;
    std::atomic<int> successful_json_requests = 0;

    std::thread streaming_worker([&] {
        const auto response = client.post_json(
            server.service_config().chat_url,
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Mixed-load streaming benchmark."}],"stream":true,"max_tokens":256})",
            std::chrono::minutes{10});
        if (response.status_code == 200 && response.body.find("data: [DONE]") != std::string::npos) {
            ++successful_streams;
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{250});

    std::vector<std::thread> json_workers;
    for (int index = 0; index < 3; ++index) {
        json_workers.emplace_back([&] {
            const auto response = client.post_json(
                server.service_config().chat_url,
                R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Mixed-load JSON benchmark."}],"stream":false,"max_tokens":64})",
                std::chrono::minutes{10});
            if (response.status_code == 200) {
                ++successful_json_requests;
            }
        });
    }

    for (auto& worker : json_workers) {
        worker.join();
    }
    streaming_worker.join();

    const auto health = client.get(server.service_config().health_url, std::chrono::seconds{30});
    std::cout << "mixed_load_stream_success=" << successful_streams.load() << '\n';
    std::cout << "mixed_load_json_success=" << successful_json_requests.load() << '\n';
    std::cout << "mixed_load_health=" << health.body << '\n';
    _putenv_s("MASODIK_TEST_STREAM_CHUNK_DELAY_MILLIS", "");
    return 0;
}
