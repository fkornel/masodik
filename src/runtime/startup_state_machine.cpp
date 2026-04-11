#include "masodik/runtime/startup_state_machine.hpp"

#include <stdexcept>

namespace masodik::runtime {
namespace {

bool is_terminal_state(const RuntimeState state) noexcept {
    return state == RuntimeState::failed || state == RuntimeState::stopped;
}

}  // namespace

RuntimeState StartupStateMachine::current_state() const noexcept { return current_state_; }

RuntimeError StartupStateMachine::last_error() const noexcept { return last_error_; }

const std::string& StartupStateMachine::last_detail() const noexcept { return last_detail_; }

bool StartupStateMachine::serving_ready() const noexcept {
    return current_state_ == RuntimeState::ready || current_state_ == RuntimeState::degraded;
}

bool StartupStateMachine::can_transition_to(const RuntimeState target) const noexcept {
    switch (current_state_) {
    case RuntimeState::discovered:
        return target == RuntimeState::configured || target == RuntimeState::failed || target == RuntimeState::stopped;
    case RuntimeState::configured:
        return target == RuntimeState::starting || target == RuntimeState::failed || target == RuntimeState::stopped;
    case RuntimeState::starting:
        return target == RuntimeState::ready || target == RuntimeState::degraded || target == RuntimeState::failed ||
               target == RuntimeState::stopped;
    case RuntimeState::ready:
        return target == RuntimeState::degraded || target == RuntimeState::failed || target == RuntimeState::stopped;
    case RuntimeState::degraded:
        return target == RuntimeState::ready || target == RuntimeState::failed || target == RuntimeState::stopped;
    case RuntimeState::failed:
        return target == RuntimeState::configured || target == RuntimeState::starting || target == RuntimeState::stopped;
    case RuntimeState::stopped:
        return target == RuntimeState::configured;
    }
    return false;
}

void StartupStateMachine::transition_to(const RuntimeState target, std::string detail, const RuntimeError error) {
    if (!can_transition_to(target)) {
        throw std::logic_error(
            "Ervenytelen runtime allapotatmenet: " + std::string{to_string(current_state_)} + " -> " +
            std::string{to_string(target)});
    }

    current_state_ = target;
    last_detail_ = std::move(detail);
    if (is_terminal_state(target)) {
        last_error_ = error == RuntimeError::none ? RuntimeError::unsupported_operation : error;
    } else {
        last_error_ = RuntimeError::none;
    }
}

}  // namespace masodik::runtime
