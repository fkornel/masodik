#include <stdexcept>

#include "test_registry.hpp"

#include "masodik/genai/generation_config.hpp"

MASODIK_TEST(generation_config_reports_hetero_effective_device) {
    masodik::genai::GenerationConfig config;
    config.primary_device = "GPU.0";
    config.hybrid_enabled = true;
    config.hybrid_fallback_device = "CPU";

    masodik::tests::require(
        config.effective_device() == "HETERO:GPU.0,CPU",
        "A generation config effective_device kimenete nem vart.");
}

MASODIK_TEST(generation_config_rejects_empty_hybrid_fallback) {
    masodik::genai::GenerationConfig config;
    config.hybrid_enabled = true;
    config.hybrid_fallback_device.clear();

    try {
        config.validate();
    } catch (const std::invalid_argument&) {
        return;
    }

    throw std::runtime_error("Az ures hybrid_fallback_device ervenytelen kell legyen.");
}
