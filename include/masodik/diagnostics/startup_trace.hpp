#pragma once

#include "masodik/diagnostics/logger.hpp"
#include "masodik/runtime/launcher_config.hpp"
#include "masodik/runtime/startup_orchestrator.hpp"

namespace masodik::diagnostics {

void trace_startup_plan(Logger& logger, const masodik::runtime::LauncherConfig& launcher_config);
void trace_validation(Logger& logger, const masodik::runtime::StartupValidationResult& validation_result);

}  // namespace masodik::diagnostics
