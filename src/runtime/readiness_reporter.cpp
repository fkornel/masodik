#include "masodik/runtime/readiness_reporter.hpp"

namespace masodik::runtime {
namespace {

std::string runtime_status_message(const RuntimeState runtime_state, const bool ready) {
    if (ready && runtime_state == RuntimeState::ready) {
        return "A pipeline keszen all.";
    }
    return std::string{to_string(runtime_state)};
}

std::string split_stage_state_from_runtime(const RuntimeState runtime_state, const bool ready) {
    if (ready && runtime_state == RuntimeState::ready) {
        return "ready";
    }
    switch (runtime_state) {
    case RuntimeState::failed:
        return "failed";
    case RuntimeState::degraded:
        return "degraded";
    case RuntimeState::starting:
    case RuntimeState::configured:
        return "starting";
    case RuntimeState::ready:
        return "ready";
    case RuntimeState::discovered:
        return "uninitialized";
    }
    return "uninitialized";
}

}  // namespace

HealthSnapshot ReadinessReporter::make_health_snapshot(
    const ServiceConfig& service_config,
    const RuntimeState runtime_state,
    const bool ready,
    const masodik::diagnostics::QueueMetricsSnapshot& queue_snapshot) const {
    return {
        .timestamp = std::chrono::system_clock::now(),
        .runtime_state = runtime_state,
        .active_profile = service_config.active_profile,
        .active_model = service_config.active_model,
        .queue_depth = queue_snapshot.queued_requests,
        .active_streams = queue_snapshot.active_streams,
        .peak_active_streams = queue_snapshot.peak_active_streams,
        .ready = ready,
        .last_error_category = RuntimeError::none,
        .last_error_message = {},
        .total_stream_failures = queue_snapshot.total_stream_failures,
        .last_disconnect_error = queue_snapshot.last_disconnect_error,
        .gpu_offload_active = false,
        .status_message = runtime_status_message(runtime_state, ready),
        .split_mode = service_config.split_mode,
         .split_profile = service_config.split_profile,
         .stage_1_state = service_config.split_mode ? split_stage_state_from_runtime(runtime_state, ready) : std::string{},
         .stage_2_state = service_config.split_mode ? split_stage_state_from_runtime(runtime_state, ready) : std::string{},
         .handoff_state = service_config.split_mode ? (ready ? "ready" : "idle") : std::string{},
         .request_path_mode = service_config.split_mode ? "placeholder" : std::string{},
         .stage_1_device = service_config.split_mode ? service_config.generation_config.stage_1_device : std::string{},
         .stage_2_device = service_config.split_mode ? service_config.generation_config.stage_2_device : std::string{},
         .stage_1_execution_devices = {},
         .stage_2_execution_devices = {},
         .split_artifact_version = service_config.split_mode ? service_config.model_repository.split_artifact_manifest.artifact_version
                                                            : std::string{},
         .validated_context_tokens = service_config.split_mode
             ? service_config.model_repository.split_artifact_manifest.validated_context_tokens
             : 0,
         .last_split_request_id = {},
         .last_split_sequence_id = {},
         .last_failure_scope = RuntimeFailureScope::none,
         .fallback_detected = false,
        .memory_summary = {.stage_1 = service_config.split_mode
                                          ? "budget:" + std::to_string(service_config.generation_config.stage_1_memory_budget_gb) + "GB"
                                          : std::string{},
                           .stage_2 = service_config.split_mode
                                          ? "budget:" + std::to_string(service_config.generation_config.stage_2_memory_budget_gb) + "GB"
                                          : std::string{},
                           .system = service_config.split_mode ? "bounded" : std::string{}},
        .last_failure_summary = {}};
    }

masodik::api::HealthResponse ReadinessReporter::make_health_response(
    const ServiceConfig& service_config,
    const RuntimeState runtime_state,
    const bool ready,
    const masodik::diagnostics::QueueMetricsSnapshot& queue_snapshot) const {
    const auto snapshot = make_health_snapshot(service_config, runtime_state, ready, queue_snapshot);
    return {
        .ready = snapshot.ready,
        .state = std::string{to_string(snapshot.runtime_state)},
        .active_model = snapshot.active_model,
        .active_profile = snapshot.active_profile,
        .queue_depth = snapshot.queue_depth,
        .active_streams = snapshot.active_streams,
        .peak_active_streams = snapshot.peak_active_streams,
        .total_stream_failures = snapshot.total_stream_failures,
        .last_disconnect_error = snapshot.last_disconnect_error,
        .gpu_offload_active = snapshot.gpu_offload_active,
        .status_message = snapshot.status_message,
        .split_mode = snapshot.split_mode,
        .split_profile = snapshot.split_profile,
        .stage_1_state = snapshot.stage_1_state,
        .stage_2_state = snapshot.stage_2_state,
        .handoff_state = snapshot.handoff_state,
        .request_path_mode = snapshot.request_path_mode,
        .stage_1_device = snapshot.stage_1_device,
        .stage_2_device = snapshot.stage_2_device,
        .stage_1_execution_devices = snapshot.stage_1_execution_devices,
        .stage_2_execution_devices = snapshot.stage_2_execution_devices,
        .split_artifact_version = snapshot.split_artifact_version,
        .validated_context_tokens = snapshot.validated_context_tokens,
        .last_split_request_id = snapshot.last_split_request_id,
        .last_split_sequence_id = snapshot.last_split_sequence_id,
        .last_failure_scope = std::string{to_string(snapshot.last_failure_scope)},
        .fallback_detected = snapshot.fallback_detected,
        .memory_summary = {.stage_1 = snapshot.memory_summary.stage_1,
                           .stage_2 = snapshot.memory_summary.stage_2,
                           .system = snapshot.memory_summary.system},
        .last_failure_summary = snapshot.last_failure_summary};
    }

}  // namespace masodik::runtime
