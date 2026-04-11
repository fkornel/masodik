#pragma once

#include <string>

#include "masodik/runtime/error_category.hpp"
#include "masodik/runtime/service_config.hpp"

namespace masodik::diagnostics {

struct SplitRuntimeErrorContext {
    bool split_mode = false;
    masodik::runtime::RuntimeFailureScope failure_scope = masodik::runtime::RuntimeFailureScope::none;
    bool fallback_detected = false;
    std::string stage_1_state;
    std::string stage_2_state;
    std::string handoff_state;
    std::string last_error_detail;
};

std::string build_runtime_error_report(
    masodik::runtime::RuntimeError error,
    std::string_view detail,
    const masodik::runtime::ServiceConfig& service_config,
    SplitRuntimeErrorContext split_context = {});

}  // namespace masodik::diagnostics
