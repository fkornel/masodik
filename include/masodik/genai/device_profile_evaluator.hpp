#pragma once

#include <string>

#include "masodik/runtime/service_config.hpp"

namespace masodik::genai {

struct DeviceProfileEvaluation {
    std::string effective_device;
    bool hybrid_requested = false;
    bool gpu_offload_expected = false;
    bool gpu_offload_active = false;
    std::string rationale;
};

DeviceProfileEvaluation evaluate_device_profile(const masodik::runtime::ServiceConfig& service_config);

}  // namespace masodik::genai
