#include "masodik/diagnostics/runtime_error_reporter.hpp"

#include <sstream>

namespace masodik::diagnostics {
namespace {

std::string remediation_hint(const masodik::runtime::RuntimeError error) {
    switch (error) {
    case masodik::runtime::RuntimeError::missing_toolchain:
        return "Ellenorizd az OpenVINO/GenAI runtime DLL-eket es a CMake hint utvonalakat.";
    case masodik::runtime::RuntimeError::missing_model:
        return "Ellenorizd, hogy a helyi modellrepo es az openvino_model.xml/bin fajlok elerhetok.";
    case masodik::runtime::RuntimeError::invalid_configuration:
        return "Ellenorizd a profile bind host/port, device es timeout mezoket.";
    case masodik::runtime::RuntimeError::capacity_exceeded:
        return "Csokkentsd a konkurenciat vagy szigorithatsz a split queue policy-n; low-concurrency split profilon inkabb reject_new javasolt.";
    case masodik::runtime::RuntimeError::split_memory_budget_exceeded:
        return "Csokkentsd a context meretet vagy novelj stage-enkenti memoria-budgetet; a ket stage sajat KV-cache-szel szamol.";
    case masodik::runtime::RuntimeError::split_handoff_failed:
        return "Ellenorizd a hidden-state handoff szerzodest, a stage-sorrendet es a request-szintu handoff eletciklust.";
    case masodik::runtime::RuntimeError::split_fallback_detected:
        return "Ellenorizd, hogy a Stage-1 tenyleg GPU.1-en, a Stage-2 pedig GPU.0-n fordult-e le, es nincs-e CPU vagy azonos-device fallback.";
    default:
        return "Nezd at a startup logot es a lokal runtime bundle allapotat.";
    }
}

}  // namespace

std::string build_runtime_error_report(
    const masodik::runtime::RuntimeError error,
    const std::string_view detail,
    const masodik::runtime::ServiceConfig& service_config,
    const SplitRuntimeErrorContext split_context) {
    std::ostringstream stream;
    stream << "Runtime hiba: " << masodik::runtime::to_string(error) << " | Profil: " << service_config.active_profile
           << " | Modell: " << service_config.active_model << " | Eszkoz: " << service_config.effective_device;
    if (!detail.empty()) {
        stream << " | Reszlet: " << detail;
    }
    if (split_context.split_mode) {
        stream << " | Split scope: " << masodik::runtime::to_string(split_context.failure_scope)
               << " | Stage-1: " << split_context.stage_1_state
               << " | Stage-2: " << split_context.stage_2_state
               << " | Handoff: " << split_context.handoff_state
               << " | Fallback: " << (split_context.fallback_detected ? "igen" : "nem");
        if (!split_context.last_error_detail.empty()) {
            stream << " | Utolso hiba: " << split_context.last_error_detail;
        }
        if (!split_context.fallback_detected && split_context.failure_scope == masodik::runtime::RuntimeFailureScope::none) {
            stream << " | Proof: futtasd a capture-manual-split-proof.ps1 workflowt a ket-GPU-s operatori bizonyitashoz.";
        }
    }
    stream << " | Teendo: " << remediation_hint(error);
    return stream.str();
}

}  // namespace masodik::diagnostics
