#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace masodik::diagnostics {

struct QueueMetricsSnapshot {
    std::size_t queued_requests = 0;
    std::size_t running_requests = 0;
    std::size_t active_streams = 0;
    std::size_t peak_queued_requests = 0;
    std::size_t peak_active_streams = 0;
    std::uint64_t total_started_requests = 0;
    std::uint64_t total_completed_requests = 0;
    std::uint64_t total_stream_failures = 0;
    std::uint64_t total_tool_call_responses = 0;
    std::uint64_t total_tool_call_stream_deltas = 0;
    std::uint64_t total_tool_call_validation_failures = 0;
    int last_wait_millis = 0;
    int last_disconnect_error = 0;
};

class QueueMetrics {
public:
    void set_queued_requests(std::size_t queued_requests) noexcept;
    void set_running_requests(std::size_t running_requests) noexcept;
    void note_started_request() noexcept;
    void note_completed_request() noexcept;
    void note_stream_started() noexcept;
    void note_stream_finished() noexcept;
    void note_stream_failure() noexcept;
    void note_tool_call_response(std::size_t tool_call_count = 1) noexcept;
    void note_tool_call_stream_delta(std::size_t delta_count = 1) noexcept;
    void note_tool_call_validation_failure() noexcept;
    void set_last_wait_millis(int wait_millis) noexcept;
    void set_last_disconnect_error(int disconnect_error) noexcept;

    [[nodiscard]] QueueMetricsSnapshot snapshot() const noexcept;

private:
    void update_peak_queue(std::size_t queued_requests) noexcept;

    std::atomic<std::size_t> queued_requests_{0};
    std::atomic<std::size_t> running_requests_{0};
    std::atomic<std::size_t> active_streams_{0};
    std::atomic<std::size_t> peak_queued_requests_{0};
    std::atomic<std::size_t> peak_active_streams_{0};
    std::atomic<std::uint64_t> total_started_requests_{0};
    std::atomic<std::uint64_t> total_completed_requests_{0};
    std::atomic<std::uint64_t> total_stream_failures_{0};
    std::atomic<std::uint64_t> total_tool_call_responses_{0};
    std::atomic<std::uint64_t> total_tool_call_stream_deltas_{0};
    std::atomic<std::uint64_t> total_tool_call_validation_failures_{0};
    std::atomic<int> last_wait_millis_{0};
    std::atomic<int> last_disconnect_error_{0};
};

}  // namespace masodik::diagnostics
