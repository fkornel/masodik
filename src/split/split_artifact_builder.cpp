#include "masodik/split/split_artifact_builder.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <span>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include <openvino/core/graph_util.hpp>
#include <openvino/op/assign.hpp>
#include <openvino/opsets/opset13.hpp>
#include <openvino/op/util/variable_extension.hpp>

#include "masodik/split/split_artifact_manifest_writer.hpp"

namespace masodik::split {
namespace {

constexpr std::uint64_t kFNVOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t kFNVPrime = 1099511628211ull;

bool split_builder_trace_enabled() {
    const char* const raw = std::getenv("MASODIK_SPLIT_BUILDER_TRACE");
    return raw != nullptr && raw[0] != '\0' && raw[0] != '0';
}

void trace_split_builder(const std::string& message) {
    if (!split_builder_trace_enabled()) {
        return;
    }
    std::cerr << "[split-builder] " << message << '\n';
}

struct ParsedCutLayer {
    int cut_after_index = -1;
    int next_index = -1;
    bool split_to_tail = false;
};

std::string normalize_model_name(const std::filesystem::path& root_path) {
    constexpr std::string_view prefix = "OpenVINO---";
    std::string name = root_path.filename().string();
    if (name.rfind(prefix, 0) == 0) {
        name.erase(0, prefix.size());
    }
    return name;
}

ParsedCutLayer parse_cut_after_layer(std::string_view raw_cut) {
    static const std::regex pattern{R"(layers\.(\d+)\s*->\s*(?:layers\.(\d+)|tail))"};
    std::smatch match;
    const std::string raw_cut_string{raw_cut};
    if (!std::regex_search(raw_cut_string, match, pattern)) {
        throw std::invalid_argument("A cut_after_layer formatuma ervenytelen: " + std::string{raw_cut});
    }

    const int cut_after_index = std::stoi(match[1].str());
    if (!match[2].matched) {
        return {.cut_after_index = cut_after_index, .next_index = -1, .split_to_tail = true};
    }

    const int next_index = std::stoi(match[2].str());
    if (next_index != cut_after_index + 1) {
        throw std::invalid_argument("A cut_after_layer csak szomszedos reteghatart tamogat: " + std::string{raw_cut});
    }

    return {.cut_after_index = cut_after_index, .next_index = next_index, .split_to_tail = false};
}

int extract_layer_index(std::string_view node_name) {
    static const std::regex pattern{R"(__module\.model\.layers\.(\d+)(?:[./]|$))"};
    std::smatch match;
    const std::string node_name_string{node_name};
    if (!std::regex_search(node_name_string, match, pattern)) {
        return -1;
    }
    return std::stoi(match[1].str());
}

bool belongs_to_layer(std::string_view node_name, const int layer_index) {
    return extract_layer_index(node_name) == layer_index;
}

int discover_max_layer_index(const std::shared_ptr<ov::Model>& source_model) {
    int max_layer_index = -1;
    for (const auto& node : source_model->get_ops()) {
        max_layer_index = std::max(max_layer_index, extract_layer_index(node->get_friendly_name()));
    }
    if (max_layer_index < 0) {
        throw std::runtime_error("A forrasmodellben nem talalhato __module.model.layers.N prefixu op.");
    }
    return max_layer_index;
}

std::string describe_output_contract(const ov::Output<ov::Node>& output) {
    const auto& names = output.get_names();
    if (!names.empty()) {
        return *names.begin();
    }
    return output.get_node()->get_friendly_name() + ":" + std::to_string(output.get_index());
}

std::shared_ptr<ov::Node> find_node_by_name(
    const std::shared_ptr<ov::Model>& model,
    const std::string& friendly_name) {
    for (const auto& node : model->get_ops()) {
        if (node->get_friendly_name() == friendly_name) {
            return node;
        }
    }
    throw std::runtime_error("A keresett op nem talalhato a modellben: " + friendly_name);
}

bool variable_belongs_to_stage(const std::string& variable_id, const int start_layer, const int end_layer) {
    static const std::regex pattern{R"((\d+))"};
    std::smatch match;
    if (!std::regex_search(variable_id, match, pattern)) {
        return false;
    }
    const int layer_index = std::stoi(match[1].str());
    return layer_index >= start_layer && layer_index <= end_layer;
}

bool output_originates_from_layer(
    const ov::Output<ov::Node>& output,
    const int target_layer_index,
    std::unordered_set<std::string>& visited) {
    const auto producer = output.get_node_shared_ptr();
    if (producer == nullptr) {
        return false;
    }

    const auto visit_key = producer->get_friendly_name() + ":" + std::to_string(output.get_index());
    if (!visited.insert(visit_key).second) {
        return false;
    }

    const int producer_layer_index = extract_layer_index(producer->get_friendly_name());
    if (producer_layer_index >= 0) {
        return producer_layer_index == target_layer_index;
    }

    for (const auto& input : producer->inputs()) {
        if (output_originates_from_layer(input.get_source_output(), target_layer_index, visited)) {
            return true;
        }
    }
    return false;
}

ov::SinkVector filter_stage_sinks(
    const std::shared_ptr<ov::Model>& model,
    const int start_layer,
    const int end_layer) {
    ov::SinkVector sinks;
    for (const auto& sink : model->get_sinks()) {
        const auto* variable_extension = dynamic_cast<ov::op::util::VariableExtension*>(sink.get());
        if (variable_extension == nullptr) {
            continue;
        }
        if (variable_belongs_to_stage(variable_extension->get_variable_id(), start_layer, end_layer)) {
            sinks.push_back(sink);
        }
    }
    return sinks;
}

void update_fnv64(std::uint64_t& hash, const std::span<const char> bytes) {
    for (const char byte : bytes) {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= kFNVPrime;
    }
}

std::string compute_source_model_checksum(const std::filesystem::path& source_model_root) {
    std::uint64_t hash = kFNVOffsetBasis;
    const std::array<std::filesystem::path, 2> files = {
        source_model_root / "openvino_model.xml",
        source_model_root / "openvino_model.bin"};
    for (const auto& file_path : files) {
        if (!std::filesystem::exists(file_path)) {
            throw std::runtime_error("A checksumhez hianyzik a forrasfajl: " + file_path.string());
        }
        const auto file_path_string = file_path.string();
        update_fnv64(hash, std::span<const char>{file_path_string.data(), file_path_string.size()});
        const auto file_size = std::filesystem::file_size(file_path);
        const auto last_write_tick = std::filesystem::last_write_time(file_path).time_since_epoch().count();
        const auto metadata = std::to_string(file_size) + ":" + std::to_string(last_write_tick);
        update_fnv64(hash, std::span<const char>{metadata.data(), metadata.size()});
    }

    std::ostringstream stream;
    stream << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}

void copy_if_exists(const std::filesystem::path& source_path, const std::filesystem::path& destination_path) {
    if (!std::filesystem::exists(source_path)) {
        return;
    }
    std::filesystem::create_directories(destination_path.parent_path());
    std::filesystem::copy_file(source_path, destination_path, std::filesystem::copy_options::overwrite_existing);
}

void copy_shared_metadata(const std::filesystem::path& source_root, const std::filesystem::path& output_root) {
    const std::array<std::string_view, 11> file_names = {
        "tokenizer.json",
        "tokenizer_config.json",
        "config.json",
        "generation_config.json",
        "special_tokens_map.json",
        "chat_template.jinja",
        "chat_template_original.jinja",
        "openvino_tokenizer.xml",
        "openvino_tokenizer.bin",
        "openvino_detokenizer.xml",
        "openvino_detokenizer.bin"};

    for (const auto& file_name : file_names) {
        copy_if_exists(source_root / file_name, output_root / file_name);
    }
}

std::shared_ptr<ov::Model> build_stage_1_model(
    const std::shared_ptr<ov::Model>& source_model,
    const SplitBoundaryDiscovery& boundary) {
    const auto boundary_node = find_node_by_name(source_model, boundary.boundary_producer_name);
    const auto boundary_output = boundary_node->output(boundary.boundary_output_index);
    const auto stage_sinks = filter_stage_sinks(source_model, 0, boundary.cut_after_index);
    return std::make_shared<ov::Model>(
        ov::OutputVector{boundary_output},
        stage_sinks,
        "manual_split_stage_1");
}

std::shared_ptr<ov::Model> build_stage_2_model(
    const std::shared_ptr<ov::Model>& source_model,
    const SplitBoundaryDiscovery& boundary) {
    const auto boundary_node = find_node_by_name(source_model, boundary.boundary_producer_name);
    const auto boundary_output = boundary_node->output(boundary.boundary_output_index);

    auto handoff_parameter = std::make_shared<ov::opset13::Parameter>(
        boundary_output.get_element_type(),
        boundary_output.get_partial_shape());
    handoff_parameter->set_friendly_name("hidden_states_from_stage_1");
    handoff_parameter->output(0).set_names({"hidden_states_from_stage_1"});

    const std::unordered_set<std::string> boundary_consumer_names{
        boundary.boundary_consumer_names.begin(),
        boundary.boundary_consumer_names.end()};
    for (const auto& target_input : boundary_output.get_target_inputs()) {
        const auto consumer_name = target_input.get_node()->get_friendly_name();
        if (boundary_consumer_names.contains(consumer_name)) {
            target_input.replace_source_output(handoff_parameter->output(0));
        }
    }

    const auto stage_sinks = filter_stage_sinks(source_model, boundary.next_index, boundary.max_layer_index);
    ov::OutputVector stage_outputs;
    stage_outputs.reserve(source_model->outputs().size());
    for (auto output : source_model->outputs()) {
        stage_outputs.push_back(output);
    }

    return std::make_shared<ov::Model>(
        stage_outputs,
        stage_sinks,
        "manual_split_stage_2");
}

void ensure_split_output_root(const SplitArtifactBuildOptions& options) {
    if (options.source_model_root.empty() || options.output_root.empty()) {
        throw std::invalid_argument("A split artifact builder source_model_root es output_root argumentumot var.");
    }
    const auto source_root = std::filesystem::weakly_canonical(options.source_model_root);
    const auto output_root = std::filesystem::absolute(options.output_root).lexically_normal();
    if (source_root == output_root) {
        throw std::invalid_argument("A split artifact output_root nem mutathat a forrasmodell gyokerere.");
    }
}

}  // namespace

SplitBoundaryDiscovery discover_split_boundary(
    const std::shared_ptr<ov::Model>& source_model,
    const std::string_view cut_after_layer) {
    if (source_model == nullptr) {
        throw std::invalid_argument("A split boundary discovery forrasmodellje nem lehet null.");
    }

    const auto parsed_cut = parse_cut_after_layer(cut_after_layer);
    const auto max_layer_index = discover_max_layer_index(source_model);
    if (parsed_cut.cut_after_index < 0 || parsed_cut.cut_after_index > max_layer_index) {
        throw std::runtime_error("A cut_after_layer a modell retegtartomanyan kivulre mutat: " + std::string{cut_after_layer});
    }
    const int effective_next_index = parsed_cut.split_to_tail ? max_layer_index + 1 : parsed_cut.next_index;

    struct CandidateKey {
        std::string producer_name;
        std::size_t output_index = 0;

        bool operator<(const CandidateKey& other) const {
            return producer_name == other.producer_name ? output_index < other.output_index
                                                        : producer_name < other.producer_name;
        }
    };

    std::map<CandidateKey, std::set<std::string>> candidates;
    for (const auto& node : source_model->get_ops()) {
        const auto node_layer_index = extract_layer_index(node->get_friendly_name());
        const bool is_stage_2_consumer =
            parsed_cut.split_to_tail ? node_layer_index < 0 : node_layer_index == effective_next_index;
        if (!is_stage_2_consumer) {
            continue;
        }
        for (const auto& input : node->inputs()) {
            const auto source_output = input.get_source_output();
            const auto producer = source_output.get_node_shared_ptr();
            if (producer == nullptr) {
                continue;
            }
            if (!parsed_cut.split_to_tail && extract_layer_index(producer->get_friendly_name()) == effective_next_index) {
                continue;
            }
            const bool direct_cut_layer_match = belongs_to_layer(producer->get_friendly_name(), parsed_cut.cut_after_index);
            std::unordered_set<std::string> visited;
            const bool indirect_cut_layer_match =
                !direct_cut_layer_match && output_originates_from_layer(source_output, parsed_cut.cut_after_index, visited);
            if (!direct_cut_layer_match && !indirect_cut_layer_match) {
                continue;
            }
            candidates[{producer->get_friendly_name(), source_output.get_index()}].insert(node->get_friendly_name());
        }
    }

    if (candidates.empty()) {
        throw std::runtime_error("Nem talalhato stage boundary a megadott reteghatarnal: " + std::string{cut_after_layer});
    }
    if (candidates.size() != 1U) {
        if (parsed_cut.split_to_tail) {
            struct RankedCandidate {
                CandidateKey key;
                std::set<std::string> consumers;
                int score = 0;
            };

            std::vector<RankedCandidate> ranked_candidates;
            ranked_candidates.reserve(candidates.size());
            for (const auto& [candidate, consumers] : candidates) {
                auto ranked = RankedCandidate{.key = candidate, .consumers = consumers, .score = static_cast<int>(consumers.size())};
                const auto boundary_node = find_node_by_name(source_model, candidate.producer_name);
                const auto contract_name = describe_output_contract(boundary_node->output(candidate.output_index));
                if (contract_name.find("hidden_states") != std::string::npos) {
                    ranked.score += 100;
                }
                if (candidate.producer_name.find("/aten::add/Add_1") != std::string::npos) {
                    ranked.score += 50;
                } else if (candidate.producer_name.find("/aten::add/Add") != std::string::npos) {
                    ranked.score += 20;
                }
                ranked_candidates.push_back(std::move(ranked));
            }

            std::sort(
                ranked_candidates.begin(),
                ranked_candidates.end(),
                [](const RankedCandidate& left, const RankedCandidate& right) {
                    if (left.score != right.score) {
                        return left.score > right.score;
                    }
                    return left.key.producer_name < right.key.producer_name;
                });

            if (!ranked_candidates.empty()) {
                const auto& best = ranked_candidates.front();
                const bool best_is_unique = ranked_candidates.size() == 1U || best.score > ranked_candidates[1].score;
                if (best_is_unique) {
                    candidates = {{best.key, best.consumers}};
                }
            }
        }

    }
    if (candidates.size() != 1U) {
        if (split_builder_trace_enabled()) {
            for (const auto& [candidate, consumers] : candidates) {
                std::ostringstream stream;
                stream << "boundary-candidate producer=\"" << candidate.producer_name
                       << "\" output_index=" << candidate.output_index
                       << " consumer_count=" << consumers.size();
                trace_split_builder(stream.str());
            }
        }
        throw std::runtime_error(
            "A split builder egyetlen hidden-state boundary outputot var, de tobbet talalt a reteghatarnal: " +
            std::string{cut_after_layer});
    }

    const auto& [candidate_key, consumer_names] = *candidates.begin();
    const auto boundary_node = find_node_by_name(source_model, candidate_key.producer_name);
    const auto boundary_output = boundary_node->output(candidate_key.output_index);
    std::vector<std::string> boundary_consumer_names{consumer_names.begin(), consumer_names.end()};

    const auto stage_2_boundary_label = parsed_cut.split_to_tail
        ? std::string{"tail"}
        : std::to_string(effective_next_index) + "-" + std::to_string(max_layer_index);

    return {
        .cut_after_index = parsed_cut.cut_after_index,
        .next_index = effective_next_index,
        .max_layer_index = max_layer_index,
        .cut_after_layer = std::string{cut_after_layer},
        .stage_boundary = "transformer layers 0-" + std::to_string(parsed_cut.cut_after_index) + " | " +
            stage_2_boundary_label,
        .stage_1_layer_range = "0-" + std::to_string(parsed_cut.cut_after_index),
        .stage_2_layer_range = stage_2_boundary_label,
        .boundary_output_name = describe_output_contract(boundary_output),
        .boundary_producer_name = candidate_key.producer_name,
        .boundary_output_index = candidate_key.output_index,
        .boundary_consumer_names = std::move(boundary_consumer_names)};
}

SplitArtifactBuildResult build_split_artifact(const SplitArtifactBuildOptions& options) {
    ensure_split_output_root(options);
    trace_split_builder("validated input paths");

    const auto source_model_root = std::filesystem::weakly_canonical(options.source_model_root);
    auto output_root = options.output_root;
    if (options.clean_output_root && std::filesystem::exists(output_root)) {
        std::filesystem::remove_all(output_root);
    }
    std::filesystem::create_directories(output_root);
    trace_split_builder("prepared output root: " + output_root.string());

    ov::Core core;
    const auto source_model_xml_path = source_model_root / "openvino_model.xml";
    trace_split_builder("reading source model: " + source_model_xml_path.string());
    const auto source_model = core.read_model(source_model_xml_path);
    trace_split_builder("discovering boundary");
    const auto boundary = discover_split_boundary(source_model, options.cut_after_layer);
    trace_split_builder("boundary discovered: " + boundary.boundary_producer_name);

    const auto stage_1_root = output_root / "stage-1";
    const auto stage_2_root = output_root / "stage-2";
    std::filesystem::create_directories(stage_1_root);
    std::filesystem::create_directories(stage_2_root);

    trace_split_builder("building stage-1 model");
    auto stage_1_model = build_stage_1_model(core.read_model(source_model_xml_path), boundary);
    trace_split_builder("building stage-2 model");
    auto stage_2_model = build_stage_2_model(core.read_model(source_model_xml_path), boundary);

    trace_split_builder("serializing stage-1");
    ov::serialize(stage_1_model, stage_1_root / "openvino_model.xml", stage_1_root / "openvino_model.bin");
    trace_split_builder("serializing stage-2");
    ov::serialize(stage_2_model, stage_2_root / "openvino_model.xml", stage_2_root / "openvino_model.bin");
    trace_split_builder("copying shared metadata");
    copy_shared_metadata(source_model_root, output_root);

    masodik::config::SplitArtifactManifest manifest;
    trace_split_builder("computing manifest checksum");
    manifest.model_name = normalize_model_name(source_model_root);
    manifest.artifact_version = options.artifact_version;
    manifest.source_model_checksum = compute_source_model_checksum(source_model_root);
    manifest.stage_count = 2;
    manifest.stage_boundary = boundary.stage_boundary;
    manifest.cut_after_layer = boundary.cut_after_layer;
    manifest.tokenizer_path = "tokenizer.json";
    manifest.max_context_tokens = options.max_context_tokens;
    manifest.validated_context_tokens = options.validated_context_tokens;
    manifest.hidden_state_contract = boundary.boundary_output_name;
    manifest.kv_contract =
        "stateful-kv:" + boundary.stage_1_layer_range + "|" + boundary.stage_2_layer_range;
    manifest.validated_for_profile = options.validated_for_profile;
    manifest.stage_1 = {
        .device_role = "primary",
        .artifact_path = "stage-1",
        .layer_range = boundary.stage_1_layer_range,
        .expected_input = "token ids + kv context",
        .expected_output = boundary.boundary_output_name,
        .memory_budget_gb = options.stage_1_memory_budget_gb,
        .kv_budget_gb = options.stage_1_kv_budget_gb};
    manifest.stage_2 = {
        .device_role = "secondary",
        .artifact_path = "stage-2",
        .layer_range = boundary.stage_2_layer_range,
        .expected_input = boundary.boundary_output_name,
        .expected_output = "logits + updated kv state",
        .memory_budget_gb = options.stage_2_memory_budget_gb,
        .kv_budget_gb = options.stage_2_kv_budget_gb};

    trace_split_builder("writing manifest");
    const auto manifest_path = output_root / "split-manifest.json";
    write_split_artifact_manifest(manifest, output_root, manifest_path);
    trace_split_builder("validating manifest");
    manifest.validate(output_root);

    return {
        .source_model_root = source_model_root,
        .artifact_root = std::filesystem::weakly_canonical(output_root),
        .split_manifest_path = manifest_path,
        .stage_1_root = stage_1_root,
        .stage_2_root = stage_2_root,
        .manifest = std::move(manifest)};
}

}  // namespace masodik::split
