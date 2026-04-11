#pragma once

#include <chrono>

#include "masodik/api/openai_contract.hpp"
#include "masodik/runtime/service_config.hpp"

namespace masodik::runtime {

class RequestTimeoutPolicy {
public:
    [[nodiscard]] std::chrono::milliseconds timeout_budget(
        const ServiceConfig& service_config,
        const masodik::api::ChatCompletionRequest& request) const;

    [[nodiscard]] std::chrono::milliseconds token_budget(const masodik::api::ChatCompletionRequest& request) const;
};

}  // namespace masodik::runtime
