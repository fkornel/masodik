#pragma once
#include <algorithm>
#include <cstddef>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace masodik::genai {

struct GenerationConfig {
    std::string primary_device = "GPU.0";
    bool hybrid_enabled = false;
    std::string hybrid_fallback_device = "CPU";
    bool split_mode = false;
    std::string stage_1_device = "GPU.1";
    std::string stage_2_device = "GPU.0";
    bool reject_cpu_fallback = false;
    std::string kv_cache_precision;
    std::string split_diagnostics_level = "standard";
    double stage_1_memory_budget_gb = 0.0;
    double stage_2_memory_budget_gb = 0.0;
    std::filesystem::path cache_directory = std::filesystem::path{"build"} / "cache";
    int max_new_tokens = 512;
    int desired_context_tokens = 32768;

    // Mintavetelesi parameterek — a repetitiv loop-ok es a kreativ valaszok finomhangolasahoz.
    // Racionále: a gpt-oss-20b modell hajlamos token-ismetlesre (loop), kulonosen
    // magyar nyelvu rovid valaszoknal. A repetition_penalty > 1.0 bunteti az ismetlest,
    // a temperature < 1.0 determinisztikusabba teszi a valaszt, a top_p/top_k szukiti
    // a mintavetelesi teret.
    float temperature = 0.7f;
    float repetition_penalty = 1.15f;
    float top_p = 0.9f;
    int top_k = 50;
    bool do_sample = true;

    void validate() const {
        if (primary_device.empty()) {
            throw std::invalid_argument("A primary_device nem lehet ures.");
        }
        if (hybrid_enabled && hybrid_fallback_device.empty()) {
            throw std::invalid_argument("Hybrid profil eseten kotelezo a hybrid_fallback_device.");
        }
        if (split_mode) {
            if (stage_1_device.empty() || stage_2_device.empty()) {
                throw std::invalid_argument("Split modban a ket stage eszkoz kotelezo.");
            }
            if (stage_1_device == stage_2_device) {
                throw std::invalid_argument("Split modban a ket stage eszkoz nem lehet azonos.");
            }
            if (!reject_cpu_fallback) {
                throw std::invalid_argument("Split modban a CPU fallback tiltasa kotelezo.");
            }
            if (stage_1_memory_budget_gb <= 0.0 || stage_2_memory_budget_gb <= 0.0) {
                throw std::invalid_argument("Split modban a stage memoria-budget csak pozitiv lehet.");
            }
        }
        if (max_new_tokens <= 0) {
            throw std::invalid_argument("A max_new_tokens csak pozitiv lehet.");
        }
        if (desired_context_tokens <= 0) {
            throw std::invalid_argument("A desired_context_tokens csak pozitiv lehet.");
        }
    }

    [[nodiscard]] std::string effective_device() const {
        if (split_mode) {
            return "MANUAL_SPLIT:" + stage_1_device + "->" + stage_2_device;
        }
        return hybrid_enabled ? "HETERO:" + primary_device + "," + hybrid_fallback_device : primary_device;
    }

    [[nodiscard]] std::size_t split_context_token_cap(const int manifest_max_context_tokens) const {
        const auto desired_cap = (std::max)(1, desired_context_tokens);
        const auto manifest_cap = (std::max)(1, manifest_max_context_tokens);
        return static_cast<std::size_t>((std::min)(desired_cap, manifest_cap));
    }

    [[nodiscard]] double split_context_utilization(
        const std::size_t total_tokens,
        const std::size_t split_context_cap) const {
        const auto normalized_cap = (std::max)(std::size_t{1}, split_context_cap);
        const auto clamped_total_tokens = (std::min)(total_tokens, normalized_cap);
        return static_cast<double>(clamped_total_tokens) / static_cast<double>(normalized_cap);
    }

    [[nodiscard]] double split_stage_required_budget_gb(
        const std::size_t total_tokens,
        const std::size_t split_context_cap,
        const double full_context_budget_gb) const {
        if (full_context_budget_gb <= 0.0) {
            return 0.0;
        }
        return split_context_utilization(total_tokens, split_context_cap) * full_context_budget_gb;
    }

    [[nodiscard]] double split_stage_budget_cap_gb(
        const double configured_budget_gb,
        const double manifest_budget_gb) const {
        if (configured_budget_gb <= 0.0) {
            return manifest_budget_gb;
        }
        if (manifest_budget_gb <= 0.0) {
            return configured_budget_gb;
        }
        return (std::min)(configured_budget_gb, manifest_budget_gb);
    }
};

}  // namespace masodik::genai
