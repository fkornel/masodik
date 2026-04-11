#include "masodik/server/openai_error_mapper.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "masodik/api/openai_contract.hpp"

namespace masodik::server {
namespace {

std::string lowercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool contains_any(const std::string_view haystack, const std::initializer_list<std::string_view> needles) {
    for (const auto needle : needles) {
        if (haystack.find(needle) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

bool contains_memory_budget_marker(const std::string_view message) {
    return contains_any(message, {"memory budget", "memoria-budget", "context budget", "context cap"});
}

bool contains_overload_marker(const std::string_view message) {
    return contains_any(message, {"varakozasi sor", "queue policy", "reject_new", "backpressure", "tulterheles"});
}

}  // namespace

masodik::api::RequestValidationOutcome classify_validation_outcome(const std::exception& error, const ErrorMappingContext) {
    const auto message = lowercase_ascii(error.what());

    if (dynamic_cast<const std::invalid_argument*>(&error) != nullptr) {
        if (contains_any(message, {"tool", "tools[", "tool_choice"})) {
            return masodik::api::RequestValidationOutcome::tool_schema_error;
        }
        if (contains_any(message, {"model", "modell"})) {
            return masodik::api::RequestValidationOutcome::model_selection_error;
        }
        return masodik::api::RequestValidationOutcome::parser_error;
    }
    if (dynamic_cast<const std::logic_error*>(&error) != nullptr) {
        return masodik::api::RequestValidationOutcome::runtime_readiness_error;
    }
    return masodik::api::RequestValidationOutcome::runtime_error;
}

::masodik::runtime::RuntimeFailureScope classify_failure_scope(const std::exception& error) {
    const auto message = lowercase_ascii(error.what());
    if (contains_any(message, {"handoff", "hidden state", "hidden-state"})) {
        return ::masodik::runtime::RuntimeFailureScope::handoff;
    }
    if (contains_memory_budget_marker(message)) {
        if (contains_any(message, {"stage-1", "stage 1", "stage_1"})) {
            return ::masodik::runtime::RuntimeFailureScope::stage_1;
        }
        if (contains_any(message, {"stage-2", "stage 2", "stage_2"})) {
            return ::masodik::runtime::RuntimeFailureScope::stage_2;
        }
        return ::masodik::runtime::RuntimeFailureScope::handoff;
    }
    if (contains_any(message, {"stage-1", "stage 1", "stage_1"})) {
        return ::masodik::runtime::RuntimeFailureScope::stage_1;
    }
    if (contains_any(message, {"stage-2", "stage 2", "stage_2"})) {
        return ::masodik::runtime::RuntimeFailureScope::stage_2;
    }
    if (contains_any(message, {"split", "manifest", "startup"})) {
        return ::masodik::runtime::RuntimeFailureScope::startup;
    }
    return ::masodik::runtime::RuntimeFailureScope::none;
}

ErrorResponseMapping classify_exception(const std::exception& error, const ErrorMappingContext context) {
    const auto validation_outcome = classify_validation_outcome(error, context);
    const auto failure_scope = classify_failure_scope(error);
    if (dynamic_cast<const std::invalid_argument*>(&error) != nullptr) {
        if (context.tool_call_related) {
            return {.status_code = 400,
                    .code = "tool_validation_error",
                    .message = error.what(),
                    .validation_outcome = validation_outcome};
        }
        return {.status_code = 400, .code = "bad_request", .message = error.what(), .validation_outcome = validation_outcome};
    }
    if (dynamic_cast<const std::logic_error*>(&error) != nullptr) {
        if (context.tool_call_related) {
            return {.status_code = 503,
                    .code = "tool_call_unavailable",
                    .message = error.what(),
                    .validation_outcome = validation_outcome};
        }
        return {.status_code = 503,
                .code = "service_unavailable",
                .message = error.what(),
                .validation_outcome = validation_outcome};
    }
    if (dynamic_cast<const std::runtime_error*>(&error) != nullptr) {
        if (context.tool_call_related && context.streaming) {
            return {.status_code = 503,
                    .code = "tool_stream_error",
                    .message = error.what(),
                    .validation_outcome = validation_outcome};
        }
        if (context.tool_call_related) {
            return {.status_code = 503,
                    .code = "tool_runtime_error",
                    .message = error.what(),
                    .validation_outcome = validation_outcome};
        }
        if (contains_memory_budget_marker(lowercase_ascii(error.what()))) {
            return {.status_code = 503,
                    .code = "split_memory_budget_error",
                    .message = error.what(),
                    .validation_outcome = validation_outcome};
        }
        if (contains_overload_marker(lowercase_ascii(error.what()))) {
            return {.status_code = 503,
                    .code = "split_overload_error",
                    .message = error.what(),
                    .validation_outcome = validation_outcome};
        }
        if (failure_scope == ::masodik::runtime::RuntimeFailureScope::handoff) {
            return {.status_code = 503,
                    .code = "split_handoff_error",
                    .message = error.what(),
                    .validation_outcome = validation_outcome};
        }
        if (failure_scope == ::masodik::runtime::RuntimeFailureScope::stage_1 ||
            failure_scope == ::masodik::runtime::RuntimeFailureScope::stage_2) {
            return {.status_code = 503,
                    .code = "split_stage_error",
                    .message = error.what(),
                    .validation_outcome = validation_outcome};
        }
        if (failure_scope == ::masodik::runtime::RuntimeFailureScope::startup) {
            return {.status_code = 503,
                    .code = "split_startup_error",
                    .message = error.what(),
                    .validation_outcome = validation_outcome};
        }
        return {.status_code = 503, .code = "runtime_error", .message = error.what(), .validation_outcome = validation_outcome};
    }
    return {.status_code = 500, .code = "internal_error", .message = error.what(), .validation_outcome = validation_outcome};
}

HttpResponse map_exception_to_response(const std::exception& error, const ErrorMappingContext context) {
    const auto mapping = classify_exception(error, context);
    return {.status_code = mapping.status_code, .body = masodik::api::to_json_error(mapping.code, mapping.message)};
}

std::string map_exception_to_terminal_sse_event(const std::exception& error, const ErrorMappingContext context) {
    const auto mapping = classify_exception(error, context);
    return masodik::api::to_sse_error_event(mapping.code, mapping.message);
}

}  // namespace masodik::server
