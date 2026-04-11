#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winhttp.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include "benchmark_registry.hpp"

#include "benchmark_support.hpp"

#pragma comment(lib, "winhttp.lib")

namespace {

std::wstring widen(const std::string_view value) {
    if (value.empty()) {
        return {};
    }

    const auto required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        throw std::runtime_error("A UTF-8 -> UTF-16 konverzio sikertelen.");
    }

    std::wstring result(required, L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required) <= 0) {
        throw std::runtime_error("A UTF-8 -> UTF-16 konverzio sikertelen.");
    }
    return result;
}

struct WinHttpCloser {
    void operator()(HINTERNET handle) const noexcept {
        if (handle != nullptr) {
            WinHttpCloseHandle(handle);
        }
    }
};

using WinHttpHandle = std::unique_ptr<std::remove_pointer_t<HINTERNET>, WinHttpCloser>;

WinHttpHandle make_handle(HINTERNET handle) { return WinHttpHandle(handle); }

}  // namespace

MASODIK_BENCHMARK(benchmark_stream_first_token) {
    masodik::benchmarks::LocalBenchmarkServer server{29128};

    const auto wide_host = widen(server.service_config().bind_host);
    const auto wide_path = widen(server.service_config().chat_endpoint_path);
    const auto body = std::string{
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Adj rovid stream valaszt."}],"stream":true,"max_tokens":64})"};

    auto session = make_handle(WinHttpOpen(L"masodik-bench/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                           WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        throw std::runtime_error("A WinHTTP session nem hozhato letre.");
    }

    auto connect = make_handle(WinHttpConnect(session.get(), wide_host.c_str(), server.service_config().bind_port, 0));
    if (!connect) {
        throw std::runtime_error("A WinHTTP kapcsolat nem hozhato letre.");
    }

    auto request = make_handle(WinHttpOpenRequest(connect.get(), L"POST", wide_path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
    if (!request) {
        throw std::runtime_error("A WinHTTP request nem hozhato letre.");
    }

    const auto timeout_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::minutes{5}).count());
    if (!WinHttpSetTimeouts(request.get(), timeout_ms, timeout_ms, timeout_ms, timeout_ms)) {
        throw std::runtime_error("A benchmark timeoutjai nem allithatok be.");
    }

    const std::wstring headers = L"Content-Type: application/json\r\nAccept: text/event-stream\r\n";
    const auto started_at = std::chrono::steady_clock::now();
    if (!WinHttpSendRequest(
            request.get(),
            headers.c_str(),
            static_cast<DWORD>(headers.size()),
            const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()),
            static_cast<DWORD>(body.size()),
            0)) {
        throw std::runtime_error("A streaming benchmark keres kuldese sikertelen.");
    }

    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        throw std::runtime_error("A streaming benchmark valasz fogadasa sikertelen.");
    }

    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request.get(), &available) || available == 0U) {
        throw std::runtime_error("A streaming benchmark nem kapott elso adatblokkot.");
    }

    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at);
    std::cout << "stream_first_token_ms=" << duration.count() << '\n';
    return 0;
}
