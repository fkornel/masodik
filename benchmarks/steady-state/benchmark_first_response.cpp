#include <chrono>
#include <iostream>

#include "benchmark_registry.hpp"

#include "benchmark_support.hpp"
#include "masodik/http/http_client.hpp"

MASODIK_BENCHMARK(benchmark_first_response) {
    masodik::benchmarks::LocalBenchmarkServer server{29127};

    const auto fixture_body = std::string{R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Adj egy rovid valaszt."}],"stream":false,"max_tokens":64})"};
    masodik::http::HttpClient http_client;

    const auto started_at = std::chrono::steady_clock::now();
    const auto response = http_client.post_json(server.service_config().chat_url, fixture_body, std::chrono::minutes{5});
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at);

    std::cout << "first_response_ms=" << duration.count() << '\n';
    return response.status_code == 200 ? 0 : 1;
}
