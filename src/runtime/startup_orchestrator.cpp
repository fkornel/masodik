#include "masodik/runtime/startup_orchestrator.hpp"

#include "masodik/runtime/launcher_config.hpp"

namespace masodik::runtime {

StartupOrchestrator::StartupOrchestrator(
    HealthMonitor health_monitor,
    masodik::http::ChatContractValidator chat_validator,
    masodik::http::ModelsContractValidator models_validator)
    : health_monitor_(std::move(health_monitor)),
      chat_validator_(std::move(chat_validator)),
      models_validator_(std::move(models_validator)) {}

StartupValidationResult StartupOrchestrator::wait_for_readiness_and_validate(
    const LauncherConfig& launcher_config,
    const std::chrono::milliseconds startup_timeout) const {
    StartupValidationResult result;
    result.health_snapshot = health_monitor_.wait_until_ready(
        launcher_config.runtime_discovery.ready_url,
        startup_timeout,
        launcher_config.serving_profile.profile_name,
        launcher_config.model_repository.model_id);

    if (!result.health_snapshot.ready) {
        return result;
    }

    result.models_validation = models_validator_.validate(
        launcher_config.runtime_discovery.models_url,
        launcher_config.runtime_discovery.models_fallback_url,
        launcher_config.serving_profile.model_name,
        std::chrono::milliseconds{30000});

    result.chat_validation = chat_validator_.validate(
        launcher_config.runtime_discovery.chat_url,
        launcher_config.runtime_discovery.chat_fallback_url,
        launcher_config.serving_profile.model_name,
        std::chrono::milliseconds{30000});

    return result;
}

}  // namespace masodik::runtime
