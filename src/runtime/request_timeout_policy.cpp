#include "masodik/runtime/request_timeout_policy.hpp"

#include <algorithm>

namespace masodik::runtime {
namespace {

std::size_t estimate_request_prompt_tokens(const masodik::api::ChatCompletionRequest& request) {
    std::size_t characters = 0;
    for (const auto& message : request.messages) {
        characters += message.content.size();
    }
    return std::max<std::size_t>(1U, characters / 3U);
}

}

std::chrono::milliseconds RequestTimeoutPolicy::token_budget(const masodik::api::ChatCompletionRequest& request) const {
    const auto per_token_seconds = request.stream ? 1 : 2;
    const auto minimum_seconds = request.stream ? 15 : 30;
    return std::chrono::seconds{std::max(minimum_seconds, request.max_tokens / per_token_seconds)};
}

std::chrono::milliseconds RequestTimeoutPolicy::timeout_budget(
    const ServiceConfig& service_config,
    const masodik::api::ChatCompletionRequest& request) const {
    const auto profile_budget = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::seconds{service_config.serving_profile.timeout_seconds});
    auto budget = std::max(profile_budget, token_budget(request));
    if (service_config.split_mode) {
        const auto prompt_tokens = estimate_request_prompt_tokens(request);
        const auto total_tokens = prompt_tokens + static_cast<std::size_t>(std::max(1, request.max_tokens));
        const auto split_context_cap = service_config.generation_config.split_context_token_cap(
            service_config.model_repository.split_artifact_manifest.max_context_tokens);
        const auto bounded_total_tokens = std::min(total_tokens, split_context_cap);
        budget += std::chrono::seconds{static_cast<int>(std::max<std::size_t>(10U, bounded_total_tokens / 256U))};
    }
    return budget;
}

}  // namespace masodik::runtime
