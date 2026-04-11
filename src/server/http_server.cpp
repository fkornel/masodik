#include "masodik/server/http_server.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>

#pragma comment(lib, "Ws2_32.lib")

namespace masodik::server {
namespace {

SOCKET& as_socket(std::intptr_t& socket_storage) {
    return reinterpret_cast<SOCKET&>(socket_storage);
}

std::string trim_copy(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || std::isspace(static_cast<unsigned char>(value.back())))) {
        value.pop_back();
    }
    std::size_t index = 0;
    while (index < value.size() && std::isspace(static_cast<unsigned char>(value[index]))) {
        ++index;
    }
    return value.substr(index);
}

std::string lowercase_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

HttpMethod parse_method(const std::string_view method) {
    if (method == "GET") {
        return HttpMethod::get;
    }
    if (method == "POST") {
        return HttpMethod::post;
    }
    throw std::invalid_argument("Nem tamogatott HTTP metodus.");
}

std::string reason_phrase(const int status_code) {
    switch (status_code) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 500:
        return "Internal Server Error";
    case 503:
        return "Service Unavailable";
    default:
        return "OK";
    }
}

bool send_all(const SOCKET socket_handle, const std::string_view payload) {
    std::size_t sent_total = 0;
    while (sent_total < payload.size()) {
        const auto sent = send(
            socket_handle,
            payload.data() + sent_total,
            static_cast<int>(payload.size() - sent_total),
            0);
        if (sent == SOCKET_ERROR) {
            return false;
        }
        sent_total += static_cast<std::size_t>(sent);
    }
    return true;
}

bool send_all(const SOCKET socket_handle, const std::string_view payload, int& last_socket_error) {
    if (send_all(socket_handle, payload)) {
        last_socket_error = 0;
        return true;
    }
    last_socket_error = WSAGetLastError();
    return false;
}

bool send_chunk_frame(const SOCKET socket_handle, const std::string_view payload, int& last_socket_error) {
    std::ostringstream stream;
    stream << std::hex << payload.size() << "\r\n";
    stream << payload << "\r\n";
    return send_all(socket_handle, stream.str(), last_socket_error);
}

bool send_chunk_terminator(const SOCKET socket_handle, int& last_socket_error) {
    return send_all(socket_handle, "0\r\n\r\n", last_socket_error);
}

HttpRequest read_request(const SOCKET socket_handle) {
    std::string buffer;
    buffer.reserve(4096);
    std::array<char, 2048> chunk{};

    std::size_t header_end = std::string::npos;
    while ((header_end = buffer.find("\r\n\r\n")) == std::string::npos) {
        const auto received = recv(socket_handle, chunk.data(), static_cast<int>(chunk.size()), 0);
        if (received <= 0) {
            throw std::runtime_error("A HTTP keres nem olvashato.");
        }
        buffer.append(chunk.data(), static_cast<std::size_t>(received));
        if (buffer.size() > 1024 * 1024) {
            throw std::runtime_error("A HTTP header tul nagy.");
        }
    }

    const auto header_block = buffer.substr(0, header_end);
    std::istringstream header_stream{header_block};
    std::string request_line;
    if (!std::getline(header_stream, request_line)) {
        throw std::invalid_argument("A HTTP request line hianyzik.");
    }
    request_line = trim_copy(std::move(request_line));

    std::istringstream request_line_stream{request_line};
    std::string method_text;
    std::string raw_target;
    std::string http_version;
    request_line_stream >> method_text >> raw_target >> http_version;
    if (method_text.empty() || raw_target.empty()) {
        throw std::invalid_argument("A HTTP request line ervenytelen.");
    }

    HttpRequest request;
    request.method = parse_method(method_text);
    request.raw_target = raw_target;
    request.path = raw_target.substr(0, raw_target.find('?'));

    std::string header_line;
    while (std::getline(header_stream, header_line)) {
        header_line = trim_copy(std::move(header_line));
        if (header_line.empty()) {
            continue;
        }
        const auto delimiter = header_line.find(':');
        if (delimiter == std::string::npos) {
            continue;
        }
        auto key = lowercase_copy(trim_copy(header_line.substr(0, delimiter)));
        auto value = trim_copy(header_line.substr(delimiter + 1));
        request.headers.insert_or_assign(std::move(key), std::move(value));
    }

    std::size_t content_length = 0;
    if (const auto iterator = request.headers.find("content-length"); iterator != request.headers.end()) {
        content_length = static_cast<std::size_t>(std::stoul(iterator->second));
    }

    request.body = buffer.substr(header_end + 4);
    while (request.body.size() < content_length) {
        const auto received = recv(socket_handle, chunk.data(), static_cast<int>(chunk.size()), 0);
        if (received <= 0) {
            throw std::runtime_error("A HTTP body nem olvashato teljesen.");
        }
        request.body.append(chunk.data(), static_cast<std::size_t>(received));
    }
    request.body.resize(content_length);

    return request;
}

std::string build_http_response(const HttpResponse& response) {
    const bool use_chunked_transfer = response.chunked_transfer || static_cast<bool>(response.stream_writer);
    std::ostringstream stream;
    stream << "HTTP/1.1 " << response.status_code << ' ' << reason_phrase(response.status_code) << "\r\n";
    stream << "Content-Type: " << response.content_type << "\r\n";
    for (const auto& [header_name, header_value] : response.headers) {
        stream << header_name << ": " << header_value << "\r\n";
    }
    if (use_chunked_transfer) {
        stream << "Transfer-Encoding: chunked\r\n";
    } else {
        stream << "Content-Length: " << response.body.size() << "\r\n";
    }
    stream << "Connection: close\r\n\r\n";
    if (!use_chunked_transfer) {
        stream << response.body;
    }
    return stream.str();
}

}  // namespace

HttpStreamWriter::HttpStreamWriter(
    WriteCallback write_callback,
    WriteCallback write_chunk_callback,
    FinishCallback finish_callback,
    ConnectedCallback connected_callback,
    ErrorCallback error_callback)
    : write_callback_(std::move(write_callback)),
      write_chunk_callback_(std::move(write_chunk_callback)),
      finish_callback_(std::move(finish_callback)),
      connected_callback_(std::move(connected_callback)),
      error_callback_(std::move(error_callback)) {}

bool HttpStreamWriter::write(const std::string_view payload) { return write_callback_ ? write_callback_(payload) : false; }

bool HttpStreamWriter::write_chunk(const std::string_view payload) {
    return write_chunk_callback_ ? write_chunk_callback_(payload) : false;
}

bool HttpStreamWriter::finish() {
    if (finished_) {
        return true;
    }
    finished_ = true;
    return finish_callback_ ? finish_callback_() : true;
}

bool HttpStreamWriter::client_connected() const { return connected_callback_ ? connected_callback_() : false; }

int HttpStreamWriter::last_socket_error() const { return error_callback_ ? error_callback_() : 0; }

HttpServer::~HttpServer() { stop(); }

void HttpServer::register_route(HttpMethod method, std::string path, RouteHandler handler) {
    if (running_) {
        throw std::logic_error("Futo HTTP szerverhez nem regisztralhato uj route.");
    }
    routes_.push_back({.method = method, .path = std::move(path), .handler = std::move(handler)});
}

void HttpServer::clear_routes() {
    if (running_) {
        throw std::logic_error("Futo HTTP szerver route-jai nem torolhetok.");
    }
    routes_.clear();
}

void HttpServer::start(const HttpServerConfig& config) {
    if (running_) {
        throw std::logic_error("A HTTP szerver mar fut.");
    }
    if (config.bind_host.empty() || config.bind_port == 0U) {
        throw std::invalid_argument("A HTTP szerver bind konfiguracioja ervenytelen.");
    }

    auto& socket_handle = as_socket(listener_socket_);
    if (socket_handle != INVALID_SOCKET) {
        closesocket(socket_handle);
        socket_handle = INVALID_SOCKET;
    }

    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw std::runtime_error("A Winsock inicializalasa sikertelen.");
    }

    const auto new_socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (new_socket_handle == INVALID_SOCKET) {
        WSACleanup();
        throw std::runtime_error("A listener socket nem hozhato letre.");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config.bind_port);
    if (InetPtonA(AF_INET, config.bind_host.c_str(), &address.sin_addr) != 1) {
        closesocket(new_socket_handle);
        WSACleanup();
        throw std::invalid_argument("A bind_host nem ervenyes IPv4 cim.");
    }

    const BOOL reuse_address = TRUE;
    setsockopt(
        new_socket_handle,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse_address),
        sizeof(reuse_address));

    if (bind(new_socket_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        closesocket(new_socket_handle);
        WSACleanup();
        throw std::runtime_error("A listener socket bind muvelete sikertelen.");
    }
    if (listen(new_socket_handle, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(new_socket_handle);
        WSACleanup();
        throw std::runtime_error("A listener socket listen muvelete sikertelen.");
    }

    config_ = config;
    as_socket(listener_socket_) = new_socket_handle;
    running_ = true;
    worker_thread_ = std::thread([this] { serve_loop(); });
}

void HttpServer::stop() noexcept {
    running_ = false;
    auto& socket_handle = as_socket(listener_socket_);
    if (socket_handle != INVALID_SOCKET) {
        closesocket(socket_handle);
        socket_handle = INVALID_SOCKET;
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    WSACleanup();
}

bool HttpServer::is_running() const noexcept { return running_; }

HttpResponse HttpServer::dispatch(const HttpRequest& request) const {
    for (const auto& route : routes_) {
        if (route.method == request.method && route.path == request.path) {
            return route.handler(request);
        }
    }

    for (const auto& route : routes_) {
        if (route.path == request.path) {
            return {.status_code = 405, .body = R"({"error":{"code":"method_not_allowed","message":"A HTTP metodus nem tamogatott."}})"};
        }
    }

    return {.status_code = 404, .body = R"({"error":{"code":"not_found","message":"Az endpoint nem talalhato."}})"};
}

void HttpServer::serve_loop() {
    auto& socket_handle = as_socket(listener_socket_);
    while (running_) {
        fd_set read_set{};
        FD_ZERO(&read_set);
        FD_SET(socket_handle, &read_set);

        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        const auto select_result = select(0, &read_set, nullptr, nullptr, &timeout);
        if (!running_) {
            break;
        }
        if (select_result == SOCKET_ERROR) {
            break;
        }
        if (select_result == 0) {
            continue;
        }

        sockaddr_in client_address{};
        int client_length = sizeof(client_address);
        const auto client_socket = accept(socket_handle, reinterpret_cast<sockaddr*>(&client_address), &client_length);
        if (client_socket == INVALID_SOCKET) {
            continue;
        }

        std::thread([this, client_socket] {
            HttpResponse response;
            try {
                response = dispatch(read_request(client_socket));
            } catch (const std::invalid_argument& error) {
                response = {.status_code = 400,
                            .body = std::string{"{\"error\":{\"code\":\"bad_request\",\"message\":\""} + error.what() +
                                    "\"}}"};
            } catch (const std::exception& error) {
                response = {.status_code = 500,
                            .body = std::string{"{\"error\":{\"code\":\"internal_error\",\"message\":\""} + error.what() +
                                    "\"}}"};
            }

            const auto payload = build_http_response(response);
            int last_socket_error = 0;
            if (!send_all(client_socket, payload, last_socket_error)) {
                closesocket(client_socket);
                return;
            }

            const bool use_stream_writer = static_cast<bool>(response.stream_writer);
            const bool use_chunked_transfer = response.chunked_transfer || use_stream_writer;
            if (use_stream_writer) {
                bool client_connected = true;
                HttpStreamWriter writer{
                    [&client_socket, &last_socket_error, &client_connected](const std::string_view chunk) {
                        const auto sent = send_all(client_socket, chunk, last_socket_error);
                        client_connected = sent;
                        return sent;
                    },
                    [&client_socket, &last_socket_error, &client_connected](const std::string_view chunk) {
                        const auto sent = send_chunk_frame(client_socket, chunk, last_socket_error);
                        client_connected = sent;
                        return sent;
                    },
                    [&client_socket, &last_socket_error, &client_connected, use_chunked_transfer]() {
                        if (!use_chunked_transfer) {
                            return true;
                        }
                        const auto sent = send_chunk_terminator(client_socket, last_socket_error);
                        client_connected = sent;
                        return sent;
                    },
                    [&client_connected]() { return client_connected; },
                    [&last_socket_error]() { return last_socket_error; }};

                response.stream_writer(writer);
                if (use_chunked_transfer) {
                    const auto finished = writer.finish();
                    (void)finished;
                }
            } else if (use_chunked_transfer) {
                const auto terminated = send_chunk_terminator(client_socket, last_socket_error);
                (void)terminated;
            }

            closesocket(client_socket);
        }).detach();
    }
}

}  // namespace masodik::server
