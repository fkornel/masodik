#pragma once

#include <string>
#include <string_view>

#include "masodik/diagnostics/queue_metrics.hpp"
#include "masodik/runtime/service_config.hpp"

namespace masodik::runtime {

class DegradedStateGuard {
public:
    explicit DegradedStateGuard(QueuePolicy queue_policy);

    [[nodiscard]] std::string effective_state(
        std::string_view pipeline_state,
        const ServiceConfig& service_config,
        const masodik::diagnostics::QueueMetricsSnapshot& snapshot,
        bool gpu_offload_active) const;

private:
    QueuePolicy queue_policy_;
};

}  // namespace masodik::runtime
