#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <openvino/core/graph_util.hpp>
#include <openvino/op/assign.hpp>
#include <openvino/op/read_value.hpp>
#include <openvino/op/util/variable.hpp>
#include <openvino/opsets/opset13.hpp>

#include "test_registry.hpp"

namespace masodik::tests {

struct ManualSplitFixtureOptions {
    std::string name;
    std::string profile_name = "b580-manual-split";
    std::uint16_t bind_port = 29131;
    bool create_stage_1 = true;
    bool create_stage_2 = true;
    int manifest_max_context_tokens = 32768;
    int validated_context_tokens = 32768;
    int profile_desired_context_tokens = 32768;
    std::string validated_for_profile = "b580-manual-split";
    std::string stage_1_device = "GPU.1";
    std::string stage_2_device = "GPU.0";
    std::string source_model_checksum = "sha256:test-fixture";
    std::string cut_after_layer = "layers.17 -> layers.18";
    std::string stage_boundary = "transformer layers 0-17 | 18-43";
    std::string stage_1_layer_range = "0-17";
    std::string stage_2_layer_range = "18-43";
    std::string stage_1_expected_output = "hidden states + kv delta";
    std::string stage_2_expected_input = "hidden states + kv delta";
    std::string stage_1_device_role = "primary";
    std::string stage_2_device_role = "secondary";
    double stage_1_memory_budget_gb = 12.0;
    double stage_2_memory_budget_gb = 24.0;
    double stage_1_kv_budget_gb = 3.5;
    double stage_2_kv_budget_gb = 18.0;
};

struct ManualSplitFixturePaths {
    std::filesystem::path root_path;
    std::filesystem::path manifest_path;
    std::filesystem::path profile_path;
    std::filesystem::path stage_1_path;
    std::filesystem::path stage_2_path;
};

struct ManualSplitSourceFixturePaths {
    std::filesystem::path root_path;
    std::filesystem::path model_xml_path;
    std::filesystem::path tokenizer_json_path;
};

inline void write_text_file(const std::filesystem::path& path, const std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        throw std::runtime_error("A teszt nem tudta megnyitni a fajlt: " + path.string());
    }
    stream << content;
}

inline ManualSplitFixturePaths create_manual_split_fixture(const ManualSplitFixtureOptions& options) {
    if (options.name.empty()) {
        throw std::invalid_argument("A manual split teszt fixture neve kotelezo.");
    }

    const auto repo_root = find_repo_root();
    const auto root = repo_root / "build" / "test-fixtures" / options.name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto stage_1_path = root / "stage-1";
    const auto stage_2_path = root / "stage-2";
    const auto tokenizer_path = root / "tokenizer";
    std::filesystem::create_directories(tokenizer_path);
    write_text_file(tokenizer_path / "tokenizer.json", "{}\n");

    if (options.create_stage_1) {
        std::filesystem::create_directories(stage_1_path);
        write_text_file(stage_1_path / "README.txt", "manual split stage-1 fixture\n");
    }
    if (options.create_stage_2) {
        std::filesystem::create_directories(stage_2_path);
        write_text_file(stage_2_path / "README.txt", "manual split stage-2 fixture\n");
    }

    const auto manifest_path = root / "split-manifest.json";
    const auto profile_path = root / "manual-split-profile.json";
    std::ostringstream manifest;
    manifest
        << "{\n"
        << "  \"model_name\": \"gpt-oss-20b-int4-ov\",\n"
        << "  \"artifact_version\": \"split-v1\",\n"
        << "  \"source_model_checksum\": \"" << options.source_model_checksum << "\",\n"
        << "  \"stage_count\": 2,\n"
        << "  \"stage_boundary\": \"" << options.stage_boundary << "\",\n"
        << "  \"cut_after_layer\": \"" << options.cut_after_layer << "\",\n"
        << "  \"tokenizer_path\": \"tokenizer/tokenizer.json\",\n"
        << "  \"max_context_tokens\": " << options.manifest_max_context_tokens << ",\n"
        << "  \"validated_context_tokens\": " << options.validated_context_tokens << ",\n"
        << "  \"hidden_state_contract\": \"hs-v1\",\n"
        << "  \"kv_contract\": \"kv-v1\",\n"
        << "  \"validated_for_profile\": \"" << options.validated_for_profile << "\",\n"
        << "  \"stage_1\": {\n"
        << "    \"device_role\": \"" << options.stage_1_device_role << "\",\n"
        << "    \"artifact_path\": \"stage-1\",\n"
        << "    \"layer_range\": \"" << options.stage_1_layer_range << "\",\n"
        << "    \"expected_input\": \"token ids + kv context\",\n"
        << "    \"expected_output\": \"" << options.stage_1_expected_output << "\",\n"
        << "    \"memory_budget_gb\": " << options.stage_1_memory_budget_gb << ",\n"
        << "    \"kv_budget_gb\": " << options.stage_1_kv_budget_gb << "\n"
        << "  },\n"
        << "  \"stage_2\": {\n"
        << "    \"device_role\": \"" << options.stage_2_device_role << "\",\n"
        << "    \"artifact_path\": \"stage-2\",\n"
        << "    \"layer_range\": \"" << options.stage_2_layer_range << "\",\n"
        << "    \"expected_input\": \"" << options.stage_2_expected_input << "\",\n"
        << "    \"expected_output\": \"logits + updated kv state\",\n"
        << "    \"memory_budget_gb\": " << options.stage_2_memory_budget_gb << ",\n"
        << "    \"kv_budget_gb\": " << options.stage_2_kv_budget_gb << "\n"
        << "  }\n"
        << "}\n";

    std::ostringstream profile;
    profile
        << "{\n"
        << "  \"profile_name\": \"" << options.profile_name << "\",\n"
        << "  \"model_name\": \"gpt-oss-20b-int4-ov\",\n"
        << "  \"primary_device\": \"" << options.stage_1_device << "\",\n"
        << "  \"hybrid_enabled\": false,\n"
        << "  \"hybrid_fallback_device\": \"CPU\",\n"
        << "  \"split_mode\": true,\n"
        << "  \"split_stage_1_device\": \"" << options.stage_1_device << "\",\n"
        << "  \"split_stage_2_device\": \"" << options.stage_2_device << "\",\n"
        << "  \"reject_cpu_fallback\": true,\n"
        << "  \"split_manifest_path\": \"split-manifest.json\",\n"
        << "  \"split_diagnostics_level\": \"verbose\",\n"
        << "  \"split_stage_1_memory_budget_gb\": " << options.stage_1_memory_budget_gb << ",\n"
        << "  \"split_stage_2_memory_budget_gb\": " << options.stage_2_memory_budget_gb << ",\n"
        << "  \"cache_directory\": \"build\\\\cache\\\\manual-split-tests\",\n"
        << "  \"runtime_log_path\": \"build\\\\logs\\\\manual-split-tests.log\",\n"
        << "  \"health_endpoint_path\": \"/health\",\n"
        << "  \"chat_endpoint_path\": \"/v1/chat/completions\",\n"
        << "  \"models_endpoint_path\": \"/v1/models\",\n"
        << "  \"bind_host\": \"127.0.0.1\",\n"
        << "  \"bind_port\": " << options.bind_port << ",\n"
        << "  \"desired_context_tokens\": " << options.profile_desired_context_tokens << ",\n"
        << "  \"max_num_seqs\": 1,\n"
        << "  \"prefix_caching_enabled\": true,\n"
        << "  \"enable_runtime_assignment_diagnostics\": false,\n"
        << "  \"cpu_fallback_enabled\": false,\n"
        << "  \"timeout_seconds\": 600,\n"
        << "  \"queue_policy\": {\n"
        << "    \"max_queued_requests\": 1,\n"
        << "    \"max_concurrent_requests\": 1,\n"
        << "    \"overflow_behavior\": \"reject_new\",\n"
        << "    \"max_queue_wait_seconds\": 5,\n"
        << "    \"poll_interval_millis\": 100\n"
        << "  },\n"
        << "  \"notes\": \"Manual split tesztprofil.\"\n"
        << "}\n";

    write_text_file(manifest_path, manifest.str());
    write_text_file(profile_path, profile.str());

    return {
        .root_path = root,
        .manifest_path = manifest_path,
        .profile_path = profile_path,
        .stage_1_path = stage_1_path,
        .stage_2_path = stage_2_path};
}

inline std::filesystem::path write_manual_split_profile_for_root(
    const std::filesystem::path& split_root,
    const std::string_view profile_name = "b580-manual-split",
    const std::uint16_t bind_port = 29171,
    const int desired_context_tokens = 32768,
    const double stage_1_memory_budget_gb = 12.0,
    const double stage_2_memory_budget_gb = 24.0) {
    const auto profile_path = split_root / "manual-split-profile.json";
    std::ostringstream profile;
    profile
        << "{\n"
        << "  \"profile_name\": \"" << profile_name << "\",\n"
        << "  \"model_name\": \"gpt-oss-20b-int4-ov\",\n"
        << "  \"primary_device\": \"GPU.1\",\n"
        << "  \"hybrid_enabled\": false,\n"
        << "  \"hybrid_fallback_device\": \"CPU\",\n"
        << "  \"split_mode\": true,\n"
        << "  \"split_stage_1_device\": \"GPU.1\",\n"
        << "  \"split_stage_2_device\": \"GPU.0\",\n"
        << "  \"reject_cpu_fallback\": true,\n"
        << "  \"split_manifest_path\": \"split-manifest.json\",\n"
        << "  \"split_diagnostics_level\": \"verbose\",\n"
        << "  \"split_stage_1_memory_budget_gb\": " << stage_1_memory_budget_gb << ",\n"
        << "  \"split_stage_2_memory_budget_gb\": " << stage_2_memory_budget_gb << ",\n"
        << "  \"cache_directory\": \"build\\\\cache\\\\manual-split-generated-tests\",\n"
        << "  \"runtime_log_path\": \"build\\\\logs\\\\manual-split-generated-tests.log\",\n"
        << "  \"health_endpoint_path\": \"/health\",\n"
        << "  \"chat_endpoint_path\": \"/v1/chat/completions\",\n"
        << "  \"models_endpoint_path\": \"/v1/models\",\n"
        << "  \"bind_host\": \"127.0.0.1\",\n"
        << "  \"bind_port\": " << bind_port << ",\n"
        << "  \"desired_context_tokens\": " << desired_context_tokens << ",\n"
        << "  \"max_num_seqs\": 1,\n"
        << "  \"prefix_caching_enabled\": true,\n"
        << "  \"enable_runtime_assignment_diagnostics\": false,\n"
        << "  \"cpu_fallback_enabled\": false,\n"
        << "  \"timeout_seconds\": 600,\n"
        << "  \"queue_policy\": {\n"
        << "    \"max_queued_requests\": 1,\n"
        << "    \"max_concurrent_requests\": 1,\n"
        << "    \"overflow_behavior\": \"reject_new\",\n"
        << "    \"max_queue_wait_seconds\": 5,\n"
        << "    \"poll_interval_millis\": 100\n"
        << "  },\n"
        << "  \"notes\": \"Generated manual split tesztprofil.\"\n"
        << "}\n";
    write_text_file(profile_path, profile.str());
    return profile_path;
}

inline ManualSplitSourceFixturePaths create_manual_split_source_fixture(const std::string_view name) {
    if (name.empty()) {
        throw std::invalid_argument("A manual split source fixture neve kotelezo.");
    }

    const auto repo_root = find_repo_root();
    const auto root = repo_root / "build" / "test-fixtures" / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    auto input_ids = std::make_shared<ov::opset13::Parameter>(ov::element::f32, ov::Shape{1, 4, 8});
    input_ids->set_friendly_name("input_ids");

    auto zero_init = ov::opset13::Constant::create(ov::element::f32, ov::Shape{1, 4, 8}, {0.0f});
    auto one_init = ov::opset13::Constant::create(ov::element::f32, ov::Shape{1, 4, 8}, {1.0f});
    auto stage_17 = std::make_shared<ov::opset13::Add>(input_ids, zero_init);
    stage_17->set_friendly_name("__module.model.layers.17.output/Add");
    auto stage_18 = std::make_shared<ov::opset13::Add>(stage_17, one_init);
    stage_18->set_friendly_name("__module.model.layers.18.output/Add");

    auto logits = std::make_shared<ov::opset13::Result>(stage_18);
    logits->set_friendly_name("logits");

    auto model = std::make_shared<ov::Model>(
        ov::ResultVector{logits},
        ov::ParameterVector{input_ids},
        "manual_split_source_fixture");

    const auto model_xml_path = root / "openvino_model.xml";
    ov::save_model(model, model_xml_path, false);
    write_text_file(root / "tokenizer.json", "{}\n");
    write_text_file(root / "graph.pbtxt", "graph {}\n");

    return {
        .root_path = root,
        .model_xml_path = model_xml_path,
        .tokenizer_json_path = root / "tokenizer.json"};
}

}  // namespace masodik::tests
