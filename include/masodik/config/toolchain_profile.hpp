#pragma once

#include <filesystem>
#include <string>

#include "masodik/config/json_loader.hpp"

namespace masodik::config {

struct ToolchainProfile {
    std::string compiler_family = "MSVC";
    std::string compiler_version;
    std::filesystem::path cl_path;
    std::string generator = "Ninja";
    std::filesystem::path openvino_cmake_dir;
    std::filesystem::path cmake_path;
    std::filesystem::path ctest_path;
    std::filesystem::path ninja_path;

    void validate() const;
};

ToolchainProfile load_toolchain_profile(const JsonValue::Object& json_object);
ToolchainProfile discover_default_toolchain_profile();

}  // namespace masodik::config
