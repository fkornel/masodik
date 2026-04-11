#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "masodik/config/default_paths.hpp"
#include "masodik/runtime/queue_policy.hpp"

namespace masodik::config {

struct ServingProfile {
    std::string profile_name = "b580-manual-split-cut23-tail-32k-u8";
    std::string model_name = "gpt-oss-20b-int4-ov";
    std::string primary_device = "GPU.1";
    bool hybrid_enabled = false;
    std::string hybrid_fallback_device = "CPU";
    bool split_mode = false;
    std::string split_stage_1_device = "GPU.1";
    std::string split_stage_2_device = "GPU.0";
    bool reject_cpu_fallback = false;
    std::string kv_cache_precision;
    std::filesystem::path split_manifest_path = "split-manifest.json";
    std::string split_diagnostics_level = "standard";
    double split_stage_1_memory_budget_gb = 12.0;
    double split_stage_2_memory_budget_gb = 24.0;
    std::filesystem::path cache_directory = masodik::config::default_paths::default_cache_directory;
    std::filesystem::path runtime_log_path = masodik::config::default_paths::default_runtime_log_path;
    std::string health_endpoint_path = std::string{masodik::config::default_paths::default_health_endpoint_path};
    std::string chat_endpoint_path = std::string{masodik::config::default_paths::default_chat_endpoint_path};
    std::string models_endpoint_path = std::string{masodik::config::default_paths::default_models_endpoint_path};

    // Legacy mezok a baseline OVMS-kod ideiglenes forditasi kompatibilitasahoz.
    std::string target_device = "GPU.1";
    bool hetero_enabled = false;
    std::string hetero_fallback_device = "CPU";
    std::string model_distribution_policy;
    std::string bind_host = "127.0.0.1";
    std::uint16_t bind_port = 29011;
    int desired_context_tokens = 32768;
    int max_num_seqs = 256;
    bool prefix_caching_enabled = true;
    bool enable_runtime_assignment_diagnostics = false;
    masodik::runtime::QueuePolicy queue_policy{};
    bool cpu_fallback_enabled = false;
    int timeout_seconds = 14400;
    std::filesystem::path ovms_graph_path;
    std::filesystem::path ovms_log_path;
    std::string ready_endpoint = "/v2/health/ready";
    std::string chat_endpoint = "/v1/chat/completions";
    std::string models_endpoint = "/v1/models";
    // Mintavetelesi parameterek
    float temperature = 0.7f;
    float repetition_penalty = 1.15f;
    float top_p = 0.9f;
    int top_k = 50;
    bool do_sample = true;

    std::string notes;

    // Embedding pipeline konfiguracio (NPU)
    bool embedding_enabled = false;
    std::string embedding_model_name;
    std::filesystem::path embedding_model_path;
    std::string embedding_device = "NPU";
    bool embedding_reject_cpu_fallback = true;
    std::string embedding_endpoint_path = "/v1/embeddings";

    void validate() const;
};

ServingProfile load_serving_profile(const std::filesystem::path& file_path);

}  // namespace masodik::config
