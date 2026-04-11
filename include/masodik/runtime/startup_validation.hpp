#pragma once

#include <string>

#include "masodik/runtime/error_category.hpp"
#include "masodik/runtime/runtime_state.hpp"
#include "masodik/runtime/service_config.hpp"

namespace masodik::genai {
class LlmPipelineService;
}

namespace masodik::runtime {

struct StartupValidationSummary {
    bool valid = false;
    RuntimeState runtime_state = RuntimeState::discovered;
    RuntimeError error = RuntimeError::none;
    RuntimeFailureScope failure_scope = RuntimeFailureScope::none;
    std::string message;
};

struct SplitStartupValidationHookResult {
    bool valid = true;
    RuntimeError error = RuntimeError::none;
    RuntimeFailureScope failure_scope = RuntimeFailureScope::none;
    std::string message;
};

[[nodiscard]] SplitStartupValidationHookResult validate_split_startup(
    const ServiceConfig& service_config,
    const masodik::genai::LlmPipelineService& pipeline_service);

StartupValidationSummary validate_startup(
    const ServiceConfig& service_config,
    const masodik::genai::LlmPipelineService& pipeline_service);

}  // namespace masodik::runtime
