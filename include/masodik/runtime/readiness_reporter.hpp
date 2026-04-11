#pragma once

#include "masodik/api/openai_contract.hpp"
#include "masodik/diagnostics/queue_metrics.hpp"
#include "masodik/runtime/health_snapshot.hpp"
#include "masodik/runtime/runtime_state.hpp"
#include "masodik/runtime/service_config.hpp"

namespace masodik::runtime {

class ReadinessReporter {
public:
    [[nodiscard]] HealthSnapshot make_health_snapshot(
        const ServiceConfig& service_config,
        RuntimeState runtime_state,
        bool ready,
        const masodik::diagnostics::QueueMetricsSnapshot& queue_snapshot = {}) const;

    [[nodiscard]] masodik::api::HealthResponse make_health_response(
        const ServiceConfig& service_config,
        RuntimeState runtime_state,
        bool ready,
        const masodik::diagnostics::QueueMetricsSnapshot& queue_snapshot = {}) const;
};

}  // namespace masodik::runtime
