#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "masodik/config/default_paths.hpp"
#include "masodik/split/split_artifact_builder.hpp"

namespace {

struct ToolArguments {
    std::filesystem::path source_model_root = masodik::config::default_paths::default_model_root;
    std::filesystem::path output_root = std::filesystem::path{"build"} / "generated-manual-split-cut23-tail-32k-u8";
    std::string validated_for_profile = "b580-manual-split-cut23-tail-32k-u8";
    std::string artifact_version = "split-v1";
    std::string cut_after_layer = "layers.23 -> tail";
    int max_context_tokens = 32768;
    int validated_context_tokens = 32768;
    double stage_1_memory_budget_gb = 12.0;
    double stage_2_memory_budget_gb = 24.0;
    double stage_1_kv_budget_gb = 3.5;
    double stage_2_kv_budget_gb = 18.0;
    bool clean_output_root = true;
    bool compress_to_fp16 = false;
};

ToolArguments parse_arguments(const int argc, char** argv) {
    ToolArguments arguments;
    for (int index = 1; index < argc; ++index) {
        const std::string current = argv[index];
        auto read_value = [&](std::string_view flag) -> std::string {
            if (index + 1 >= argc) {
                throw std::invalid_argument("Hianyzo ertek a kovetkezo argumentum utan: " + std::string{flag});
            }
            return argv[++index];
        };

        if (current == "--source-model-root") {
            arguments.source_model_root = read_value(current);
        } else if (current == "--output-root") {
            arguments.output_root = read_value(current);
        } else if (current == "--validated-for-profile") {
            arguments.validated_for_profile = read_value(current);
        } else if (current == "--artifact-version") {
            arguments.artifact_version = read_value(current);
        } else if (current == "--cut-after-layer") {
            arguments.cut_after_layer = read_value(current);
        } else if (current == "--max-context-tokens") {
            arguments.max_context_tokens = std::stoi(read_value(current));
        } else if (current == "--validated-context-tokens") {
            arguments.validated_context_tokens = std::stoi(read_value(current));
        } else if (current == "--stage-1-memory-budget-gb") {
            arguments.stage_1_memory_budget_gb = std::stod(read_value(current));
        } else if (current == "--stage-2-memory-budget-gb") {
            arguments.stage_2_memory_budget_gb = std::stod(read_value(current));
        } else if (current == "--stage-1-kv-budget-gb") {
            arguments.stage_1_kv_budget_gb = std::stod(read_value(current));
        } else if (current == "--stage-2-kv-budget-gb") {
            arguments.stage_2_kv_budget_gb = std::stod(read_value(current));
        } else if (current == "--no-clean-output-root") {
            arguments.clean_output_root = false;
        } else if (current == "--compress-to-fp16") {
            arguments.compress_to_fp16 = true;
        } else {
            throw std::invalid_argument("Ismeretlen argumentum: " + current);
        }
    }
    return arguments;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto arguments = parse_arguments(argc, argv);
        const auto result = masodik::split::build_split_artifact({
            .source_model_root = arguments.source_model_root,
            .output_root = arguments.output_root,
            .validated_for_profile = arguments.validated_for_profile,
            .artifact_version = arguments.artifact_version,
            .cut_after_layer = arguments.cut_after_layer,
            .max_context_tokens = arguments.max_context_tokens,
            .validated_context_tokens = arguments.validated_context_tokens,
            .stage_1_memory_budget_gb = arguments.stage_1_memory_budget_gb,
            .stage_2_memory_budget_gb = arguments.stage_2_memory_budget_gb,
            .stage_1_kv_budget_gb = arguments.stage_1_kv_budget_gb,
            .stage_2_kv_budget_gb = arguments.stage_2_kv_budget_gb,
            .clean_output_root = arguments.clean_output_root,
            .compress_to_fp16 = arguments.compress_to_fp16,
        });

        std::cout << "split_artifact_root=" << result.artifact_root.string() << '\n';
        std::cout << "split_manifest_path=" << result.split_manifest_path.string() << '\n';
        std::cout << "cut_after_layer=" << result.manifest.cut_after_layer << '\n';
        std::cout << "stage_1_layer_range=" << result.manifest.stage_1.layer_range << '\n';
        std::cout << "stage_2_layer_range=" << result.manifest.stage_2.layer_range << '\n';
        std::cout << "source_model_checksum=" << result.manifest.source_model_checksum << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
