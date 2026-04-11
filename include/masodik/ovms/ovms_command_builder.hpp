#pragma once

#include <filesystem>
#include <string>

#include "masodik/ovms/ovms_process.hpp"

namespace masodik::runtime {
struct LauncherConfig;
}

namespace masodik::ovms {

struct OvmsCommandBuildResult {
    OvmsLaunchOptions launch_options;
    std::filesystem::path materialized_graph_path;
    std::string effective_device;
    std::string plugin_config_json;
};

OvmsCommandBuildResult build_ovms_launch_options(const masodik::runtime::LauncherConfig& launcher_config);

}  // namespace masodik::ovms
