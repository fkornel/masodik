#include "test_registry.hpp"

#include "masodik/config/serving_profile.hpp"

MASODIK_TEST(serving_profile_loads_embedding_config_from_active_profile) {
    const auto repo_root = masodik::tests::find_repo_root();
    const auto profile_path = repo_root / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json";
    const auto profile = masodik::config::load_serving_profile(profile_path);

    masodik::tests::require(profile.embedding_enabled == true, "Az embedding_enabled true kell legyen.");
    masodik::tests::require(
        profile.embedding_model_name == "Qwen3-Embedding-0.6B-int4-cw-ov",
        "Az embedding model_name nem vart.");
    masodik::tests::require(!profile.embedding_model_path.empty(), "Az embedding model_path nem lehet ures.");
    masodik::tests::require(profile.embedding_device == "NPU", "Az embedding device 'NPU' kell legyen.");
    masodik::tests::require(profile.embedding_reject_cpu_fallback == true, "Az embedding reject_cpu_fallback true kell legyen.");
    masodik::tests::require(profile.embedding_endpoint_path == "/v1/embeddings", "Az embedding endpoint_path nem vart.");
}

MASODIK_TEST(serving_profile_defaults_embedding_disabled_without_section) {
    // Ha nincs embedding szekció, az alapertelmezesek ervenyesulnek
    masodik::config::ServingProfile profile;
    masodik::tests::require(profile.embedding_enabled == false, "Az embedding_enabled alapertelmezetten false kell legyen.");
    masodik::tests::require(profile.embedding_device == "NPU", "Az embedding_device alapertelmezetten 'NPU' kell legyen.");
    masodik::tests::require(
        profile.embedding_endpoint_path == "/v1/embeddings",
        "Az embedding_endpoint_path alapertelmezetten '/v1/embeddings' kell legyen.");
}
