#pragma once

#include <string_view>
#include <system_error>

namespace masodik::runtime {

enum class RuntimeError {
    none = 0,
    invalid_configuration = 1,
    missing_toolchain = 2,
    missing_model = 3,
    process_spawn_failed = 4,
    network_failure = 5,
    health_timeout = 6,
    capacity_exceeded = 7,
    degraded_profile = 8,
    unsupported_operation = 9,
    split_manifest_invalid = 10,
    split_stage_configuration = 11,
    split_fallback_detected = 12,
    split_handoff_failed = 13,
    split_memory_budget_exceeded = 14
};

enum class RuntimeFailureScope {
    none = 0,
    startup,
    stage_1,
    stage_2,
    handoff
};

const std::error_category& runtime_error_category() noexcept;
std::error_code make_error_code(RuntimeError error) noexcept;
std::string_view to_string(RuntimeError error) noexcept;
std::string_view to_string(RuntimeFailureScope scope) noexcept;

}  // namespace masodik::runtime

template <>
struct std::is_error_code_enum<masodik::runtime::RuntimeError> : true_type {};
