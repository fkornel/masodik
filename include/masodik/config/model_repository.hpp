#pragma once

#include <filesystem>
#include <string>

namespace masodik::config {

struct SplitStageManifest {
    std::string device_role;
    std::filesystem::path artifact_path;
    std::string layer_range;
    std::string expected_input;
    std::string expected_output;
    double memory_budget_gb = 0.0;
    double kv_budget_gb = 0.0;

    void validate(const std::filesystem::path& split_root, std::string_view stage_name) const;
};

struct SplitArtifactManifest {
    std::string model_name;
    std::string artifact_version;
    std::string source_model_checksum;
    int stage_count = 0;
    std::string stage_boundary;
    std::string cut_after_layer;
    std::filesystem::path tokenizer_path;
    int max_context_tokens = 0;
    int validated_context_tokens = 0;
    std::string hidden_state_contract;
    std::string kv_contract;
    std::string validated_for_profile;
    SplitStageManifest stage_1;
    SplitStageManifest stage_2;

    void validate(const std::filesystem::path& split_root) const;
};

struct ModelRepository {
    std::string model_id;
    std::filesystem::path root_path;
    std::filesystem::path model_xml_path;
    std::filesystem::path model_bin_path;
    std::filesystem::path tokenizer_json_path;
    std::filesystem::path graph_path;
    bool split_mode = false;
    std::filesystem::path split_root_path;
    std::filesystem::path split_manifest_path;
    SplitArtifactManifest split_artifact_manifest;
    std::string source = "AI Playground";
    bool read_only = true;

    void validate() const;
};

SplitArtifactManifest load_split_artifact_manifest(
    const std::filesystem::path& split_root,
    const std::filesystem::path& manifest_path = {});

ModelRepository inspect_model_repository(
    const std::filesystem::path& root_path,
    std::string source = "AI Playground",
    bool read_only = true,
    bool expect_split = false,
    const std::filesystem::path& split_manifest_path = {});

}  // namespace masodik::config
