#pragma once

#include <exception>
#include <string>

#include "masodik/api/openai_contract.hpp"
#include "masodik/runtime/error_category.hpp"
#include "masodik/server/http_server.hpp"

namespace masodik::server {

struct ErrorMappingContext {
    bool tool_call_related = false;
    bool streaming = false;
};

struct ErrorResponseMapping {
    int status_code = 500;
    std::string code = "internal_error";
    std::string message;
    masodik::api::RequestValidationOutcome validation_outcome = masodik::api::RequestValidationOutcome::runtime_error;
};

masodik::api::RequestValidationOutcome classify_validation_outcome(
    const std::exception& error,
    ErrorMappingContext context = {});
[[nodiscard]] ::masodik::runtime::RuntimeFailureScope classify_failure_scope(const std::exception& error);
ErrorResponseMapping classify_exception(const std::exception& error, ErrorMappingContext context = {});
HttpResponse map_exception_to_response(const std::exception& error, ErrorMappingContext context = {});
std::string map_exception_to_terminal_sse_event(const std::exception& error, ErrorMappingContext context = {});

}  // namespace masodik::server
