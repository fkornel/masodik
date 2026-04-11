#pragma once

#include <string_view>

namespace masodik::runtime {

enum class RuntimeState {
    discovered,
    configured,
    starting,
    ready,
    degraded,
    failed,
    stopped
};

inline constexpr std::string_view to_string(const RuntimeState state) noexcept {
    switch (state) {
    case RuntimeState::discovered:
        return "discovered";
    case RuntimeState::configured:
        return "configured";
    case RuntimeState::starting:
        return "starting";
    case RuntimeState::ready:
        return "ready";
    case RuntimeState::degraded:
        return "degraded";
    case RuntimeState::failed:
        return "failed";
    case RuntimeState::stopped:
        return "stopped";
    }
    return "unknown";
}

}  // namespace masodik::runtime
