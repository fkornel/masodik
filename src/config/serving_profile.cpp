#include "masodik/config/serving_profile.hpp"

#include <stdexcept>

#include "masodik/config/default_paths.hpp"
#include "masodik/config/json_loader.hpp"

namespace masodik::config {
namespace {

std::filesystem::path get_path_or(
    const JsonValue::Object& object,
    const std::string_view key,
    const std::filesystem::path& fallback) {
    return std::filesystem::path{get_string_or(object, key, fallback.string())};
}

}  // namespace

void ServingProfile::validate() const {
    if (primary_device.empty()) {
        throw std::invalid_argument("A primary_device nem lehet ures.");
    }
    if (hybrid_enabled && hybrid_fallback_device.empty()) {
        throw std::invalid_argument("Hybrid profil eseten kotelezo a hybrid_fallback_device.");
    }
    if (split_mode) {
        if (split_stage_1_device.empty() || split_stage_2_device.empty()) {
            throw std::invalid_argument("Split profil eseten a split_stage_1_device es split_stage_2_device kotelezo.");
        }
        if (split_stage_1_device == split_stage_2_device) {
            throw std::invalid_argument("Split profil eseten a ket stage eszkoz nem lehet azonos.");
        }
        if (!reject_cpu_fallback) {
            throw std::invalid_argument("Split profil eseten a reject_cpu_fallback csak true lehet.");
        }
        if (split_manifest_path.empty()) {
            throw std::invalid_argument("Split profil eseten a split_manifest_path kotelezo.");
        }
        if (split_stage_1_memory_budget_gb <= 0.0 || split_stage_2_memory_budget_gb <= 0.0) {
            throw std::invalid_argument("Split profil eseten a stage memoria-budget csak pozitiv lehet.");
        }
    }
    if (desired_context_tokens <= 0) {
        throw std::invalid_argument("A desired_context_tokens csak pozitiv lehet.");
    }
    if (bind_host.empty()) {
        throw std::invalid_argument("A bind_host nem lehet ures.");
    }
    if (bind_port == 0U) {
        throw std::invalid_argument("A bind_port nem lehet 0.");
    }
    if (cpu_fallback_enabled) {
        throw std::invalid_argument("A v1-ben a cpu_fallback_enabled csak false lehet.");
    }
    if (timeout_seconds <= 0) {
        throw std::invalid_argument("A timeout_seconds csak pozitiv lehet.");
    }
    if (health_endpoint_path.empty() || chat_endpoint_path.empty() || models_endpoint_path.empty()) {
        throw std::invalid_argument("Az in-process endpoint path mezok nem lehetnek uresek.");
    }
    queue_policy.validate();
}

ServingProfile load_serving_profile(const std::filesystem::path& file_path) {
    const auto root_value = JsonLoader::load_file(file_path);
    if (!root_value.is_object()) {
        throw std::runtime_error("A serving profile gyokere csak JSON objektum lehet.");
    }

    const auto& object = root_value.as_object();
    ServingProfile profile;
    profile.profile_name = get_string_or(object, "profile_name", profile.profile_name);
    profile.model_name = get_string_or(object, "model_name", profile.model_name);
    profile.primary_device =
        get_string_or(object, "primary_device", get_string_or(object, "target_device", profile.primary_device));
    profile.hybrid_enabled = get_bool_or(object, "hybrid_enabled", get_bool_or(object, "hetero_enabled", profile.hybrid_enabled));
    profile.hybrid_fallback_device = get_string_or(
        object,
        "hybrid_fallback_device",
        get_string_or(object, "hetero_fallback_device", profile.hybrid_fallback_device));
    profile.split_mode = get_bool_or(object, "split_mode", profile.split_mode);
    profile.split_stage_1_device = get_string_or(object, "split_stage_1_device", profile.split_stage_1_device);
    profile.split_stage_2_device = get_string_or(object, "split_stage_2_device", profile.split_stage_2_device);
    profile.reject_cpu_fallback = get_bool_or(object, "reject_cpu_fallback", profile.reject_cpu_fallback);
    profile.kv_cache_precision = get_string_or(object, "kv_cache_precision", profile.kv_cache_precision);
    profile.split_manifest_path = get_path_or(object, "split_manifest_path", profile.split_manifest_path);
    profile.split_diagnostics_level = get_string_or(object, "split_diagnostics_level", profile.split_diagnostics_level);
    profile.split_stage_1_memory_budget_gb =
        get_number_or(object, "split_stage_1_memory_budget_gb", profile.split_stage_1_memory_budget_gb);
    profile.split_stage_2_memory_budget_gb =
        get_number_or(object, "split_stage_2_memory_budget_gb", profile.split_stage_2_memory_budget_gb);
    profile.model_distribution_policy =
        get_string_or(object, "model_distribution_policy", profile.model_distribution_policy);
    profile.bind_host = get_string_or(object, "bind_host", profile.bind_host);
    profile.bind_port =
        static_cast<std::uint16_t>(get_number_or(object, "bind_port", static_cast<double>(profile.bind_port)));
    profile.desired_context_tokens =
        static_cast<int>(get_number_or(object, "desired_context_tokens", profile.desired_context_tokens));
    profile.max_num_seqs = static_cast<int>(get_number_or(object, "max_num_seqs", profile.max_num_seqs));
    profile.prefix_caching_enabled =
        get_bool_or(object, "prefix_caching_enabled", profile.prefix_caching_enabled);
    profile.enable_runtime_assignment_diagnostics = get_bool_or(
        object,
        "enable_runtime_assignment_diagnostics",
        profile.enable_runtime_assignment_diagnostics);
    profile.cpu_fallback_enabled = get_bool_or(object, "cpu_fallback_enabled", profile.cpu_fallback_enabled);
    profile.timeout_seconds = static_cast<int>(get_number_or(object, "timeout_seconds", profile.timeout_seconds));
    profile.cache_directory = get_path_or(object, "cache_directory", profile.cache_directory);
    profile.runtime_log_path = get_path_or(
        object,
        "runtime_log_path",
        get_path_or(object, "ovms_log_path", profile.runtime_log_path));
    profile.health_endpoint_path =
        get_string_or(object, "health_endpoint_path", get_string_or(object, "ready_endpoint", profile.health_endpoint_path));
    profile.chat_endpoint_path =
        get_string_or(object, "chat_endpoint_path", get_string_or(object, "chat_endpoint", profile.chat_endpoint_path));
    profile.models_endpoint_path =
        get_string_or(object, "models_endpoint_path", get_string_or(object, "models_endpoint", profile.models_endpoint_path));
    profile.ovms_graph_path = get_string_or(
        object, "ovms_graph_path", (std::filesystem::path{"config"} / "ovms" / "gpt-oss-20b-int4-ov.pbtxt").string());
    profile.ovms_log_path = profile.runtime_log_path;
    profile.ready_endpoint = profile.health_endpoint_path;
    profile.chat_endpoint = profile.chat_endpoint_path;
    profile.models_endpoint = profile.models_endpoint_path;
    profile.target_device = profile.primary_device;
    profile.hetero_enabled = profile.hybrid_enabled;
    profile.hetero_fallback_device = profile.hybrid_fallback_device;
    if (profile.split_mode) {
        profile.primary_device = profile.split_stage_1_device;
        profile.target_device = profile.split_stage_1_device;
        profile.hybrid_enabled = false;
        profile.hetero_enabled = false;
        profile.hybrid_fallback_device.clear();
        profile.hetero_fallback_device.clear();
        profile.model_distribution_policy.clear();
    }
    profile.temperature = static_cast<float>(get_number_or(object, "temperature", profile.temperature));
    profile.repetition_penalty = static_cast<float>(get_number_or(object, "repetition_penalty", profile.repetition_penalty));
    profile.top_p = static_cast<float>(get_number_or(object, "top_p", profile.top_p));
    profile.top_k = static_cast<int>(get_number_or(object, "top_k", profile.top_k));
    profile.do_sample = get_bool_or(object, "do_sample", profile.do_sample);

    profile.notes = get_string_or(object, "notes", {});

    // Embedding pipeline konfiguracio az "embedding" szekicobol
    if (const auto* embedding_object = get_object_if_present(object, "embedding"); embedding_object != nullptr) {
        profile.embedding_enabled = get_bool_or(*embedding_object, "enabled", false);
        profile.embedding_model_name = get_string_or(*embedding_object, "model_name", "");
        profile.embedding_model_path = get_string_or(*embedding_object, "model_path", "");
        profile.embedding_device = get_string_or(*embedding_object, "device", "NPU");
        profile.embedding_reject_cpu_fallback = get_bool_or(*embedding_object, "reject_cpu_fallback", true);
        profile.embedding_endpoint_path = get_string_or(*embedding_object, "endpoint_path", "/v1/embeddings");
    }

    if (const auto* queue_object = get_object_if_present(object, "queue_policy"); queue_object != nullptr) {
        profile.queue_policy = masodik::runtime::load_queue_policy(JsonValue{*queue_object});
    }

    profile.validate();
    return profile;
}

}  // namespace masodik::config
