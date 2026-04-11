#include "masodik/runtime/launcher_config.hpp"

#include <stdexcept>

namespace masodik::runtime {
namespace {

std::string build_effective_device(const masodik::config::ServingProfile& serving_profile) {
    if (serving_profile.split_mode) {
        return "MANUAL_SPLIT:" + serving_profile.split_stage_1_device + "->" + serving_profile.split_stage_2_device;
    }
    if (!serving_profile.hetero_enabled) {
        return serving_profile.target_device;
    }
    if (serving_profile.target_device.rfind("HETERO:", 0) == 0) {
        return serving_profile.target_device;
    }
    return "HETERO:" + serving_profile.target_device + "," + serving_profile.hetero_fallback_device;
}

}  // namespace

void LauncherConfig::validate() const {
    toolchain.validate();
    model_repository.validate();
    serving_profile.validate();
    runtime_discovery.validate();
    if (effective_device.empty()) {
        throw std::invalid_argument("Az effective_device nem lehet ures.");
    }
}

LauncherConfig build_launcher_config(
    const std::filesystem::path& profile_path,
    const std::filesystem::path& model_root,
    const std::filesystem::path& repo_root) {
    LauncherConfig config;
    config.repo_root = repo_root;
    config.toolchain = masodik::config::discover_default_toolchain_profile();
    config.serving_profile = masodik::config::load_serving_profile(profile_path);
    config.model_repository = masodik::config::inspect_model_repository(
        model_root,
        "AI Playground",
        true,
        config.serving_profile.split_mode,
        config.serving_profile.split_manifest_path);
    config.runtime_discovery = discover_runtime(config.toolchain, config.model_repository, config.serving_profile, repo_root);
    config.effective_device = build_effective_device(config.serving_profile);
    config.active_chat_url = config.runtime_discovery.chat_url;
    config.active_models_url = config.runtime_discovery.models_url;
    config.validate();
    return config;
}

}  // namespace masodik::runtime
