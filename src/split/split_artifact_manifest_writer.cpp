#include "masodik/split/split_artifact_manifest_writer.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace masodik::split {
namespace {

std::string escape_json(std::string_view raw) {
    std::string escaped;
    escaped.reserve(raw.size());
    for (const char character : raw) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

}  // namespace

std::string render_split_artifact_manifest_json(const masodik::config::SplitArtifactManifest& manifest) {
    std::ostringstream stream;
    stream
        << "{\n"
        << "  \"model_name\": \"" << escape_json(manifest.model_name) << "\",\n"
        << "  \"artifact_version\": \"" << escape_json(manifest.artifact_version) << "\",\n"
        << "  \"source_model_checksum\": \"" << escape_json(manifest.source_model_checksum) << "\",\n"
        << "  \"stage_count\": " << manifest.stage_count << ",\n"
        << "  \"stage_boundary\": \"" << escape_json(manifest.stage_boundary) << "\",\n"
        << "  \"cut_after_layer\": \"" << escape_json(manifest.cut_after_layer) << "\",\n"
        << "  \"tokenizer_path\": \"" << escape_json(manifest.tokenizer_path.string()) << "\",\n"
        << "  \"max_context_tokens\": " << manifest.max_context_tokens << ",\n"
        << "  \"validated_context_tokens\": " << manifest.validated_context_tokens << ",\n"
        << "  \"hidden_state_contract\": \"" << escape_json(manifest.hidden_state_contract) << "\",\n"
        << "  \"kv_contract\": \"" << escape_json(manifest.kv_contract) << "\",\n"
        << "  \"validated_for_profile\": \"" << escape_json(manifest.validated_for_profile) << "\",\n"
        << "  \"stage_1\": {\n"
        << "    \"device_role\": \"" << escape_json(manifest.stage_1.device_role) << "\",\n"
        << "    \"artifact_path\": \"" << escape_json(manifest.stage_1.artifact_path.string()) << "\",\n"
        << "    \"layer_range\": \"" << escape_json(manifest.stage_1.layer_range) << "\",\n"
        << "    \"expected_input\": \"" << escape_json(manifest.stage_1.expected_input) << "\",\n"
        << "    \"expected_output\": \"" << escape_json(manifest.stage_1.expected_output) << "\",\n"
        << "    \"memory_budget_gb\": " << manifest.stage_1.memory_budget_gb << ",\n"
        << "    \"kv_budget_gb\": " << manifest.stage_1.kv_budget_gb << "\n"
        << "  },\n"
        << "  \"stage_2\": {\n"
        << "    \"device_role\": \"" << escape_json(manifest.stage_2.device_role) << "\",\n"
        << "    \"artifact_path\": \"" << escape_json(manifest.stage_2.artifact_path.string()) << "\",\n"
        << "    \"layer_range\": \"" << escape_json(manifest.stage_2.layer_range) << "\",\n"
        << "    \"expected_input\": \"" << escape_json(manifest.stage_2.expected_input) << "\",\n"
        << "    \"expected_output\": \"" << escape_json(manifest.stage_2.expected_output) << "\",\n"
        << "    \"memory_budget_gb\": " << manifest.stage_2.memory_budget_gb << ",\n"
        << "    \"kv_budget_gb\": " << manifest.stage_2.kv_budget_gb << "\n"
        << "  }\n"
        << "}\n";
    return stream.str();
}

void write_split_artifact_manifest(
    const masodik::config::SplitArtifactManifest& manifest,
    const std::filesystem::path& split_root,
    const std::filesystem::path& manifest_path) {
    const auto resolved_manifest_path =
        manifest_path.empty() ? split_root / "split-manifest.json" : manifest_path;
    std::filesystem::create_directories(resolved_manifest_path.parent_path());

    std::ofstream stream{resolved_manifest_path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        throw std::runtime_error("A split manifest nem nyithato meg irasra: " + resolved_manifest_path.string());
    }

    stream << render_split_artifact_manifest_json(manifest);
    if (!stream.good()) {
        throw std::runtime_error("A split manifest irasa kozben hiba tortent: " + resolved_manifest_path.string());
    }
}

}  // namespace masodik::split
