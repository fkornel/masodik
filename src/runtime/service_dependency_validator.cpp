#include "masodik/runtime/service_dependency_validator.hpp"

#include <sstream>
#include <stdexcept>

namespace masodik::runtime {

ServiceDependencyValidation ServiceDependencyValidator::validate(const ServiceConfig& service_config) const {
    ServiceDependencyValidation validation;
    validation.valid = true;

    try {
        service_config.validate();
    } catch (const std::exception& error) {
        validation.valid = false;
        validation.error = RuntimeError::invalid_configuration;
        validation.notes.push_back(error.what());
        return validation;
    }

    const auto& runtime_dependencies = service_config.runtime_dependencies;
    if (!runtime_dependencies.openvino_include_dir_exists) {
        validation.valid = false;
        validation.error = RuntimeError::missing_toolchain;
        validation.notes.emplace_back("Az OpenVINO include konyvtar nem erheto el.");
    }
    if (!runtime_dependencies.openvino_runtime_libs_dir_exists) {
        validation.valid = false;
        validation.error = RuntimeError::missing_toolchain;
        validation.notes.emplace_back("Az OpenVINO runtime DLL konyvtar nem erheto el.");
    }
    if (!runtime_dependencies.openvino_genai_dll_exists || !runtime_dependencies.openvino_genai_c_dll_exists) {
        validation.valid = false;
        validation.error = RuntimeError::missing_toolchain;
        validation.notes.emplace_back("Az OpenVINO GenAI runtime DLL keszlet hianyos.");
    }
    if (!runtime_dependencies.model_repository_exists) {
        validation.valid = false;
        validation.error = RuntimeError::missing_model;
        validation.notes.emplace_back("A lokal modellrepo nem erheto el.");
    }

    validation.notes.insert(
        validation.notes.end(),
        runtime_dependencies.notes.begin(),
        runtime_dependencies.notes.end());

    const auto split_validation = validate_split_dependencies(service_config);
    if (!split_validation.valid) {
        validation.valid = false;
        validation.error = split_validation.error;
        validation.failure_scope = split_validation.failure_scope;
        validation.notes.insert(validation.notes.end(), split_validation.notes.begin(), split_validation.notes.end());
    }
    return validation;
}

ServiceDependencyValidation ServiceDependencyValidator::validate_split_dependencies(const ServiceConfig& service_config) const {
    ServiceDependencyValidation validation;
    validation.valid = true;
    if (!service_config.split_mode) {
        return validation;
    }

    validation.failure_scope = RuntimeFailureScope::startup;
    if (!service_config.model_repository.split_mode) {
        validation.valid = false;
        validation.error = RuntimeError::split_manifest_invalid;
        validation.notes.emplace_back("Split profilhoz split-aware model repository szukseges.");
    }
    if (service_config.split_manifest_path.empty()) {
        validation.valid = false;
        validation.error = RuntimeError::split_manifest_invalid;
        validation.notes.emplace_back("A split manifest eleresi utja hianyzik.");
    }
    if (service_config.generation_config.stage_1_device == service_config.generation_config.stage_2_device) {
        validation.valid = false;
        validation.error = RuntimeError::split_stage_configuration;
        validation.notes.emplace_back("A split stage-ek nem futhatnak ugyanazon az eszkozazonositon.");
    }
    if (!service_config.generation_config.reject_cpu_fallback) {
        validation.valid = false;
        validation.error = RuntimeError::split_fallback_detected;
        validation.notes.emplace_back("A split profilban a CPU fallback tiltasa kotelezo.");
    }
    if (service_config.model_repository.split_mode &&
        service_config.model_repository.split_artifact_manifest.validated_for_profile != service_config.active_profile) {
        validation.valid = false;
        validation.error = RuntimeError::split_manifest_invalid;
        validation.notes.emplace_back("A split manifest validated_for_profile mezoje nem egyezik az aktiv profillal.");
    }
    return validation;
}

void ServiceDependencyValidator::ensure_valid(const ServiceConfig& service_config) const {
    const auto validation = validate(service_config);
    if (validation.valid) {
        return;
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < validation.notes.size(); ++index) {
        if (index > 0) {
            stream << " | ";
        }
        stream << validation.notes[index];
    }

    throw std::runtime_error(stream.str().empty() ? "A startup dependency validacio sikertelen." : stream.str());
}

}  // namespace masodik::runtime
