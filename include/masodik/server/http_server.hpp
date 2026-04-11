#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace masodik::server {

class HttpStreamWriter;

enum class HttpMethod {
    get,
    post
};

struct HttpRequest {
    HttpMethod method = HttpMethod::get;
    std::string raw_target;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code = 200;
    std::string content_type = "application/json";
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
    bool chunked_transfer = false;
    std::function<void(HttpStreamWriter&)> stream_writer;
};

struct HttpServerConfig {
    std::string bind_host = "127.0.0.1";
    std::uint16_t bind_port = 0;
};

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpStreamWriter {
public:
    using WriteCallback = std::function<bool(std::string_view)>;
    using FinishCallback = std::function<bool()>;
    using ConnectedCallback = std::function<bool()>;
    using ErrorCallback = std::function<int()>;

    HttpStreamWriter(
        WriteCallback write_callback,
        WriteCallback write_chunk_callback,
        FinishCallback finish_callback,
        ConnectedCallback connected_callback,
        ErrorCallback error_callback);

    [[nodiscard]] bool write(std::string_view payload);
    [[nodiscard]] bool write_chunk(std::string_view payload);
    [[nodiscard]] bool finish();
    [[nodiscard]] bool client_connected() const;
    [[nodiscard]] int last_socket_error() const;

private:
    WriteCallback write_callback_;
    WriteCallback write_chunk_callback_;
    FinishCallback finish_callback_;
    ConnectedCallback connected_callback_;
    ErrorCallback error_callback_;
    bool finished_ = false;
};

class HttpServer {
public:
    HttpServer() = default;
    ~HttpServer();

    void register_route(HttpMethod method, std::string path, RouteHandler handler);
    void clear_routes();
    void start(const HttpServerConfig& config);
    void stop() noexcept;

    [[nodiscard]] bool is_running() const noexcept;

private:
    struct Route {
        HttpMethod method = HttpMethod::get;
        std::string path;
        RouteHandler handler;
    };

    void serve_loop();
    HttpResponse dispatch(const HttpRequest& request) const;

    std::vector<Route> routes_;
    std::atomic<bool> running_{false};
    HttpServerConfig config_{};
    std::thread worker_thread_;
    std::intptr_t listener_socket_ = -1;
};

}  // namespace masodik::server
