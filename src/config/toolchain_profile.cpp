#include "masodik/config/toolchain_profile.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "masodik/config/default_paths.hpp"

namespace masodik::config {
namespace {

std::filesystem::path find_latest_toolset_path() {
    std::vector<std::filesystem::directory_entry> entries;
    if (std::filesystem::exists(default_paths::msvc_tools_root)) {
        for (const auto& entry : std::filesystem::directory_iterator{default_paths::msvc_tools_root}) {
            if (entry.is_directory()) {
                entries.push_back(entry);
            }
        }
    }

    if (entries.empty()) {
        return default_paths::default_cl_path.parent_path().parent_path().parent_path().parent_path();
    }

    std::ranges::sort(entries, [](const auto& left, const auto& right) {
        return left.path().filename().wstring() > right.path().filename().wstring();
    });
    return entries.front().path();
}

std::string infer_compiler_version(const std::filesystem::path& cl_path) {
    const auto toolset_path = cl_path.parent_path().parent_path().parent_path().parent_path();
    return toolset_path.filename().string();
}

}  // namespace

void ToolchainProfile::validate() const {
    if (compiler_family != "MSVC") {
        throw std::invalid_argument("A v1-ben csak az MSVC toolchain tamogatott.");
    }
    if (cl_path.empty() || !std::filesystem::exists(cl_path)) {
        throw std::invalid_argument("A cl.exe utvonala nem ervenyes.");
    }
    if (openvino_cmake_dir.empty() || !std::filesystem::exists(openvino_cmake_dir)) {
        throw std::invalid_argument("Az OpenVINO CMake konyvtar nem talalhato.");
    }
    if (cmake_path.empty() || !std::filesystem::exists(cmake_path)) {
        throw std::invalid_argument("A cmake.exe utvonala nem talalhato.");
    }
    if (ctest_path.empty() || !std::filesystem::exists(ctest_path)) {
        throw std::invalid_argument("A ctest.exe utvonala nem talalhato.");
    }
    if (ninja_path.empty() || !std::filesystem::exists(ninja_path)) {
        throw std::invalid_argument("A ninja.exe utvonala nem talalhato.");
    }
    if (generator.empty()) {
        throw std::invalid_argument("A generator nem lehet ures.");
    }
}

ToolchainProfile load_toolchain_profile(const JsonValue::Object& json_object) {
    ToolchainProfile profile;
    profile.compiler_family = get_string_or(json_object, "compiler_family", profile.compiler_family);
    profile.compiler_version = get_string_or(json_object, "compiler_version", profile.compiler_version);
    profile.cl_path = get_string(json_object, "cl_path");
    profile.generator = get_string_or(json_object, "generator", profile.generator);
    profile.openvino_cmake_dir = get_string(json_object, "openvino_cmake_dir");
    profile.cmake_path = get_string_or(json_object, "cmake_path", default_paths::default_cmake_path.string());
    profile.ctest_path = get_string_or(json_object, "ctest_path", default_paths::default_ctest_path.string());
    profile.ninja_path = get_string_or(json_object, "ninja_path", default_paths::default_ninja_path.string());
    if (profile.compiler_version.empty()) {
        profile.compiler_version = infer_compiler_version(profile.cl_path);
    }
    profile.validate();
    return profile;
}

ToolchainProfile discover_default_toolchain_profile() {
    const auto toolset_path = find_latest_toolset_path();
    ToolchainProfile profile;
    profile.cl_path = toolset_path / "bin" / "Hostx64" / "x64" / "cl.exe";
    profile.compiler_version = toolset_path.filename().string();
    profile.generator = std::string{default_paths::default_generator};
    profile.openvino_cmake_dir = default_paths::default_openvino_cmake_dir;
    profile.cmake_path = default_paths::default_cmake_path;
    profile.ctest_path = default_paths::default_ctest_path;
    profile.ninja_path = default_paths::default_ninja_path;
    profile.validate();
    return profile;
}

}  // namespace masodik::config
