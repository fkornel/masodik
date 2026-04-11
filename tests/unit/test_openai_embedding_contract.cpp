#include "test_registry.hpp"

#include <stdexcept>

#include "masodik/api/openai_contract.hpp"

MASODIK_TEST(embedding_contract_parses_single_string_input) {
    const auto request = masodik::api::parse_embedding_request(
        R"({"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":"Hello vilag"})");

    masodik::tests::require(request.model == "Qwen3-Embedding-0.6B-int4-cw-ov", "A model azonosito nem vart.");
    masodik::tests::require(request.input.size() == 1U, "Egyetlen input vart.");
    masodik::tests::require(request.input.front() == "Hello vilag", "Az input szoveg nem vart.");
    masodik::tests::require(request.encoding_format == "float", "Az alapertelmezett encoding_format 'float' kell legyen.");
}

MASODIK_TEST(embedding_contract_parses_array_input) {
    const auto request = masodik::api::parse_embedding_request(
        R"({"model":"test-model","input":["foo","bar","baz"]})");

    masodik::tests::require(request.model == "test-model", "A model azonosito nem vart.");
    masodik::tests::require(request.input.size() == 3U, "Harom input vart.");
    masodik::tests::require(request.input[0] == "foo", "Az elso input nem vart.");
    masodik::tests::require(request.input[1] == "bar", "A masodik input nem vart.");
    masodik::tests::require(request.input[2] == "baz", "A harmadik input nem vart.");
}

MASODIK_TEST(embedding_contract_rejects_empty_input_string) {
    bool threw = false;
    try {
        masodik::api::parse_embedding_request(
            R"({"model":"test-model","input":""})");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    masodik::tests::require(threw, "Ures input string eseten std::invalid_argument vart.");
}

MASODIK_TEST(embedding_contract_rejects_empty_input_array) {
    bool threw = false;
    try {
        masodik::api::parse_embedding_request(
            R"({"model":"test-model","input":[]})");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    masodik::tests::require(threw, "Ures input tomb eseten std::invalid_argument vart.");
}

MASODIK_TEST(embedding_contract_rejects_missing_model) {
    bool threw = false;
    try {
        masodik::api::parse_embedding_request(
            R"({"input":"hello"})");
    } catch (const std::exception&) {
        threw = true;
    }
    masodik::tests::require(threw, "Hianyzó model mezo eseten exception vart.");
}

MASODIK_TEST(embedding_contract_rejects_unsupported_encoding_format) {
    bool threw = false;
    try {
        masodik::api::parse_embedding_request(
            R"({"model":"test-model","input":"hello","encoding_format":"base64"})");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    masodik::tests::require(threw, "Nem tamogatott encoding_format eseten std::invalid_argument vart.");
}

MASODIK_TEST(embedding_contract_serializes_response) {
    masodik::api::EmbeddingResponse response;
    response.model = "test-model";
    response.data.push_back({.object = "embedding", .embedding = {0.1f, 0.2f, 0.3f}, .index = 0});
    response.usage = {.prompt_tokens = 5, .total_tokens = 5};

    const auto json = masodik::api::to_json(response);

    masodik::tests::require(json.find("\"object\":\"list\"") != std::string::npos, "A response object 'list' kell legyen.");
    masodik::tests::require(json.find("\"model\":\"test-model\"") != std::string::npos, "A model nem vart.");
    masodik::tests::require(json.find("\"prompt_tokens\":5") != std::string::npos, "A prompt_tokens nem vart.");
    masodik::tests::require(json.find("\"total_tokens\":5") != std::string::npos, "A total_tokens nem vart.");
    masodik::tests::require(json.find("\"index\":0") != std::string::npos, "Az index nem vart.");
}
