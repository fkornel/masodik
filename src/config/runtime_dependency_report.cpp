#include "masodik/config/runtime_dependency_report.hpp"

#include <filesystem>

#include "masodik/config/default_paths.hpp"

namespace masodik::config {
namespace {

std::filesystem::path find_first_matching_file(
    const std::filesystem::path& root,
    const std::wstring_view file_name) {
    if (!std::filesystem::exists(root)) {
        return {};
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator{root}) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename().wstring() == file_name) {
            return entry.path();
        }
    }

    return {};
}

}  // namespace

bool RuntimeDependencyReport::ready_for_in_process_runtime() const noexcept {
    return openvino_include_dir_exists && openvino_genai_dll_exists && model_repository_exists;
}

RuntimeDependencyReport inspect_runtime_dependency_report(
    const ToolchainProfile& toolchain,
    const ModelRepository& model_repository) {
    RuntimeDependencyReport report;
    report.openvino_include_dir = default_paths::default_openvino_include_dir;
    report.openvino_runtime_libs_dir = default_paths::default_openvino_runtime_libs_dir;
    report.openvino_genai_dll_path = default_paths::default_openvino_genai_dll_path;
    report.openvino_genai_c_dll_path = default_paths::default_openvino_genai_c_dll_path;
    report.openvino_genai_import_lib_path = std::filesystem::exists(default_paths::bundled_openvino_genai_import_lib_path)
                                                ? default_paths::bundled_openvino_genai_import_lib_path
                                                : find_first_matching_file(
                                                      default_paths::ai_playground_root / "resources" / "OpenVINO",
                                                      L"openvino_genai.lib");

    report.openvino_include_dir_exists = std::filesystem::exists(report.openvino_include_dir);
    report.openvino_runtime_libs_dir_exists = std::filesystem::exists(report.openvino_runtime_libs_dir);
    report.openvino_genai_dll_exists = std::filesystem::exists(report.openvino_genai_dll_path);
    report.openvino_genai_c_dll_exists = std::filesystem::exists(report.openvino_genai_c_dll_path);
    report.openvino_genai_import_lib_exists =
        !report.openvino_genai_import_lib_path.empty() && std::filesystem::exists(report.openvino_genai_import_lib_path);
    report.model_repository_exists = std::filesystem::exists(model_repository.root_path);

    if (!std::filesystem::exists(toolchain.openvino_cmake_dir)) {
        report.notes.emplace_back("Az OpenVINO CMake konyvtar nem erheto el a toolchain profil szerint.");
    }
    if (!report.openvino_include_dir_exists) {
        report.notes.emplace_back("Az OpenVINO fejlesztoi include konyvtar nem talalhato a vart helyen.");
    }
    if (!report.openvino_genai_dll_exists) {
        report.notes.emplace_back("Az openvino_genai.dll nem talalhato a vart helyen.");
    }
    if (!report.openvino_genai_import_lib_exists) {
        report.notes.emplace_back("Az openvino_genai.lib nem talalhato a vizsgalt AI Playground bundle-ben.");
    }
    if (!report.model_repository_exists) {
        report.notes.emplace_back("A modellrepo nem erheto el a megadott root_path alatt.");
    }

    return report;
}

}  // namespace masodik::config
