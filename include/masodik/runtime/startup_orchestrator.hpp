#pragma once

#include <chrono>
#include <string>

#include "masodik/http/chat_contract_validator.hpp"
#include "masodik/http/models_contract_validator.hpp"
#include "masodik/ovms/ovms_process.hpp"
#include "masodik/runtime/health_monitor.hpp"

namespace masodik::runtime {

struct LauncherConfig;

struct StartupValidationResult {
    HealthSnapshot health_snapshot;
    masodik::http::ChatContractValidationResult chat_validation;
    masodik::http::ModelsContractValidationResult models_validation;
};

class StartupOrchestrator {
public:
    StartupOrchestrator(
        HealthMonitor health_monitor = {},
        masodik::http::ChatContractValidator chat_validator = {},
        masodik::http::ModelsContractValidator models_validator = {});

    StartupValidationResult wait_for_readiness_and_validate(
        const LauncherConfig& launcher_config,
        std::chrono::milliseconds startup_timeout) const;

private:
    HealthMonitor health_monitor_;
    masodik::http::ChatContractValidator chat_validator_;
    masodik::http::ModelsContractValidator models_validator_;
};

}  // namespace masodik::runtime
