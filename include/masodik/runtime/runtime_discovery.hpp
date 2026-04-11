#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "masodik/config/model_repository.hpp"
#include "masodik/config/serving_profile.hpp"
#include "masodik/config/toolchain_profile.hpp"

namespace masodik::runtime {

struct RuntimeDiscovery {
    std::filesystem::path ovms_exe_path;
    std::filesystem::path graph_template_path;
    std::filesystem::path runtime_root;
    std::filesystem::path runtime_graph_dir;
    std::filesystem::path runtime_graph_path;
    std::filesystem::path runtime_log_path;
    std::filesystem::path runtime_pid_path;
    std::uint16_t grpc_port = 0;
    std::string ready_url;
    std::string chat_url;
    std::string chat_fallback_url;
    std::string models_url;
    std::string models_fallback_url;

    void validate() const;
};

RuntimeDiscovery discover_runtime(
    const masodik::config::ToolchainProfile& toolchain,
    const masodik::config::ModelRepository& model_repository,
    const masodik::config::ServingProfile& serving_profile,
    const std::filesystem::path& repo_root);

}  // namespace masodik::runtime
