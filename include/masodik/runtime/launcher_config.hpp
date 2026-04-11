#pragma once

#include <filesystem>
#include <string>

#include "masodik/config/model_repository.hpp"
#include "masodik/config/serving_profile.hpp"
#include "masodik/config/toolchain_profile.hpp"
#include "masodik/runtime/runtime_discovery.hpp"

namespace masodik::runtime {

struct LauncherConfig {
    masodik::config::ToolchainProfile toolchain;
    masodik::config::ModelRepository model_repository;
    masodik::config::ServingProfile serving_profile;
    RuntimeDiscovery runtime_discovery;
    std::filesystem::path repo_root;
    std::string effective_device;
    std::string active_chat_url;
    std::string active_models_url;

    void validate() const;
};

LauncherConfig build_launcher_config(
    const std::filesystem::path& profile_path,
    const std::filesystem::path& model_root,
    const std::filesystem::path& repo_root);

}  // namespace masodik::runtime
