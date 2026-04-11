#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <openvino/openvino.hpp>

#include "masodik/config/model_repository.hpp"

namespace masodik::split {

struct SplitArtifactBuildOptions {
    std::filesystem::path source_model_root;
    std::filesystem::path output_root;
    std::string validated_for_profile = "b580-manual-split";
    std::string artifact_version = "split-v1";
    std::string cut_after_layer = "layers.17 -> layers.18";
    int max_context_tokens = 131072;
    int validated_context_tokens = 131072;
    double stage_1_memory_budget_gb = 12.0;
    double stage_2_memory_budget_gb = 24.0;
    double stage_1_kv_budget_gb = 3.5;
    double stage_2_kv_budget_gb = 18.0;
    bool clean_output_root = true;
    bool compress_to_fp16 = false;
};

struct SplitBoundaryDiscovery {
    int cut_after_index = -1;
    int next_index = -1;
    int max_layer_index = -1;
    std::string cut_after_layer;
    std::string stage_boundary;
    std::string stage_1_layer_range;
    std::string stage_2_layer_range;
    std::string boundary_output_name;
    std::string boundary_producer_name;
    std::size_t boundary_output_index = 0;
    std::vector<std::string> boundary_consumer_names;
};

struct SplitArtifactBuildResult {
    std::filesystem::path source_model_root;
    std::filesystem::path artifact_root;
    std::filesystem::path split_manifest_path;
    std::filesystem::path stage_1_root;
    std::filesystem::path stage_2_root;
    masodik::config::SplitArtifactManifest manifest;
};

[[nodiscard]] SplitBoundaryDiscovery discover_split_boundary(
    const std::shared_ptr<ov::Model>& source_model,
    std::string_view cut_after_layer);

[[nodiscard]] SplitArtifactBuildResult build_split_artifact(const SplitArtifactBuildOptions& options);

}  // namespace masodik::split
