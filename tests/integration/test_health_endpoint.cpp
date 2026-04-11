#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "test_registry.hpp"

#include "masodik/runtime/health_monitor.hpp"

#pragma comment(lib, "Ws2_32.lib")

namespace {

class LocalHttpResponder {
public:
    explicit LocalHttpResponder(const unsigned short port) : port_(port) {}

    void start() {
        running_ = true;
        server_thread_ = std::thread([this] { serve(); });
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
    }

    void stop() {
        running_ = false;
        if (server_socket_ != INVALID_SOCKET) {
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    ~LocalHttpResponder() { stop(); }

private:
    void serve() {
        WSADATA wsa_data{};
        WSAStartup(MAKEWORD(2, 2), &wsa_data);

        server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port_);
        bind(server_socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        listen(server_socket_, 1);

        while (running_) {
            sockaddr_in client_address{};
            int length = sizeof(client_address);
            const auto client = accept(server_socket_, reinterpret_cast<sockaddr*>(&client_address), &length);
            if (client == INVALID_SOCKET) {
                if (!running_) {
                    break;
                }
                continue;
            }

            char buffer[512]{};
            recv(client, buffer, sizeof(buffer), 0);
            const std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
            send(client, response.data(), static_cast<int>(response.size()), 0);
            closesocket(client);
        }

        WSACleanup();
    }

    unsigned short port_ = 0;
    std::atomic<bool> running_{false};
    SOCKET server_socket_ = INVALID_SOCKET;
    std::thread server_thread_;
};

}  // namespace

MASODIK_TEST(health_monitor_marks_local_ready_endpoint_as_ready) {
    constexpr unsigned short port = 29123;
    LocalHttpResponder responder{port};
    responder.start();

    masodik::runtime::HealthMonitor monitor;
    const auto snapshot = monitor.wait_until_ready(
        "http://127.0.0.1:29123/v2/health/ready",
        std::chrono::seconds{5},
        "integration-test",
        "dummy-model");

    responder.stop();

    masodik::tests::require(snapshot.ready, "A health monitor nem latta ready allapotunak a lokalis endpointot.");
}
