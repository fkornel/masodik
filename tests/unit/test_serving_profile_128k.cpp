#include "test_registry.hpp"

#include "masodik/config/serving_profile.hpp"

MASODIK_TEST(serving_profile_loads_128k_deep_split_configuration) {
    const auto repo_root = masodik::tests::find_repo_root();
    const auto profile_path = repo_root / "config" / "profiles" / "b580-deep-split-128k.json";
    const auto profile = masodik::config::load_serving_profile(profile_path);

    masodik::tests::require(profile.profile_name == "b580-deep-split-128k", "A profil neve nem vart.");
    masodik::tests::require(profile.split_mode == true, "A split_mode true kell legyen.");
    masodik::tests::require(profile.split_stage_1_device == "GPU.1", "A stage-1 device GPU.1 kell legyen.");
    masodik::tests::require(profile.split_stage_2_device == "GPU.0", "A stage-2 device GPU.0 kell legyen.");
    masodik::tests::require(profile.reject_cpu_fallback == true, "A reject_cpu_fallback true kell legyen.");
    masodik::tests::require(profile.kv_cache_precision == "u8", "A kv_cache_precision u8 kell legyen.");
    masodik::tests::require(profile.desired_context_tokens == 131072, "A desired_context_tokens 131072 kell legyen.");
    masodik::tests::require(profile.split_stage_1_memory_budget_gb == 12.0, "A stage-1 budget 12.0 GB kell legyen.");
    masodik::tests::require(profile.split_stage_2_memory_budget_gb == 8.0, "A stage-2 budget 8.0 GB kell legyen.");
    masodik::tests::require(profile.bind_port == 29011, "A bind_port 29011 kell legyen.");
    masodik::tests::require(profile.embedding_enabled == true, "Az embedding_enabled true kell legyen.");
    masodik::tests::require(profile.embedding_device == "NPU", "Az embedding device NPU kell legyen.");
}

MASODIK_TEST(serving_profile_128k_and_32k_profiles_coexist) {
    const auto repo_root = masodik::tests::find_repo_root();

    const auto profile_32k = masodik::config::load_serving_profile(
        repo_root / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json");
    const auto profile_128k = masodik::config::load_serving_profile(
        repo_root / "config" / "profiles" / "b580-deep-split-128k.json");

    masodik::tests::require(profile_32k.desired_context_tokens == 32768, "A 32k profil context 32768 kell legyen.");
    masodik::tests::require(profile_128k.desired_context_tokens == 131072, "A 128k profil context 131072 kell legyen.");
    masodik::tests::require(profile_32k.desired_context_tokens != profile_128k.desired_context_tokens,
        "A ket profil context tokenje kulonbozo kell legyen.");
}
