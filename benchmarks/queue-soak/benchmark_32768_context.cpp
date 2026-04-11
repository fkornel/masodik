#include <chrono>
#include <iostream>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

MASODIK_BENCHMARK(benchmark_32768_context) {
    masodik::benchmarks::LocalBenchmarkServer server{
        29133,
        masodik::benchmarks::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json"};
    masodik::http::HttpClient client;

    const auto started_at = std::chrono::steady_clock::now();
    const auto response = client.post_json(
        server.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"32768 context stabilitasi benchmark."}],"max_tokens":32})",
        std::chrono::minutes{10});
    const auto duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count();

    std::cout << "context_32768_status=" << response.status_code << '\n';
    std::cout << "context_32768_ms=" << duration_ms << '\n';
    return 0;
}
