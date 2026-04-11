#include "masodik/runtime/health_monitor.hpp"

#include <thread>

namespace masodik::runtime {

HealthMonitor::HealthMonitor(masodik::http::HttpClient http_client, const std::chrono::milliseconds poll_interval)
    : http_client_(std::move(http_client)), poll_interval_(poll_interval) {}

HealthSnapshot HealthMonitor::sample_ready_endpoint(
    const std::string_view ready_url,
    const std::string_view active_profile,
    const std::string_view active_model) const {
    HealthSnapshot snapshot;
    snapshot.active_profile = std::string{active_profile};
    snapshot.active_model = std::string{active_model};

    try {
        const auto response = http_client_.get(ready_url, poll_interval_);
        snapshot.ready = response.status_code == 200;
        snapshot.runtime_state = snapshot.ready ? RuntimeState::ready : RuntimeState::starting;
        snapshot.gpu_offload_active = snapshot.ready;
    } catch (const std::runtime_error& error) {
        snapshot.runtime_state = RuntimeState::failed;
        snapshot.last_error_category = RuntimeError::network_failure;
        snapshot.last_error_message = error.what();
    }

    return snapshot;
}

HealthSnapshot HealthMonitor::wait_until_ready(
    const std::string_view ready_url,
    const std::chrono::milliseconds timeout,
    const std::string_view active_profile,
    const std::string_view active_model) const {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        auto snapshot = sample_ready_endpoint(ready_url, active_profile, active_model);
        if (snapshot.ready) {
            return snapshot;
        }
        std::this_thread::sleep_for(poll_interval_);
    }

    return {
        .timestamp = std::chrono::system_clock::now(),
        .runtime_state = RuntimeState::failed,
        .active_profile = std::string{active_profile},
        .active_model = std::string{active_model},
        .queue_depth = 0,
        .ready = false,
        .last_error_category = RuntimeError::health_timeout,
        .last_error_message = "A runtime a megadott idon belul nem valt ready allapotba.",
        .gpu_offload_active = false};
}

}  // namespace masodik::runtime
