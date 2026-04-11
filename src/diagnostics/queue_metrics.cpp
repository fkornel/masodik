#include "masodik/diagnostics/queue_metrics.hpp"

namespace masodik::diagnostics {

void QueueMetrics::set_queued_requests(const std::size_t queued_requests) noexcept {
    queued_requests_ = queued_requests;
    update_peak_queue(queued_requests);
}

void QueueMetrics::set_running_requests(const std::size_t running_requests) noexcept { running_requests_ = running_requests; }

void QueueMetrics::note_started_request() noexcept { ++total_started_requests_; }

void QueueMetrics::note_completed_request() noexcept { ++total_completed_requests_; }

void QueueMetrics::note_stream_started() noexcept {
    const auto active_streams = active_streams_.fetch_add(1) + 1;
    auto current_peak = peak_active_streams_.load();
    while (active_streams > current_peak && !peak_active_streams_.compare_exchange_weak(current_peak, active_streams)) {
    }
}

void QueueMetrics::note_stream_finished() noexcept {
    auto current = active_streams_.load();
    while (current > 0 && !active_streams_.compare_exchange_weak(current, current - 1)) {
    }
}

void QueueMetrics::note_stream_failure() noexcept { ++total_stream_failures_; }

void QueueMetrics::note_tool_call_response(const std::size_t tool_call_count) noexcept {
    if (tool_call_count == 0) {
        return;
    }
    total_tool_call_responses_.fetch_add(tool_call_count);
}

void QueueMetrics::note_tool_call_stream_delta(const std::size_t delta_count) noexcept {
    if (delta_count == 0) {
        return;
    }
    total_tool_call_stream_deltas_.fetch_add(delta_count);
}

void QueueMetrics::note_tool_call_validation_failure() noexcept { ++total_tool_call_validation_failures_; }

void QueueMetrics::set_last_wait_millis(const int wait_millis) noexcept { last_wait_millis_ = wait_millis; }

void QueueMetrics::set_last_disconnect_error(const int disconnect_error) noexcept { last_disconnect_error_ = disconnect_error; }

QueueMetricsSnapshot QueueMetrics::snapshot() const noexcept {
    return {
        .queued_requests = queued_requests_.load(),
        .running_requests = running_requests_.load(),
        .active_streams = active_streams_.load(),
        .peak_queued_requests = peak_queued_requests_.load(),
        .peak_active_streams = peak_active_streams_.load(),
        .total_started_requests = total_started_requests_.load(),
        .total_completed_requests = total_completed_requests_.load(),
        .total_stream_failures = total_stream_failures_.load(),
        .total_tool_call_responses = total_tool_call_responses_.load(),
        .total_tool_call_stream_deltas = total_tool_call_stream_deltas_.load(),
        .total_tool_call_validation_failures = total_tool_call_validation_failures_.load(),
        .last_wait_millis = last_wait_millis_.load(),
        .last_disconnect_error = last_disconnect_error_.load()};
}

void QueueMetrics::update_peak_queue(const std::size_t queued_requests) noexcept {
    auto current_peak = peak_queued_requests_.load();
    while (queued_requests > current_peak && !peak_queued_requests_.compare_exchange_weak(current_peak, queued_requests)) {
    }
}

}  // namespace masodik::diagnostics
