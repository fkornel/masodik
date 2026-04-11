#include "masodik/runtime/queue_policy.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "masodik/config/json_loader.hpp"
#include "masodik/config/default_paths.hpp"

namespace masodik::runtime {

void QueuePolicy::validate() const {
    if (max_concurrent_requests == 0U) {
        throw std::invalid_argument("A max_concurrent_requests nem lehet 0.");
    }
    if (max_queued_requests < max_concurrent_requests) {
        throw std::invalid_argument("A max_queued_requests nem lehet kisebb a max_concurrent_requests ertekenel.");
    }
    if (max_queue_wait_seconds <= 0) {
        throw std::invalid_argument("A max_queue_wait_seconds csak pozitiv lehet.");
    }
    if (poll_interval_millis <= 0) {
        throw std::invalid_argument("A poll_interval_millis csak pozitiv lehet.");
    }
}

QueuePolicy load_queue_policy(const masodik::config::JsonValue& json_value) {
    QueuePolicy policy{
        .max_queued_requests = masodik::config::default_paths::default_max_queued_requests,
        .max_concurrent_requests = masodik::config::default_paths::default_max_concurrent_requests,
        .overflow_behavior = QueueOverflowBehavior::wait_for_slot,
        .max_queue_wait_seconds = masodik::config::default_paths::default_queue_wait_seconds,
        .poll_interval_millis = masodik::config::default_paths::default_poll_interval_millis};

    if (!json_value.is_object()) {
        policy.validate();
        return policy;
    }

    const auto& object = json_value.as_object();
    policy.max_queued_requests = static_cast<std::size_t>(
        masodik::config::get_number_or(object, "max_queued_requests", static_cast<double>(policy.max_queued_requests)));
    policy.max_concurrent_requests = static_cast<std::size_t>(masodik::config::get_number_or(
        object, "max_concurrent_requests", static_cast<double>(policy.max_concurrent_requests)));
    policy.max_queue_wait_seconds =
        static_cast<int>(masodik::config::get_number_or(object, "max_queue_wait_seconds", policy.max_queue_wait_seconds));
    policy.poll_interval_millis =
        static_cast<int>(masodik::config::get_number_or(object, "poll_interval_millis", policy.poll_interval_millis));

    const auto behavior =
        masodik::config::get_string_or(object, "overflow_behavior", to_string(policy.overflow_behavior));
    if (behavior == "reject_new") {
        policy.overflow_behavior = QueueOverflowBehavior::reject_new;
    } else if (behavior == "wait_for_slot") {
        policy.overflow_behavior = QueueOverflowBehavior::wait_for_slot;
    } else {
        throw std::invalid_argument("Ismeretlen queue overflow viselkedes: " + behavior);
    }

    policy.validate();
    return policy;
}

QueueBudgetDecision evaluate_queue_budget(
    const QueuePolicy& policy,
    const std::size_t current_queued,
    const std::size_t current_running,
    const std::chrono::seconds timeout_budget) {
    policy.validate();

    if (current_running < policy.max_concurrent_requests && current_queued == 0U) {
        return {.accepted = true, .should_wait = false, .estimated_wait_millis = 0, .reason = "Azonnal futtathato."};
    }

    if (current_queued >= policy.max_queued_requests) {
        return {
            .accepted = false,
            .should_wait = false,
            .estimated_wait_millis = 0,
            .reason = "A varakozasi sor megtelt."};
    }

    const auto slots_overload =
        static_cast<double>(std::max<std::size_t>(current_running, policy.max_concurrent_requests)) /
        static_cast<double>(policy.max_concurrent_requests);
    const auto estimated_wait =
        static_cast<int>(std::ceil(slots_overload * static_cast<double>(policy.poll_interval_millis)));

    if (policy.overflow_behavior == QueueOverflowBehavior::reject_new) {
        return {
            .accepted = false,
            .should_wait = false,
            .estimated_wait_millis = estimated_wait,
            .reason = "A policy uj kerest nem enged a sorba."};
    }

    if (timeout_budget < std::chrono::seconds{policy.max_queue_wait_seconds}) {
        return {
            .accepted = false,
            .should_wait = false,
            .estimated_wait_millis = estimated_wait,
            .reason = "A timeout budget rovidebb a megengedett queue varakozasnal."};
    }

    return {
        .accepted = true,
        .should_wait = true,
        .estimated_wait_millis = estimated_wait,
        .reason = "A keres bekerulhet a varakozasi sorba."};
}

std::string to_string(const QueueOverflowBehavior behavior) {
    switch (behavior) {
    case QueueOverflowBehavior::reject_new:
        return "reject_new";
    case QueueOverflowBehavior::wait_for_slot:
        return "wait_for_slot";
    }
    return "unknown";
}

}  // namespace masodik::runtime
