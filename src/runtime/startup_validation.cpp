#include "masodik/runtime/startup_validation.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"

namespace masodik::runtime {

SplitStartupValidationHookResult validate_split_startup(
    const ServiceConfig& service_config,
    const masodik::genai::LlmPipelineService& pipeline_service) {
    SplitStartupValidationHookResult result;
    result.valid = true;
    if (!service_config.split_mode) {
        return result;
    }

    const auto health = pipeline_service.make_health_response();
    if (health.fallback_detected) {
        result.valid = false;
        result.error = RuntimeError::split_fallback_detected;
        result.failure_scope = RuntimeFailureScope::startup;
        result.message = health.status_message;
        return result;
    }

    if (health.stage_1_state == "failed") {
        result.valid = false;
        result.error = RuntimeError::split_stage_configuration;
        result.failure_scope = RuntimeFailureScope::stage_1;
        result.message = health.status_message;
        return result;
    }
    if (health.stage_2_state == "failed") {
        result.valid = false;
        result.error = RuntimeError::split_stage_configuration;
        result.failure_scope = RuntimeFailureScope::stage_2;
        result.message = health.status_message;
        return result;
    }
    if (health.handoff_state == "failed") {
        result.valid = false;
        result.error = RuntimeError::split_handoff_failed;
        result.failure_scope = RuntimeFailureScope::handoff;
        result.message = health.status_message;
    }
    return result;
}

StartupValidationSummary validate_startup(
    const ServiceConfig&,
    const masodik::genai::LlmPipelineService& pipeline_service) {
    StartupValidationSummary summary;
    summary.valid = pipeline_service.ready();
    summary.message = pipeline_service.status_message();

    const auto split_summary = validate_split_startup(pipeline_service.service_config(), pipeline_service);
    if (!split_summary.valid) {
        summary.valid = false;
        summary.runtime_state = RuntimeState::failed;
        summary.error = split_summary.error;
        summary.failure_scope = split_summary.failure_scope;
        summary.message = split_summary.message.empty() ? pipeline_service.status_message() : split_summary.message;
        return summary;
    }

    if (pipeline_service.state() == "ready") {
        summary.runtime_state = RuntimeState::ready;
        return summary;
    }
    if (pipeline_service.state() == "degraded") {
        summary.runtime_state = RuntimeState::degraded;
        return summary;
    }
    if (pipeline_service.state() == "failed") {
        summary.runtime_state = RuntimeState::failed;
        summary.error = summary.message.find("modell") != std::string::npos ? RuntimeError::missing_model
                                                                            : RuntimeError::missing_toolchain;
        if (pipeline_service.service_config().split_mode) {
            const auto health = pipeline_service.make_health_response();
            if (health.last_failure_scope == "stage-1") {
                summary.failure_scope = RuntimeFailureScope::stage_1;
                summary.error = RuntimeError::split_stage_configuration;
            } else if (health.last_failure_scope == "stage-2") {
                summary.failure_scope = RuntimeFailureScope::stage_2;
                summary.error = RuntimeError::split_stage_configuration;
            } else if (health.last_failure_scope == "handoff") {
                summary.failure_scope = RuntimeFailureScope::handoff;
                summary.error = RuntimeError::split_handoff_failed;
            } else {
                summary.failure_scope = RuntimeFailureScope::startup;
            }
        }
        return summary;
    }

    summary.runtime_state = RuntimeState::starting;
    summary.error = RuntimeError::unsupported_operation;
    summary.failure_scope = pipeline_service.service_config().split_mode ? RuntimeFailureScope::startup : RuntimeFailureScope::none;
    return summary;
}

}  // namespace masodik::runtime
