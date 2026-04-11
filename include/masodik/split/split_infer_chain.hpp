#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <openvino/openvino.hpp>

#include "masodik/config/model_repository.hpp"
#include "masodik/genai/generation_config.hpp"

namespace masodik::split {

enum class SplitRequestPathMode {
    placeholder,
    compiled,
    degraded
};

struct SplitHandoffContract {
    std::string hidden_state_contract;
    std::string kv_contract;
    std::string artifact_version;
    std::string stage_1_device;
    std::string stage_2_device;
    int validated_context_tokens = 0;
};

struct SplitTensorHandoffBuffer {
    std::vector<char> storage;
    ov::Tensor tensor;
    ov::element::Type element_type;
    ov::Shape shape;
    std::size_t byte_size = 0;
    std::uint64_t fingerprint = 0;
    std::string source_contract;
    std::string target_tensor_name;
};

[[nodiscard]] SplitHandoffContract build_split_handoff_contract(
    const masodik::config::SplitArtifactManifest& manifest,
    const masodik::genai::GenerationConfig& generation_config);

void validate_split_handoff_contract(const SplitHandoffContract& contract);

[[nodiscard]] SplitTensorHandoffBuffer make_ram_backed_handoff_buffer(
    const ov::Tensor& source_tensor,
    std::string_view source_contract,
    std::string_view target_tensor_name);

[[nodiscard]] std::string describe_tensor_shape(const ov::Tensor& tensor);

[[nodiscard]] std::size_t layer_range_span(std::string_view layer_range);

[[nodiscard]] SplitRequestPathMode infer_split_request_path_mode(
    bool split_mode,
    bool compiled_request_path_active,
    bool stage_1_placeholder_mode,
    bool stage_2_placeholder_mode,
    bool runtime_failure_detected) noexcept;

[[nodiscard]] std::string_view to_string(SplitRequestPathMode mode) noexcept;

}  // namespace masodik::split
