#include "masodik/runtime/degraded_state_guard.hpp"

namespace masodik::runtime {

DegradedStateGuard::DegradedStateGuard(QueuePolicy queue_policy) : queue_policy_(std::move(queue_policy)) {}

std::string DegradedStateGuard::effective_state(
    const std::string_view pipeline_state,
    const ServiceConfig& service_config,
    const masodik::diagnostics::QueueMetricsSnapshot& snapshot,
    const bool gpu_offload_active) const {
    if (pipeline_state == "failed" || pipeline_state == "degraded") {
        return std::string{pipeline_state};
    }

    if (service_config.serving_profile.hybrid_enabled && !gpu_offload_active) {
        return "degraded";
    }

    if (snapshot.queued_requests > 0U && snapshot.running_requests > 0U) {
        return "degraded";
    }

    if (snapshot.active_streams > 0U && snapshot.queued_requests > 0U) {
        return "degraded";
    }

    return std::string{pipeline_state};
}

}  // namespace masodik::runtime
