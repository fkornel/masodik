#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "masodik/config/model_repository.hpp"
#include "masodik/config/runtime_dependency_report.hpp"
#include "masodik/config/serving_profile.hpp"
#include "masodik/config/toolchain_profile.hpp"
#include "masodik/genai/generation_config.hpp"

namespace masodik::runtime {

struct ServiceConfig {
    masodik::config::ToolchainProfile toolchain;
    masodik::config::ModelRepository model_repository;
    masodik::config::ServingProfile serving_profile;
    masodik::config::RuntimeDependencyReport runtime_dependencies;
    masodik::genai::GenerationConfig generation_config;
    std::filesystem::path repo_root;
    bool split_mode = false;
    std::string split_profile;
    std::filesystem::path split_manifest_path;
    std::string active_model;
    std::string active_profile;
    std::string effective_device;
    std::string bind_host;
    std::uint16_t bind_port = 0;
    std::string health_endpoint_path;
    std::string chat_endpoint_path;
    std::string models_endpoint_path;
    std::string health_url;
    std::string chat_url;
    std::string models_url;

    void validate() const;
};

ServiceConfig build_service_config(
    const std::filesystem::path& profile_path,
    const std::filesystem::path& model_root,
    const std::filesystem::path& repo_root);

}  // namespace masodik::runtime
