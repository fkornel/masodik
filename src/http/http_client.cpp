#include "masodik/http/http_client.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

#pragma comment(lib, "winhttp.lib")

namespace masodik::http {
namespace {

using WinHttpHandle = std::unique_ptr<std::remove_pointer_t<HINTERNET>, decltype(&WinHttpCloseHandle)>;

std::wstring widen(const std::string_view value) {
    if (value.empty()) {
        return {};
    }

    const auto required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        throw std::runtime_error("A UTF-8 -> UTF-16 konverzio sikertelen.");
    }

    std::wstring result(required, L'\0');
    const auto converted =
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required);
    if (converted <= 0) {
        throw std::runtime_error("A UTF-8 -> UTF-16 konverzio sikertelen.");
    }
    return result;
}

WinHttpHandle make_handle(HINTERNET handle) {
    return WinHttpHandle(handle, WinHttpCloseHandle);
}

const wchar_t* to_winhttp_method(const HttpMethod method) {
    switch (method) {
    case HttpMethod::get:
        return L"GET";
    case HttpMethod::post:
        return L"POST";
    }
    return L"GET";
}

}  // namespace

HttpResponse HttpClient::perform(const HttpRequest& request_data, const std::chrono::milliseconds timeout) const {
    const auto wide_url = widen(request_data.url);

    URL_COMPONENTSW components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &components)) {
        throw std::runtime_error("A health URL nem elemezheto.");
    }

    const std::wstring host{components.lpszHostName, components.dwHostNameLength};
    std::wstring path{components.lpszUrlPath, components.dwUrlPathLength};
    if (components.dwExtraInfoLength > 0U) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    auto session = make_handle(WinHttpOpen(L"masodik/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                           WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        throw std::runtime_error("A WinHTTP session nem hozhato letre.");
    }

    auto connect = make_handle(WinHttpConnect(session.get(), host.c_str(), components.nPort, 0));
    if (!connect) {
        throw std::runtime_error("A WinHTTP kapcsolat nem hozhato letre.");
    }

    const auto flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    auto request = make_handle(WinHttpOpenRequest(connect.get(), to_winhttp_method(request_data.method), path.c_str(),
                                                  nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) {
        throw std::runtime_error("A WinHTTP request nem hozhato letre.");
    }

    const auto timeout_ms = static_cast<int>(timeout.count());
    if (!WinHttpSetTimeouts(request.get(), timeout_ms, timeout_ms, timeout_ms, timeout_ms)) {
        throw std::runtime_error("A WinHTTP timeoutok nem allithatok be.");
    }

    const auto wide_content_type = widen(request_data.content_type);
    std::wstring headers;
    if (request_data.method == HttpMethod::post) {
        headers = L"Content-Type: " + wide_content_type + L"\r\n";
    }

    auto* body_pointer = request_data.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request_data.body.data());
    const auto body_size = static_cast<DWORD>(request_data.body.size());
    if (!WinHttpSendRequest(request.get(), headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                            headers.empty() ? 0UL : static_cast<DWORD>(headers.size()), body_pointer, body_size, body_size,
                            0)) {
        throw std::runtime_error("A HTTP keres kuldese sikertelen.");
    }
    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        throw std::runtime_error("A HTTP valasz fogadasa sikertelen.");
    }

    DWORD status_code = 0;
    DWORD status_code_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_code_size, WINHTTP_NO_HEADER_INDEX)) {
        throw std::runtime_error("A HTTP status code nem olvashato.");
    }

    std::string content_type;
    DWORD content_type_size = 0;
    WinHttpQueryHeaders(
        request.get(),
        WINHTTP_QUERY_CONTENT_TYPE,
        WINHTTP_HEADER_NAME_BY_INDEX,
        WINHTTP_NO_OUTPUT_BUFFER,
        &content_type_size,
        WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && content_type_size > sizeof(wchar_t)) {
        std::wstring content_type_buffer(content_type_size / sizeof(wchar_t), L'\0');
        if (WinHttpQueryHeaders(
                request.get(),
                WINHTTP_QUERY_CONTENT_TYPE,
                WINHTTP_HEADER_NAME_BY_INDEX,
                content_type_buffer.data(),
                &content_type_size,
                WINHTTP_NO_HEADER_INDEX)) {
            const auto wide_size = content_type_size / sizeof(wchar_t);
            std::wstring_view wide_value{content_type_buffer.data(), wide_size > 0 ? wide_size - 1 : 0};
            const auto required = WideCharToMultiByte(
                CP_UTF8,
                0,
                wide_value.data(),
                static_cast<int>(wide_value.size()),
                nullptr,
                0,
                nullptr,
                nullptr);
            if (required > 0) {
                content_type.resize(required);
                const auto converted = WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    wide_value.data(),
                    static_cast<int>(wide_value.size()),
                    content_type.data(),
                    required,
                    nullptr,
                    nullptr);
                if (converted <= 0) {
                    content_type.clear();
                }
            }
        }
    }

    std::string body;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available)) {
            throw std::runtime_error("A HTTP body merete nem kerdezheto le.");
        }
        if (available == 0U) {
            break;
        }

        std::string buffer(available, '\0');
        DWORD bytes_read = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), available, &bytes_read)) {
            throw std::runtime_error("A HTTP body nem olvashato.");
        }
        buffer.resize(bytes_read);
        body += buffer;
    }

    return {.status_code = static_cast<int>(status_code), .content_type = std::move(content_type), .body = std::move(body)};
}

HttpResponse HttpClient::get(const std::string_view url, const std::chrono::milliseconds timeout) const {
    return perform(HttpRequest{.method = HttpMethod::get, .url = std::string{url}}, timeout);
}

HttpResponse HttpClient::post_json(
    const std::string_view url,
    const std::string_view body,
    const std::chrono::milliseconds timeout) const {
    return perform(
        HttpRequest{
            .method = HttpMethod::post,
            .url = std::string{url},
            .content_type = "application/json",
            .body = std::string{body}},
        timeout);
}

}  // namespace masodik::http
