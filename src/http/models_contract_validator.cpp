#include "masodik/http/models_contract_validator.hpp"

#include <algorithm>

#include "masodik/config/json_loader.hpp"

namespace masodik::http {
namespace {

ModelsContractValidationResult validate_at_url(
    const HttpClient& http_client,
    const std::string_view url,
    const std::string_view model_name,
    const std::chrono::milliseconds timeout) {
    auto response = http_client.get(url, timeout);
    ModelsContractValidationResult result;
    result.endpoint_url = std::string{url};

    if (response.status_code != 200) {
        result.error_message = "A models endpoint nem 200 valaszt adott: " + std::to_string(response.status_code);
        return result;
    }

    const auto root = masodik::config::JsonLoader::parse(response.body);
    if (!root.is_object()) {
        result.error_message = "A models endpoint valasza nem JSON objektum.";
        return result;
    }

    const auto& object = root.as_object();
    const auto* data_value = masodik::config::find_member(object, "data");
    if (data_value == nullptr || !data_value->is_array()) {
        result.error_message = "A models endpoint valaszaban nincs data tomb.";
        return result;
    }

    for (const auto& item : data_value->as_array()) {
        if (!item.is_object()) {
            continue;
        }
        const auto* id_value = masodik::config::find_member(item.as_object(), "id");
        if (id_value != nullptr && id_value->is_string() && id_value->as_string() == model_name) {
            result.model_found = true;
            break;
        }
    }

    result.valid = true;
    if (!result.model_found) {
        result.error_message = "A models endpoint valaszaban nem szerepel a vart modellazonosito.";
    }
    return result;
}

}  // namespace

ModelsContractValidator::ModelsContractValidator(HttpClient http_client) : http_client_(std::move(http_client)) {}

ModelsContractValidationResult ModelsContractValidator::validate(
    const std::string_view primary_url,
    const std::string_view fallback_url,
    const std::string_view model_name,
    const std::chrono::milliseconds timeout) const {
    auto primary = validate_at_url(http_client_, primary_url, model_name, timeout);
    if (primary.valid && primary.model_found) {
        return primary;
    }
    if (fallback_url.empty()) {
        return primary;
    }

    auto fallback = validate_at_url(http_client_, fallback_url, model_name, timeout);
    if (fallback.valid && fallback.model_found) {
        return fallback;
    }

    if (!primary.error_message.empty() && !fallback.error_message.empty()) {
        fallback.error_message = primary.error_message + " | Fallback: " + fallback.error_message;
    }
    return fallback;
}

}  // namespace masodik::http
