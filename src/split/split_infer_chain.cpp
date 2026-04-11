#include "masodik/split/split_infer_chain.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace masodik::split {
namespace {

constexpr std::uint64_t kFNVOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t kFNVPrime = 1099511628211ull;

std::string lowercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool is_cpu_device(const std::string& device_name) {
    return lowercase_ascii(device_name).find("cpu") != std::string::npos;
}

std::uint64_t tensor_fingerprint(const ov::Tensor& tensor) {
    std::uint64_t hash = kFNVOffsetBasis;
    const auto* bytes = static_cast<const std::uint8_t*>(tensor.data());
    for (std::size_t index = 0; index < tensor.get_byte_size(); ++index) {
        hash ^= bytes[index];
        hash *= kFNVPrime;
    }
    return hash;
}

}  // namespace

SplitHandoffContract build_split_handoff_contract(
    const masodik::config::SplitArtifactManifest& manifest,
    const masodik::genai::GenerationConfig& generation_config) {
    return {
        .hidden_state_contract = manifest.hidden_state_contract,
        .kv_contract = manifest.kv_contract,
        .artifact_version = manifest.artifact_version,
        .stage_1_device = generation_config.stage_1_device,
        .stage_2_device = generation_config.stage_2_device,
        .validated_context_tokens = manifest.validated_context_tokens,
    };
}

void validate_split_handoff_contract(const SplitHandoffContract& contract) {
    if (contract.hidden_state_contract.empty()) {
        throw std::invalid_argument("A split hidden_state_contract nem lehet ures.");
    }
    if (contract.kv_contract.empty()) {
        throw std::invalid_argument("A split kv_contract nem lehet ures.");
    }
    if (contract.artifact_version.empty()) {
        throw std::invalid_argument("A split artifact_version nem lehet ures.");
    }
    if (contract.stage_1_device.empty() || contract.stage_2_device.empty()) {
        throw std::invalid_argument("A split stage device-ok kotelezok.");
    }
    if (contract.stage_1_device == contract.stage_2_device) {
        throw std::invalid_argument("A split stage device-ok nem lehetnek azonosak.");
    }
    if (is_cpu_device(contract.stage_1_device) || is_cpu_device(contract.stage_2_device)) {
        throw std::invalid_argument("A split handoff contract CPU device-ot nem enged.");
    }
    if (contract.validated_context_tokens <= 0) {
        throw std::invalid_argument("A split validated_context_tokens csak pozitiv lehet.");
    }
}

SplitTensorHandoffBuffer make_ram_backed_handoff_buffer(
    const ov::Tensor& source_tensor,
    const std::string_view source_contract,
    const std::string_view target_tensor_name) {
    SplitTensorHandoffBuffer result;
    result.element_type = source_tensor.get_element_type();
    result.shape = source_tensor.get_shape();
    result.byte_size = source_tensor.get_byte_size();
    result.storage.resize(result.byte_size);
    if (result.byte_size > 0U) {
        std::memcpy(result.storage.data(), source_tensor.data(), result.byte_size);
    }
    result.tensor = ov::Tensor{
        result.element_type,
        result.shape,
        result.storage.empty() ? nullptr : result.storage.data()};
    result.fingerprint = tensor_fingerprint(result.tensor);
    result.source_contract = std::string{source_contract};
    result.target_tensor_name = std::string{target_tensor_name};
    return result;
}

std::string describe_tensor_shape(const ov::Tensor& tensor) {
    std::ostringstream stream;
    stream << "[";
    const auto shape = tensor.get_shape();
    for (std::size_t index = 0; index < shape.size(); ++index) {
        if (index > 0) {
            stream << "x";
        }
        stream << shape[index];
    }
    stream << "]";
    return stream.str();
}

std::size_t layer_range_span(const std::string_view layer_range) {
    const auto separator = layer_range.find('-');
    if (separator == std::string_view::npos) {
        throw std::invalid_argument("A layer_range formatuma ervenytelen: " + std::string{layer_range});
    }
    const auto start = std::stoll(std::string{layer_range.substr(0, separator)});
    const auto end = std::stoll(std::string{layer_range.substr(separator + 1)});
    if (end < start) {
        throw std::invalid_argument("A layer_range vege nem lehet kisebb a kezdopontnal: " + std::string{layer_range});
    }
    return static_cast<std::size_t>(end - start + 1);
}

SplitRequestPathMode infer_split_request_path_mode(
    const bool split_mode,
    const bool compiled_request_path_active,
    const bool stage_1_placeholder_mode,
    const bool stage_2_placeholder_mode,
    const bool runtime_failure_detected) noexcept {
    if (!split_mode) {
        return SplitRequestPathMode::degraded;
    }
    if (runtime_failure_detected) {
        return SplitRequestPathMode::degraded;
    }
    if (stage_1_placeholder_mode || stage_2_placeholder_mode) {
        return SplitRequestPathMode::placeholder;
    }
    return compiled_request_path_active ? SplitRequestPathMode::compiled : SplitRequestPathMode::placeholder;
}

std::string_view to_string(const SplitRequestPathMode mode) noexcept {
    switch (mode) {
    case SplitRequestPathMode::placeholder:
        return "placeholder";
    case SplitRequestPathMode::compiled:
        return "compiled";
    case SplitRequestPathMode::degraded:
        return "degraded";
    }
    return "degraded";
}

}  // namespace masodik::split
