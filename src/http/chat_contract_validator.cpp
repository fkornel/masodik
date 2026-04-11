#include "masodik/http/chat_contract_validator.hpp"

#include <algorithm>
#include <string>

#include "masodik/config/json_loader.hpp"

namespace masodik::http {
namespace {

ChatContractValidationResult validate_at_url(
    const HttpClient& http_client,
    const std::string_view url,
    const std::string_view model_name,
    const std::chrono::milliseconds timeout) {
    const auto request_body =
        std::string{"{\"model\":\""} + std::string{model_name} +
        "\",\"messages\":[{\"role\":\"user\",\"content\":\"Adj egy rovid magyar valaszt.\"}],\"stream\":false,\"max_tokens\":32}";

    auto response = http_client.post_json(url, request_body, timeout);
    ChatContractValidationResult result;
    result.endpoint_url = std::string{url};
    result.response_excerpt = response.body.substr(0, std::min<std::size_t>(response.body.size(), 300U));

    if (response.status_code != 200) {
        result.error_message = "A chat endpoint nem 200 valaszt adott: " + std::to_string(response.status_code);
        return result;
    }

    const auto root = masodik::config::JsonLoader::parse(response.body);
    if (!root.is_object()) {
        result.error_message = "A chat endpoint valasza nem JSON objektum.";
        return result;
    }

    const auto& object = root.as_object();
    const auto* choices_value = masodik::config::find_member(object, "choices");
    if (choices_value == nullptr || !choices_value->is_array() || choices_value->as_array().empty()) {
        result.error_message = "A chat endpoint valaszaban nincs hasznalhato choices tomb.";
        return result;
    }

    result.valid = true;
    return result;
}

}  // namespace

ChatContractValidator::ChatContractValidator(HttpClient http_client) : http_client_(std::move(http_client)) {}

ChatContractValidationResult ChatContractValidator::validate(
    const std::string_view primary_url,
    const std::string_view fallback_url,
    const std::string_view model_name,
    const std::chrono::milliseconds timeout) const {
    auto primary = validate_at_url(http_client_, primary_url, model_name, timeout);
    if (primary.valid) {
        return primary;
    }
    if (fallback_url.empty()) {
        return primary;
    }

    auto fallback = validate_at_url(http_client_, fallback_url, model_name, timeout);
    if (fallback.valid) {
        return fallback;
    }

    fallback.error_message = primary.error_message + " | Fallback: " + fallback.error_message;
    return fallback;
}

}  // namespace masodik::http
