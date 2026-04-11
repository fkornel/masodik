#pragma once

#include <chrono>
#include <string>

#include "masodik/http/http_client.hpp"

namespace masodik::http {

struct ChatContractValidationResult {
    bool valid = false;
    std::string endpoint_url;
    std::string response_excerpt;
    std::string error_message;
};

class ChatContractValidator {
public:
    ChatContractValidator(HttpClient http_client = {});

    ChatContractValidationResult validate(
        std::string_view primary_url,
        std::string_view fallback_url,
        std::string_view model_name,
        std::chrono::milliseconds timeout) const;

private:
    HttpClient http_client_;
};

}  // namespace masodik::http
