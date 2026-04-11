#include "masodik/genai/device_profile_evaluator.hpp"

namespace masodik::genai {

DeviceProfileEvaluation evaluate_device_profile(const masodik::runtime::ServiceConfig& service_config) {
    DeviceProfileEvaluation evaluation;
    evaluation.effective_device = service_config.effective_device;
    evaluation.hybrid_requested = service_config.serving_profile.hybrid_enabled;
    evaluation.gpu_offload_expected =
        evaluation.hybrid_requested || service_config.serving_profile.primary_device == "GPU.0";

    // A HETERO profil itt tudatos kompromisszum: a GPU memóriaterhét csökkenti, de PCIe-mozgatással járhat.
    if (evaluation.hybrid_requested) {
        evaluation.rationale =
            "A hybrid profil GPU.0 + CPU megosztassal probalja eletben tartani a 20B modellt korlatozott VRAM mellett.";
    } else {
        evaluation.rationale = "A profil tiszta eszkozkotest ker, hybrid fallback nelkul.";
    }

    return evaluation;
}

}  // namespace masodik::genai
