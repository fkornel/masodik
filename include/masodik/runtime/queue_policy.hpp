#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace masodik::config {
class JsonValue;
}

namespace masodik::runtime {

enum class QueueOverflowBehavior {
    reject_new,
    wait_for_slot
};

struct QueuePolicy {
    std::size_t max_queued_requests = 32;
    std::size_t max_concurrent_requests = 5;
    QueueOverflowBehavior overflow_behavior = QueueOverflowBehavior::wait_for_slot;
    int max_queue_wait_seconds = 14400;
    int poll_interval_millis = 500;

    void validate() const;
};

struct QueueBudgetDecision {
    bool accepted = false;
    bool should_wait = false;
    int estimated_wait_millis = 0;
    std::string reason;
};

QueuePolicy load_queue_policy(const masodik::config::JsonValue& json_value);
QueueBudgetDecision evaluate_queue_budget(
    const QueuePolicy& policy,
    std::size_t current_queued,
    std::size_t current_running,
    std::chrono::seconds timeout_budget);
std::string to_string(QueueOverflowBehavior behavior);

}  // namespace masodik::runtime
