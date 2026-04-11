#include "test_registry.hpp"

#include "masodik/api/openai_contract.hpp"
#include "masodik/genai/embedding_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/server/openai_handlers.hpp"

namespace {

// Teszt segitseg: letrehoz egy ServiceConfig-ot embedding konfiguracioval
masodik::runtime::ServiceConfig make_embedding_test_config() {
    const auto repo_root = masodik::tests::find_repo_root();
    auto config = masodik::runtime::build_service_config(
        repo_root / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        repo_root / "build" / "generated-manual-split-cut23-tail-32k-u8",
        repo_root);
    return config;
}

}  // namespace

// T011: Egyedi embedding keres — a pipeline nincs inicializalva,
// tehat 503-at varunk (service unavailable).
// A valodi NPU-s happy path a smoke tesztben (T018) ellenorizheto.
MASODIK_TEST(embedding_endpoint_returns_503_when_pipeline_not_ready) {
    auto config = make_embedding_test_config();
    masodik::genai::EmbeddingPipelineService embedding_service;
    // Nem hivjuk az initialize()-t → state = uninitialized

    masodik::server::HttpRequest request;
    request.body = R"({"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":"Hello"})";

    const auto response = masodik::server::handle_embeddings_request(request, config, embedding_service);

    masodik::tests::require(response.status_code == 503, "A nem kesz embedding pipeline-nal 503 vart.");
    masodik::tests::require(
        response.body.find("service_unavailable") != std::string::npos,
        "A 503 valaszban 'service_unavailable' kod vart.");
}

// T012: Ismeretlen modell → 404
MASODIK_TEST(embedding_endpoint_returns_404_for_unknown_model) {
    auto config = make_embedding_test_config();
    masodik::genai::EmbeddingPipelineService embedding_service;

    masodik::server::HttpRequest request;
    request.body = R"({"model":"nonexistent-model","input":"Hello"})";

    const auto response = masodik::server::handle_embeddings_request(request, config, embedding_service);

    masodik::tests::require(response.status_code == 404, "Ismeretlen modell eseten 404 vart.");
    masodik::tests::require(
        response.body.find("not_found") != std::string::npos,
        "A 404 valaszban 'not_found' kod vart.");
}

// T013: Ures input → 400
MASODIK_TEST(embedding_endpoint_returns_400_for_empty_input) {
    auto config = make_embedding_test_config();
    masodik::genai::EmbeddingPipelineService embedding_service;

    masodik::server::HttpRequest request;
    request.body = R"({"model":"Qwen3-Embedding-0.6B-int4-cw-ov","input":""})";

    const auto response = masodik::server::handle_embeddings_request(request, config, embedding_service);

    masodik::tests::require(response.status_code == 400, "Ures input eseten 400 vart.");
    masodik::tests::require(
        response.body.find("bad_request") != std::string::npos,
        "A 400 valaszban 'bad_request' kod vart.");
}

// T013 kiegeszites: Ervenytelen JSON → 400
MASODIK_TEST(embedding_endpoint_returns_400_for_invalid_json) {
    auto config = make_embedding_test_config();
    masodik::genai::EmbeddingPipelineService embedding_service;

    masodik::server::HttpRequest request;
    request.body = "not valid json";

    const auto response = masodik::server::handle_embeddings_request(request, config, embedding_service);

    masodik::tests::require(response.status_code == 400 || response.status_code == 500,
        "Ervenytelen JSON eseten hiba valasz vart.");
}
