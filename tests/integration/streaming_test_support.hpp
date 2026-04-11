#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#pragma comment(lib, "winhttp.lib")

namespace masodik::tests {

struct RawHttpResponse {
    int status_code = 0;
    std::string raw_headers;
    std::string body;
};

inline std::wstring widen_utf8(const std::string_view value) {
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

inline std::string narrow_utf8(const std::wstring_view value) {
    if (value.empty()) {
        return {};
    }

    const auto required = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        throw std::runtime_error("A UTF-16 -> UTF-8 konverzio sikertelen.");
    }

    std::string result(required, '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr) <= 0) {
        throw std::runtime_error("A UTF-16 -> UTF-8 konverzio sikertelen.");
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

inline WinHttpHandle make_winhttp_handle(HINTERNET handle) { return WinHttpHandle(handle); }

inline RawHttpResponse post_raw_http(
    const std::string_view url,
    const std::string_view body,
    const std::chrono::milliseconds timeout,
    const std::string_view accept_header = "text/event-stream, application/json") {
    const auto wide_url = widen_utf8(url);

    URL_COMPONENTSW components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &components)) {
        throw std::runtime_error("A teszt URL nem elemezheto.");
    }

    const std::wstring host{components.lpszHostName, components.dwHostNameLength};
    std::wstring path{components.lpszUrlPath, components.dwUrlPathLength};
    if (components.dwExtraInfoLength > 0U) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    auto session = make_winhttp_handle(WinHttpOpen(L"masodik-tests/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                                   WINHTTP_NO_PROXY_BYPASS, 0));
    auto connection = make_winhttp_handle(WinHttpConnect(session.get(), host.c_str(), components.nPort, 0));
    const auto flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    auto request =
        make_winhttp_handle(WinHttpOpenRequest(connection.get(), L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!session || !connection || !request) {
        throw std::runtime_error("A raw WinHTTP teszt request nem hozhato letre.");
    }

    const auto timeout_ms = static_cast<int>(timeout.count());
    if (!WinHttpSetTimeouts(request.get(), timeout_ms, timeout_ms, timeout_ms, timeout_ms)) {
        throw std::runtime_error("A raw WinHTTP timeoutok nem allithatok be.");
    }

    const auto wide_accept = widen_utf8(accept_header);
    const std::wstring headers = L"Content-Type: application/json\r\nAccept: " + wide_accept + L"\r\n";
    if (!WinHttpSendRequest(
            request.get(),
            headers.c_str(),
            static_cast<DWORD>(headers.size()),
            const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()),
            static_cast<DWORD>(body.size()),
            0)) {
        throw std::runtime_error("A raw WinHTTP keres kuldese sikertelen.");
    }
    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        throw std::runtime_error("A raw WinHTTP valasz fogadasa sikertelen.");
    }

    DWORD status_code = 0;
    DWORD status_code_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             &status_code, &status_code_size, WINHTTP_NO_HEADER_INDEX)) {
        throw std::runtime_error("A raw WinHTTP status code nem olvashato.");
    }

    DWORD raw_headers_size = 0;
    WinHttpQueryHeaders(
        request.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &raw_headers_size,
        WINHTTP_NO_HEADER_INDEX);
    std::wstring raw_headers_buffer(raw_headers_size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(
            request.get(),
            WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX,
            raw_headers_buffer.data(),
            &raw_headers_size,
            WINHTTP_NO_HEADER_INDEX)) {
        throw std::runtime_error("A raw WinHTTP headerek nem olvashatok.");
    }

    std::string response_body;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available)) {
            throw std::runtime_error("A raw WinHTTP body merete nem kerdezheto le.");
        }
        if (available == 0U) {
            break;
        }

        std::string buffer(available, '\0');
        DWORD bytes_read = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), available, &bytes_read)) {
            throw std::runtime_error("A raw WinHTTP body nem olvashato.");
        }
        buffer.resize(bytes_read);
        response_body += buffer;
    }

    const auto raw_header_count = raw_headers_size / sizeof(wchar_t);
    const std::wstring_view raw_headers_wide{raw_headers_buffer.data(), raw_header_count > 0 ? raw_header_count - 1 : 0};
    return {.status_code = static_cast<int>(status_code), .raw_headers = narrow_utf8(raw_headers_wide), .body = std::move(response_body)};
}

class ScopedEnvVar {
public:
    ScopedEnvVar(std::string name, std::string value)
        : name_(std::move(name)) {
        if (const char* current = std::getenv(name_.c_str()); current != nullptr) {
            had_previous_ = true;
            previous_value_ = current;
        }
        _putenv_s(name_.c_str(), value.c_str());
    }

    ~ScopedEnvVar() {
        if (had_previous_) {
            _putenv_s(name_.c_str(), previous_value_.c_str());
        } else {
            _putenv_s(name_.c_str(), "");
        }
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

private:
    std::string name_;
    std::string previous_value_;
    bool had_previous_ = false;
};

}  // namespace masodik::tests
