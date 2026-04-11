#include <stdexcept>

#include "test_registry.hpp"

#include "masodik/runtime/startup_state_machine.hpp"

namespace {

void require_throws_invalid_transition(const std::function<void()>& action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return;
    }

    throw std::runtime_error("Az ervenytelen allapotatmenetnek logic_error-t kellett volna dobnia.");
}

}  // namespace

MASODIK_TEST(startup_state_machine_tracks_nominal_serving_path) {
    masodik::runtime::StartupStateMachine machine;

    machine.transition_to(masodik::runtime::RuntimeState::configured, "A konfiguracio betoltve.");
    machine.transition_to(masodik::runtime::RuntimeState::starting, "A pipeline inicializalasa elkezdodott.");
    machine.transition_to(masodik::runtime::RuntimeState::ready, "A pipeline keszen all.");

    masodik::tests::require(
        machine.current_state() == masodik::runtime::RuntimeState::ready,
        "A nominalis startup utan ready allapot vart.");
    masodik::tests::require(machine.serving_ready(), "A ready allapotnak kiszolgalasra kesznek kell lennie.");
}

MASODIK_TEST(startup_state_machine_rejects_invalid_skip_and_allows_restart_cycle) {
    masodik::runtime::StartupStateMachine machine;

    require_throws_invalid_transition([&machine] {
        machine.transition_to(masodik::runtime::RuntimeState::ready, "A discovered -> ready atugras nem ervenyes.");
    });

    machine.transition_to(masodik::runtime::RuntimeState::configured, "A konfiguracio kesz.");
    machine.transition_to(masodik::runtime::RuntimeState::starting, "Startup folyamat.");
    machine.transition_to(
        masodik::runtime::RuntimeState::failed,
        "A GenAI runtime hianyzik.",
        masodik::runtime::RuntimeError::missing_toolchain);
    machine.transition_to(masodik::runtime::RuntimeState::starting, "Ujrainditasi kiserlet.");
    machine.transition_to(masodik::runtime::RuntimeState::stopped, "Leallitas.");
    machine.transition_to(masodik::runtime::RuntimeState::configured, "Ujrakonfiguralt inditas.");

    masodik::tests::require(
        machine.current_state() == masodik::runtime::RuntimeState::configured,
        "A restart ciklus vegen configured allapot vart.");
}
