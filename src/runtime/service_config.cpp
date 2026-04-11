#include "masodik/runtime/service_config.hpp"

#include <stdexcept>

namespace masodik::runtime {
namespace {

std::string build_url(const std::string_view host, const std::uint16_t port, const std::string_view path) {
    return "http://" + std::string{host} + ":" + std::to_string(port) + std::string{path};
}

}  // namespace

void ServiceConfig::validate() const {
    toolchain.validate();
    model_repository.validate();
    serving_profile.validate();
    generation_config.validate();
    if (active_model.empty()) {
        throw std::invalid_argument("Az active_model nem lehet ures.");
    }
    if (active_profile.empty()) {
        throw std::invalid_argument("Az active_profile nem lehet ures.");
    }
    if (split_mode) {
        if (model_repository.split_artifact_manifest.validated_for_profile != active_profile) {
            throw std::invalid_argument("A split manifest validated_for_profile mezoje nem egyezik az aktiv profillal.");
        }
        if (split_profile.empty()) {
            throw std::invalid_argument("Split modban a split_profile nem lehet ures.");
        }
        if (split_manifest_path.empty()) {
            throw std::invalid_argument("Split modban a split_manifest_path nem lehet ures.");
        }
    }
    if (effective_device.empty()) {
        throw std::invalid_argument("Az effective_device nem lehet ures.");
    }
    if (bind_host.empty()) {
        throw std::invalid_argument("A bind_host nem lehet ures.");
    }
    if (bind_port == 0U) {
        throw std::invalid_argument("A bind_port nem lehet 0.");
    }
    if (health_endpoint_path.empty() || chat_endpoint_path.empty() || models_endpoint_path.empty()) {
        throw std::invalid_argument("A szolgaltatas endpoint path mezoi nem lehetnek uresek.");
    }
    if (health_url.empty() || chat_url.empty() || models_url.empty()) {
        throw std::invalid_argument("A szolgaltatas endpoint URL-jei nem lehetnek uresek.");
    }
}

ServiceConfig build_service_config(
    const std::filesystem::path& profile_path,
    const std::filesystem::path& model_root,
    const std::filesystem::path& repo_root) {
    ServiceConfig config;
    config.repo_root = repo_root;
    config.toolchain = masodik::config::discover_default_toolchain_profile();
    config.serving_profile = masodik::config::load_serving_profile(profile_path);
    config.model_repository = masodik::config::inspect_model_repository(
        model_root,
        "AI Playground",
        true,
        config.serving_profile.split_mode,
        config.serving_profile.split_manifest_path);
    config.runtime_dependencies = masodik::config::inspect_runtime_dependency_report(config.toolchain, config.model_repository);
    config.generation_config.primary_device = config.serving_profile.primary_device;
    config.generation_config.hybrid_enabled = config.serving_profile.hybrid_enabled;
    config.generation_config.hybrid_fallback_device = config.serving_profile.hybrid_fallback_device;
    config.generation_config.split_mode = config.serving_profile.split_mode;
    config.generation_config.stage_1_device = config.serving_profile.split_stage_1_device;
    config.generation_config.stage_2_device = config.serving_profile.split_stage_2_device;
    config.generation_config.reject_cpu_fallback = config.serving_profile.reject_cpu_fallback;
    config.generation_config.kv_cache_precision = config.serving_profile.kv_cache_precision;
    config.generation_config.split_diagnostics_level = config.serving_profile.split_diagnostics_level;
    config.generation_config.stage_1_memory_budget_gb = config.serving_profile.split_stage_1_memory_budget_gb;
    config.generation_config.stage_2_memory_budget_gb = config.serving_profile.split_stage_2_memory_budget_gb;
    config.generation_config.cache_directory = config.serving_profile.cache_directory;
    config.generation_config.desired_context_tokens = config.serving_profile.desired_context_tokens;
    config.generation_config.temperature = config.serving_profile.temperature;
    config.generation_config.repetition_penalty = config.serving_profile.repetition_penalty;
    config.generation_config.top_p = config.serving_profile.top_p;
    config.generation_config.top_k = config.serving_profile.top_k;
    config.generation_config.do_sample = config.serving_profile.do_sample;
    config.split_mode = config.serving_profile.split_mode;
    config.split_profile = config.serving_profile.split_mode ? config.serving_profile.profile_name : std::string{};
    config.split_manifest_path = config.model_repository.split_manifest_path;
    config.active_model = config.model_repository.model_id;
    config.active_profile = config.serving_profile.profile_name;
    config.effective_device = config.generation_config.effective_device();
    config.bind_host = config.serving_profile.bind_host;
    config.bind_port = config.serving_profile.bind_port;
    config.health_endpoint_path = config.serving_profile.health_endpoint_path;
    config.chat_endpoint_path = config.serving_profile.chat_endpoint_path;
    config.models_endpoint_path = config.serving_profile.models_endpoint_path;
    config.health_url = build_url(config.bind_host, config.bind_port, config.health_endpoint_path);
    config.chat_url = build_url(config.bind_host, config.bind_port, config.chat_endpoint_path);
    config.models_url = build_url(config.bind_host, config.bind_port, config.models_endpoint_path);
    config.validate();
    return config;
}

}  // namespace masodik::runtime
