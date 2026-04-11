#include "masodik/runtime/error_category.hpp"

#include <string>

namespace masodik::runtime {
namespace {

class RuntimeErrorCategoryImpl final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override { return "masodik.runtime"; }

    [[nodiscard]] std::string message(const int condition) const override {
        switch (static_cast<RuntimeError>(condition)) {
        case RuntimeError::none:
            return "nincs hiba";
        case RuntimeError::invalid_configuration:
            return "ervenytelen konfiguracio";
        case RuntimeError::missing_toolchain:
            return "hianyzo toolchain komponens";
        case RuntimeError::missing_model:
            return "hianyzo modell eroforras";
        case RuntimeError::process_spawn_failed:
            return "az OVMS folyamat nem indithato";
        case RuntimeError::network_failure:
            return "halozaati vagy HTTP hiba";
        case RuntimeError::health_timeout:
            return "health timeout";
        case RuntimeError::capacity_exceeded:
            return "kapacitas korlat";
        case RuntimeError::degraded_profile:
            return "a kivant gyorsitott profil nem tarthato";
        case RuntimeError::unsupported_operation:
            return "nem tamogatott muvelet";
        case RuntimeError::split_manifest_invalid:
            return "ervenytelen split manifest";
        case RuntimeError::split_stage_configuration:
            return "ervenytelen split stage konfiguracio";
        case RuntimeError::split_fallback_detected:
            return "tiltott split fallback eszlelve";
        case RuntimeError::split_handoff_failed:
            return "split handoff hiba";
        case RuntimeError::split_memory_budget_exceeded:
            return "split memoria-budget tullepes";
        }
        return "ismeretlen runtime hiba";
    }
};

}  // namespace

const std::error_category& runtime_error_category() noexcept {
    static RuntimeErrorCategoryImpl category;
    return category;
}

std::error_code make_error_code(const RuntimeError error) noexcept {
    return {static_cast<int>(error), runtime_error_category()};
}

std::string_view to_string(const RuntimeError error) noexcept {
    switch (error) {
    case RuntimeError::none:
        return "none";
    case RuntimeError::invalid_configuration:
        return "invalid_configuration";
    case RuntimeError::missing_toolchain:
        return "missing_toolchain";
    case RuntimeError::missing_model:
        return "missing_model";
    case RuntimeError::process_spawn_failed:
        return "process_spawn_failed";
    case RuntimeError::network_failure:
        return "network_failure";
    case RuntimeError::health_timeout:
        return "health_timeout";
    case RuntimeError::capacity_exceeded:
        return "capacity_exceeded";
    case RuntimeError::degraded_profile:
        return "degraded_profile";
    case RuntimeError::unsupported_operation:
        return "unsupported_operation";
    case RuntimeError::split_manifest_invalid:
        return "split_manifest_invalid";
    case RuntimeError::split_stage_configuration:
        return "split_stage_configuration";
    case RuntimeError::split_fallback_detected:
        return "split_fallback_detected";
    case RuntimeError::split_handoff_failed:
        return "split_handoff_failed";
    case RuntimeError::split_memory_budget_exceeded:
        return "split_memory_budget_exceeded";
    }
    return "unknown";
}

std::string_view to_string(const RuntimeFailureScope scope) noexcept {
    switch (scope) {
    case RuntimeFailureScope::none:
        return "";
    case RuntimeFailureScope::startup:
        return "startup";
    case RuntimeFailureScope::stage_1:
        return "stage-1";
    case RuntimeFailureScope::stage_2:
        return "stage-2";
    case RuntimeFailureScope::handoff:
        return "handoff";
    }
    return "";
}

}  // namespace masodik::runtime
