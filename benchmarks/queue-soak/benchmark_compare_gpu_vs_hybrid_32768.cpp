#include <chrono>
#include <iostream>
#include <string>

#include "benchmark_registry.hpp"
#include "benchmark_support.hpp"

#include "masodik/http/http_client.hpp"

// Ez a benchmark a cut23-tail manual split profil 32k kontextus-kezelési
// teljesítményét méri ismételt kérések sorozatával.
MASODIK_BENCHMARK(benchmark_compare_gpu_vs_hybrid_32768) {
    const auto repo_root = masodik::benchmarks::find_repo_root();
    const auto split_profile = repo_root / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json";

    bool startup_ok = false;
    long long startup_ms = 0;
    bool request_ok = false;
    long long request_ms = 0;
    int status_code = 0;
    std::string error;

    try {
        const auto startup_started = std::chrono::steady_clock::now();
        masodik::benchmarks::LocalBenchmarkServer server{29134, split_profile};
        startup_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startup_started).count();
        startup_ok = true;

        masodik::http::HttpClient client;
        const auto request_started = std::chrono::steady_clock::now();
        const auto response = client.post_json(
            server.service_config().chat_url,
            R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Adj rovid osszehasonlitast a VRAM es hybrid betoltesrol."}],"max_tokens":32})",
            std::chrono::minutes{10});
        request_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - request_started).count();
        status_code = response.status_code;
        request_ok = response.status_code == 200;
    } catch (const std::exception& ex) {
        error = ex.what();
    }

    std::cout << "split_startup_ok=" << (startup_ok ? "true" : "false") << '\n';
    std::cout << "split_startup_ms=" << startup_ms << '\n';
    std::cout << "split_request_ok=" << (request_ok ? "true" : "false") << '\n';
    std::cout << "split_request_ms=" << request_ms << '\n';
    std::cout << "split_status_code=" << status_code << '\n';
    if (!error.empty()) {
        std::cout << "split_error=" << error << '\n';
    }

    return 0;
}

