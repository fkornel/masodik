#include "masodik/config/model_repository.hpp"

#include <stdexcept>

#include "masodik/config/default_paths.hpp"
#include "masodik/config/json_loader.hpp"

namespace masodik::config {
namespace {

std::string normalize_model_id(std::string name) {
    constexpr std::string_view prefix = "OpenVINO---";
    if (name.rfind(prefix, 0) == 0) {
        name.erase(0, prefix.size());
    }
    return name;
}

std::filesystem::path first_existing_path(
    const std::filesystem::path& root,
    std::initializer_list<std::filesystem::path> candidates) {
    for (const auto& candidate : candidates) {
        const auto full_path = root / candidate;
        if (std::filesystem::exists(full_path)) {
            return full_path;
        }
    }
    return {};
}

std::filesystem::path resolve_against_root(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) {
    if (candidate.empty()) {
        return {};
    }
    return candidate.is_absolute() ? candidate : root / candidate;
}

SplitStageManifest parse_split_stage_manifest(const JsonValue::Object& object) {
    SplitStageManifest stage;
    stage.device_role = get_string(object, "device_role");
    stage.artifact_path = std::filesystem::path{get_string(object, "artifact_path")};
    stage.layer_range = get_string(object, "layer_range");
    stage.expected_input = get_string(object, "expected_input");
    stage.expected_output = get_string(object, "expected_output");
    stage.memory_budget_gb = get_number(object, "memory_budget_gb");
    stage.kv_budget_gb = get_number(object, "kv_budget_gb");
    return stage;
}

}  // namespace

void SplitStageManifest::validate(const std::filesystem::path& split_root, const std::string_view stage_name) const {
    if (device_role.empty()) {
        throw std::invalid_argument("A split manifest " + std::string{stage_name} + ".device_role mezoje kotelezo.");
    }
    if (artifact_path.empty()) {
        throw std::invalid_argument("A split manifest " + std::string{stage_name} + ".artifact_path mezoje kotelezo.");
    }
    if (layer_range.empty()) {
        throw std::invalid_argument("A split manifest " + std::string{stage_name} + ".layer_range mezoje kotelezo.");
    }
    if (expected_input.empty() || expected_output.empty()) {
        throw std::invalid_argument("A split manifest stage input/output szerzodeset kotelezo megadni.");
    }
    if (memory_budget_gb <= 0.0) {
        throw std::invalid_argument("A split manifest memoria-budget csak pozitiv lehet.");
    }
    if (kv_budget_gb <= 0.0) {
        throw std::invalid_argument("A split manifest KV-budget csak pozitiv lehet.");
    }
    const auto resolved_path = resolve_against_root(split_root, artifact_path);
    if (!std::filesystem::exists(resolved_path)) {
        throw std::invalid_argument("A split manifest stage artifact path nem letezik: " + resolved_path.string());
    }
}

void SplitArtifactManifest::validate(const std::filesystem::path& split_root) const {
    if (model_name.empty()) {
        throw std::invalid_argument("A split manifest model_name mezoje kotelezo.");
    }
    if (artifact_version.empty()) {
        throw std::invalid_argument("A split manifest artifact_version mezoje kotelezo.");
    }
    if (source_model_checksum.empty()) {
        throw std::invalid_argument("A split manifest source_model_checksum mezoje kotelezo.");
    }
    if (stage_count != 2) {
        throw std::invalid_argument("A split manifest stage_count erteke jelenleg csak 2 lehet.");
    }
    if (stage_boundary.empty()) {
        throw std::invalid_argument("A split manifest stage_boundary mezoje kotelezo.");
    }
    if (cut_after_layer.empty()) {
        throw std::invalid_argument("A split manifest cut_after_layer mezoje kotelezo.");
    }
    if (tokenizer_path.empty()) {
        throw std::invalid_argument("A split manifest tokenizer_path mezoje kotelezo.");
    }
    if (hidden_state_contract.empty() || kv_contract.empty()) {
        throw std::invalid_argument("A split manifest hidden_state_contract es kv_contract mezoje kotelezo.");
    }
    if (validated_for_profile.empty()) {
        throw std::invalid_argument("A split manifest validated_for_profile mezoje kotelezo.");
    }
    if (max_context_tokens <= 0) {
        throw std::invalid_argument("A split manifest max_context_tokens mezoje kotelezo es pozitiv kell legyen.");
    }
    if (validated_context_tokens <= 0) {
        throw std::invalid_argument("A split manifest validated_context_tokens mezoje kotelezo es pozitiv kell legyen.");
    }
    const auto resolved_tokenizer = resolve_against_root(split_root, tokenizer_path);
    if (!std::filesystem::exists(resolved_tokenizer)) {
        throw std::invalid_argument("A split manifest tokenizer_path nem letezik: " + resolved_tokenizer.string());
    }

    stage_1.validate(split_root, "stage_1");
    stage_2.validate(split_root, "stage_2");
    if (stage_1.device_role != "primary") {
        throw std::invalid_argument("A split manifest stage_1.device_role csak primary lehet.");
    }
    if (stage_2.device_role != "secondary") {
        throw std::invalid_argument("A split manifest stage_2.device_role csak secondary lehet.");
    }
    if (stage_1.expected_output != stage_2.expected_input) {
        throw std::invalid_argument("A split manifest stage_1.expected_output es stage_2.expected_input nem kompatibilis.");
    }
    if (stage_1.layer_range == stage_2.layer_range) {
        throw std::invalid_argument("A split manifest stage layer_range mezoi nem lehetnek azonosak.");
    }
}

void ModelRepository::validate() const {
    if (root_path.empty() || !std::filesystem::exists(root_path)) {
        throw std::invalid_argument("A modell root path nem letezik.");
    }
    if (split_mode) {
        if (split_root_path.empty() || !std::filesystem::exists(split_root_path)) {
            throw std::invalid_argument("A split artifact root path nem letezik.");
        }
        if (split_manifest_path.empty() || !std::filesystem::exists(split_manifest_path)) {
            throw std::invalid_argument("A split manifest fajl hianyzik.");
        }
        split_artifact_manifest.validate(split_root_path);
        return;
    }
    if (model_xml_path.empty() || !std::filesystem::exists(model_xml_path)) {
        throw std::invalid_argument("Az openvino_model.xml hianyzik.");
    }
    if (model_bin_path.empty() || !std::filesystem::exists(model_bin_path)) {
        throw std::invalid_argument("Az openvino_model.bin hianyzik.");
    }
    if (graph_path.empty() || !std::filesystem::exists(graph_path)) {
        throw std::invalid_argument("A graph.pbtxt hianyzik.");
    }
    if (source != "AI Playground") {
        throw std::invalid_argument("A v1-ben csak AI Playground eredetu modell tamogatott.");
    }
}

SplitArtifactManifest load_split_artifact_manifest(
    const std::filesystem::path& split_root,
    const std::filesystem::path& manifest_path) {
    const auto resolved_manifest_path = manifest_path.empty()
        ? first_existing_path(split_root, {"split-manifest.json", "split_artifact_manifest.json", "manifest.json"})
        : resolve_against_root(split_root, manifest_path);
    if (resolved_manifest_path.empty() || !std::filesystem::exists(resolved_manifest_path)) {
        throw std::invalid_argument("A split manifest nem talalhato.");
    }

    const auto root_value = JsonLoader::load_file(resolved_manifest_path);
    if (!root_value.is_object()) {
        throw std::runtime_error("A split manifest gyokere csak JSON objektum lehet.");
    }

    const auto& object = root_value.as_object();
    SplitArtifactManifest manifest;
    manifest.model_name = get_string(object, "model_name");
    manifest.artifact_version = get_string(object, "artifact_version");
    manifest.source_model_checksum = get_string(object, "source_model_checksum");
    manifest.stage_count = static_cast<int>(get_number(object, "stage_count"));
    manifest.stage_boundary = get_string(object, "stage_boundary");
    manifest.cut_after_layer = get_string(object, "cut_after_layer");
    manifest.tokenizer_path = std::filesystem::path{get_string(object, "tokenizer_path")};
    manifest.max_context_tokens = static_cast<int>(get_number_or(object, "max_context_tokens", 0.0));
    manifest.validated_context_tokens = static_cast<int>(get_number_or(object, "validated_context_tokens", 0.0));
    manifest.hidden_state_contract = get_string(object, "hidden_state_contract");
    manifest.kv_contract = get_string(object, "kv_contract");
    manifest.validated_for_profile = get_string(object, "validated_for_profile");
    manifest.stage_1 = parse_split_stage_manifest(get_object(object, "stage_1"));
    manifest.stage_2 = parse_split_stage_manifest(get_object(object, "stage_2"));
    manifest.validate(split_root);
    return manifest;
}

ModelRepository inspect_model_repository(
    const std::filesystem::path& root_path,
    std::string source,
    const bool read_only,
    const bool expect_split,
    const std::filesystem::path& split_manifest_path) {
    ModelRepository repository;
    repository.root_path = root_path.empty() ? default_paths::default_model_root : root_path;
    repository.split_mode = expect_split;

    if (repository.split_mode) {
        repository.split_root_path = repository.root_path;
        repository.split_manifest_path = split_manifest_path.empty()
            ? first_existing_path(repository.root_path, {"split-manifest.json", "split_artifact_manifest.json", "manifest.json"})
            : resolve_against_root(repository.root_path, split_manifest_path);
        repository.split_artifact_manifest = load_split_artifact_manifest(repository.root_path, repository.split_manifest_path);
        repository.model_id = normalize_model_id(repository.split_artifact_manifest.model_name);
        repository.tokenizer_json_path = resolve_against_root(
            repository.root_path,
            repository.split_artifact_manifest.tokenizer_path);
    } else {
        repository.model_id = normalize_model_id(repository.root_path.filename().string());
        repository.model_xml_path = first_existing_path(repository.root_path, {"openvino_model.xml", "model.xml"});
        repository.model_bin_path = first_existing_path(repository.root_path, {"openvino_model.bin", "model.bin"});
        repository.tokenizer_json_path = first_existing_path(
            repository.root_path,
            {"tokenizer.json", "tokenizer_config.json", std::filesystem::path{"tokenizer"} / "tokenizer.json"});
        repository.graph_path = first_existing_path(
            repository.root_path, {"graph.pbtxt", std::filesystem::path{"config"} / "graph.pbtxt"});
    }
    repository.source = std::move(source);
    repository.read_only = read_only;
    repository.validate();
    return repository;
}

}  // namespace masodik::config
