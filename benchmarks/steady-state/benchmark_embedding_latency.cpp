#include <chrono>
#include <iostream>
#include <string>

#include "benchmark_registry.hpp"

#include "benchmark_support.hpp"
#include "masodik/http/http_client.hpp"

// NPU embedding latency benchmark: egyedi es batch keresek meresi ideje.
// Az NPU elso hivasa lassabb lehet (warmup), ezert elobb egy probe kerest kuldunk.

MASODIK_BENCHMARK(benchmark_embedding_single_latency) {
    masodik::benchmarks::LocalBenchmarkServer server{29128};
    masodik::http::HttpClient http_client;
    const auto base_url = "http://" + server.service_config().bind_host + ":" +
                          std::to_string(server.service_config().bind_port);
    const auto url = base_url + "/v1/embeddings";

    // Warmup keres
    http_client.post_json(url,
        R"({"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":"warmup"})",
        std::chrono::minutes{2});

    // Mert keres: egyedi embedding
    const auto started_at = std::chrono::steady_clock::now();
    const auto response = http_client.post_json(url,
        R"({"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":"Budapest a fovarosa Magyarorszagnak."})",
        std::chrono::minutes{2});
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);

    std::cout << "embedding_single_latency_ms=" << duration.count() << '\n';
    std::cout << "embedding_single_status=" << response.status_code << '\n';
    return response.status_code == 200 ? 0 : 1;
}

MASODIK_BENCHMARK(benchmark_embedding_batch_latency) {
    masodik::benchmarks::LocalBenchmarkServer server{29129};
    masodik::http::HttpClient http_client;
    const auto base_url = "http://" + server.service_config().bind_host + ":" +
                          std::to_string(server.service_config().bind_port);
    const auto url = base_url + "/v1/embeddings";

    // Warmup
    http_client.post_json(url,
        R"({"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":"warmup"})",
        std::chrono::minutes{2});

    // 10 szoveges batch meres
    const std::string batch_body = R"({"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":["Budapest","Debrecen","Szeged","Miskolc","Pecs","Gyor","Nyiregyhaza","Kecskemet","Szekesfehervar","Eger"]})";

    const auto started_at = std::chrono::steady_clock::now();
    const auto response = http_client.post_json(url, batch_body, std::chrono::minutes{2});
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);

    std::cout << "embedding_batch10_latency_ms=" << duration.count() << '\n';
    std::cout << "embedding_batch10_status=" << response.status_code << '\n';
    return response.status_code == 200 ? 0 : 1;
}
