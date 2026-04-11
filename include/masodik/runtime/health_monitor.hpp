#pragma once

#include <chrono>
#include <string_view>

#include "masodik/http/http_client.hpp"
#include "masodik/runtime/health_snapshot.hpp"

namespace masodik::runtime {

class HealthMonitor {
public:
    HealthMonitor(
        masodik::http::HttpClient http_client = {},
        std::chrono::milliseconds poll_interval = std::chrono::milliseconds{1000});

    HealthSnapshot sample_ready_endpoint(
        std::string_view ready_url,
        std::string_view active_profile,
        std::string_view active_model) const;

    HealthSnapshot wait_until_ready(
        std::string_view ready_url,
        std::chrono::milliseconds timeout,
        std::string_view active_profile,
        std::string_view active_model) const;

private:
    masodik::http::HttpClient http_client_;
    std::chrono::milliseconds poll_interval_;
};

}  // namespace masodik::runtime
