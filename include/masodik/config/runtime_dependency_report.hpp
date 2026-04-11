#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "masodik/config/model_repository.hpp"
#include "masodik/config/toolchain_profile.hpp"

namespace masodik::config {

struct RuntimeDependencyReport {
    std::filesystem::path openvino_include_dir;
    std::filesystem::path openvino_runtime_libs_dir;
    std::filesystem::path openvino_genai_dll_path;
    std::filesystem::path openvino_genai_c_dll_path;
    std::filesystem::path openvino_genai_import_lib_path;
    bool openvino_include_dir_exists = false;
    bool openvino_runtime_libs_dir_exists = false;
    bool openvino_genai_dll_exists = false;
    bool openvino_genai_c_dll_exists = false;
    bool openvino_genai_import_lib_exists = false;
    bool model_repository_exists = false;
    std::vector<std::string> notes;

    [[nodiscard]] bool ready_for_in_process_runtime() const noexcept;
};

RuntimeDependencyReport inspect_runtime_dependency_report(
    const ToolchainProfile& toolchain,
    const ModelRepository& model_repository);

}  // namespace masodik::config
