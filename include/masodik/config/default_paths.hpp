#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace masodik::config::default_paths {

inline const std::filesystem::path visual_studio_build_tools_root =
    LR"(C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools)";
inline const std::filesystem::path msvc_tools_root =
    visual_studio_build_tools_root / "VC" / "Tools" / "MSVC";
inline const std::filesystem::path default_cl_path =
    msvc_tools_root / "14.50.35717" / "bin" / "Hostx64" / "x64" / "cl.exe";
inline const std::filesystem::path default_cmake_path =
    visual_studio_build_tools_root / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" /
    "CMake" / "bin" / "cmake.exe";
inline const std::filesystem::path default_ctest_path =
    visual_studio_build_tools_root / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" /
    "CMake" / "bin" / "ctest.exe";
inline const std::filesystem::path default_ninja_path =
    visual_studio_build_tools_root / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" /
    "Ninja" / "ninja.exe";
inline const std::filesystem::path default_vsdevcmd_path =
    visual_studio_build_tools_root / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";

inline const std::filesystem::path ai_playground_root =
    LR"(C:\Users\jezus\AppData\Local\Programs\AI Playground)";
inline const std::filesystem::path bundled_openvino_genai_root =
    LR"(C:\dev\copilot\masodik\openvino_genai_windows_2026.1.0.0rc2_x86_64)";
inline const std::filesystem::path bundled_openvino_runtime_root = bundled_openvino_genai_root / "runtime";
inline const std::filesystem::path bundled_openvino_cmake_dir = bundled_openvino_runtime_root / "cmake";
inline const std::filesystem::path bundled_openvino_include_dir = bundled_openvino_runtime_root / "include";
inline const std::filesystem::path bundled_openvino_runtime_bin_dir =
    bundled_openvino_runtime_root / "bin" / "intel64" / "Release";
inline const std::filesystem::path bundled_openvino_genai_dll_path =
    bundled_openvino_runtime_bin_dir / "openvino_genai.dll";
inline const std::filesystem::path bundled_openvino_genai_c_dll_path =
    bundled_openvino_runtime_bin_dir / "openvino_genai_c.dll";
inline const std::filesystem::path bundled_openvino_genai_import_lib_path =
    bundled_openvino_runtime_root / "lib" / "intel64" / "Release" / "openvino_genai.lib";
inline const std::filesystem::path default_openvino_cmake_dir =
    std::filesystem::exists(bundled_openvino_cmake_dir)
        ? bundled_openvino_cmake_dir
        : ai_playground_root / "resources" / "OpenVINO" / ".venv" / "Lib" / "site-packages" / "openvino" / "cmake";
inline const std::filesystem::path default_openvino_site_packages_root =
    ai_playground_root / "resources" / "OpenVINO" / ".venv" / "Lib" / "site-packages" / "openvino";
inline const std::filesystem::path default_openvino_include_dir =
    std::filesystem::exists(bundled_openvino_include_dir)
        ? bundled_openvino_include_dir
        : default_openvino_site_packages_root / "include";
inline const std::filesystem::path default_openvino_runtime_libs_dir =
    std::filesystem::exists(bundled_openvino_runtime_bin_dir)
        ? bundled_openvino_runtime_bin_dir
        : default_openvino_site_packages_root / "libs";
inline const std::filesystem::path default_ovms_root =
    ai_playground_root / "resources" / "OpenVINO" / "ovms";
inline const std::filesystem::path default_ovms_exe_path = default_ovms_root / "ovms.exe";
inline const std::filesystem::path default_openvino_genai_dll_path =
    std::filesystem::exists(bundled_openvino_genai_dll_path)
        ? bundled_openvino_genai_dll_path
        : default_ovms_root / "openvino_genai.dll";
inline const std::filesystem::path default_openvino_genai_c_dll_path =
    std::filesystem::exists(bundled_openvino_genai_c_dll_path)
        ? bundled_openvino_genai_c_dll_path
        : default_ovms_root / "openvino_genai_c.dll";
inline const std::filesystem::path default_runtime_log_path = std::filesystem::path{"build"} / "logs" / "masodik-runtime.log";
inline const std::filesystem::path default_cache_directory = std::filesystem::path{"build"} / "cache";
inline const std::filesystem::path default_backend_versions_path =
    ai_playground_root / "resources" / "backend-versions.json";
inline const std::filesystem::path default_model_root =
    ai_playground_root / "resources" / "models" / "LLM" / "openvino" / "OpenVINO---gpt-oss-20b-int4-ov";

inline constexpr std::string_view default_generator = "Ninja";
inline constexpr std::string_view default_model_id = "gpt-oss-20b-int4-ov";
inline constexpr std::string_view default_bind_host = "127.0.0.1";
inline constexpr std::uint16_t default_bind_port = 29000;
inline constexpr std::string_view default_health_endpoint_path = "/health";
inline constexpr std::string_view default_chat_endpoint_path = "/v1/chat/completions";
inline constexpr std::string_view default_models_endpoint_path = "/v1/models";
inline constexpr int default_timeout_seconds = 14400;
inline constexpr int default_context_tokens = 32768;
inline constexpr int default_max_num_seqs = 256;
inline constexpr std::size_t default_max_queued_requests = 32;
inline constexpr std::size_t default_max_concurrent_requests = 5;
inline constexpr int default_queue_wait_seconds = 14400;
inline constexpr int default_poll_interval_millis = 500;

}  // namespace masodik::config::default_paths
