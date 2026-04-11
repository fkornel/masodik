#pragma once

#include <chrono>
#include <string>

#include "masodik/http/http_client.hpp"

namespace masodik::http {

struct ModelsContractValidationResult {
    bool valid = false;
    std::string endpoint_url;
    bool model_found = false;
    std::string error_message;
};

class ModelsContractValidator {
public:
    ModelsContractValidator(HttpClient http_client = {});

    ModelsContractValidationResult validate(
        std::string_view primary_url,
        std::string_view fallback_url,
        std::string_view model_name,
        std::chrono::milliseconds timeout) const;

private:
    HttpClient http_client_;
};

}  // namespace masodik::http
