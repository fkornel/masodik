#pragma once

#include <atomic>
#include <chrono>

namespace masodik::runtime {

class ShutdownController {
public:
    void request_stop() noexcept;
    void reset() noexcept;
    [[nodiscard]] bool stop_requested() const noexcept;
    void wait_until_stop_requested(std::chrono::milliseconds poll_interval) const;

private:
    std::atomic<bool> stop_requested_{false};
};

}  // namespace masodik::runtime
