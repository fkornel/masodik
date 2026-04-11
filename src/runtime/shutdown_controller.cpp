#include "masodik/runtime/shutdown_controller.hpp"

#include <thread>

namespace masodik::runtime {

void ShutdownController::request_stop() noexcept { stop_requested_ = true; }

void ShutdownController::reset() noexcept { stop_requested_ = false; }

bool ShutdownController::stop_requested() const noexcept { return stop_requested_.load(); }

void ShutdownController::wait_until_stop_requested(const std::chrono::milliseconds poll_interval) const {
    while (!stop_requested()) {
        std::this_thread::sleep_for(poll_interval);
    }
}

}  // namespace masodik::runtime
