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

MASODIK_BENCHMARK(benchmark_tool_call_first_delta) {
    masodik::benchmarks::LocalBenchmarkServer server{29129};

    const auto wide_host = widen(server.service_config().bind_host);
    const auto wide_path = widen(server.service_config().chat_endpoint_path);
    const auto body = std::string{
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Deritsd ki a nyitott PR-eket egy elerheto toollal."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis shell parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto","stream":true,"max_tokens":96})"};

    auto session = make_handle(WinHttpOpen(L"masodik-bench/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                           WINHTTP_NO_PROXY_BYPASS, 0));
    auto connect = make_handle(WinHttpConnect(session.get(), wide_host.c_str(), server.service_config().bind_port, 0));
    auto request = make_handle(WinHttpOpenRequest(connect.get(), L"POST", wide_path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
    if (!session || !connect || !request) {
        throw std::runtime_error("A tool-call benchmark WinHTTP handle-jei nem hozhatok letre.");
    }

    const auto timeout_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::minutes{5}).count());
    if (!WinHttpSetTimeouts(request.get(), timeout_ms, timeout_ms, timeout_ms, timeout_ms)) {
        throw std::runtime_error("A tool-call benchmark timeoutjai nem allithatok be.");
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
        throw std::runtime_error("A tool-call benchmark keres kuldese sikertelen.");
    }

    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        throw std::runtime_error("A tool-call benchmark valasz fogadasa sikertelen.");
    }

    std::string collected;
    while (collected.find("\"tool_calls\"") == std::string::npos) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available) || available == 0U) {
            throw std::runtime_error("A tool-call benchmark nem kapott tool-call delta adatot.");
        }

        std::string buffer(available, '\0');
        DWORD bytes_read = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), available, &bytes_read)) {
            throw std::runtime_error("A tool-call benchmark nem tudta beolvasni a streaming valaszt.");
        }
        buffer.resize(bytes_read);
        collected += buffer;
    }

    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at);
    std::cout << "tool_call_first_delta_ms=" << duration.count() << '\n';
    return 0;
}
