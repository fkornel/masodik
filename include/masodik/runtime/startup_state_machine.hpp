#pragma once

#include <string>

#include "masodik/runtime/error_category.hpp"
#include "masodik/runtime/runtime_state.hpp"

namespace masodik::runtime {

class StartupStateMachine {
public:
    [[nodiscard]] RuntimeState current_state() const noexcept;
    [[nodiscard]] RuntimeError last_error() const noexcept;
    [[nodiscard]] const std::string& last_detail() const noexcept;
    [[nodiscard]] bool serving_ready() const noexcept;

    [[nodiscard]] bool can_transition_to(RuntimeState target) const noexcept;
    void transition_to(RuntimeState target, std::string detail = {}, RuntimeError error = RuntimeError::none);

private:
    RuntimeState current_state_ = RuntimeState::discovered;
    RuntimeError last_error_ = RuntimeError::none;
    std::string last_detail_;
};

}  // namespace masodik::runtime
