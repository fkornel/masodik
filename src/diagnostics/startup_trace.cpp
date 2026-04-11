#include "masodik/diagnostics/startup_trace.hpp"

namespace masodik::diagnostics {

void trace_startup_plan(Logger& logger, const masodik::runtime::LauncherConfig& launcher_config) {
    logger.info("Launcher startup workflow indul.");
    logger.info(std::string{"Profil: "} + launcher_config.serving_profile.profile_name);
    logger.info(std::string{"Effektiv device: "} + launcher_config.effective_device);
    logger.info(std::string{"OVMS: "} + launcher_config.runtime_discovery.ovms_exe_path.string());
    logger.info(std::string{"Runtime graph: "} + launcher_config.runtime_discovery.runtime_graph_path.string());
    logger.info(std::string{"Ready URL: "} + launcher_config.runtime_discovery.ready_url);
    logger.info(std::string{"Chat URL: "} + launcher_config.runtime_discovery.chat_url);
    logger.info(std::string{"Models URL: "} + launcher_config.runtime_discovery.models_url);
}

void trace_validation(Logger& logger, const masodik::runtime::StartupValidationResult& validation_result) {
    logger.info(
        std::string{"Ready allapot: "} + (validation_result.health_snapshot.ready ? "igen" : "nem"));
    if (!validation_result.health_snapshot.last_error_message.empty()) {
        logger.warning(std::string{"Health hiba: "} + validation_result.health_snapshot.last_error_message);
    }
    if (validation_result.models_validation.valid) {
        logger.info(std::string{"Models endpoint validalt: "} + validation_result.models_validation.endpoint_url);
    } else if (!validation_result.models_validation.error_message.empty()) {
        logger.warning(std::string{"Models endpoint hiba: "} + validation_result.models_validation.error_message);
    }
    if (validation_result.chat_validation.valid) {
        logger.info(std::string{"Chat endpoint validalt: "} + validation_result.chat_validation.endpoint_url);
    } else if (!validation_result.chat_validation.error_message.empty()) {
        logger.warning(std::string{"Chat endpoint hiba: "} + validation_result.chat_validation.error_message);
    }
}

}  // namespace masodik::diagnostics
