#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace masodik::http {

enum class HttpMethod {
    get,
    post
};

struct HttpRequest {
    HttpMethod method = HttpMethod::get;
    std::string url;
    std::string content_type = "application/json";
    std::string body;
};

struct HttpResponse {
    int status_code = 0;
    std::string content_type;
    std::string body;
};

class HttpClient {
public:
    HttpResponse perform(const HttpRequest& request, std::chrono::milliseconds timeout) const;
    HttpResponse get(std::string_view url, std::chrono::milliseconds timeout) const;
    HttpResponse post_json(std::string_view url, std::string_view body, std::chrono::milliseconds timeout) const;
};

}  // namespace masodik::http
