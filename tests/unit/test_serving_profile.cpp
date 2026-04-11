#include "test_registry.hpp"

#include "masodik/config/serving_profile.hpp"

MASODIK_TEST(serving_profile_materializes_b580_cut23_tail_profile) {
    const auto profile = masodik::config::load_serving_profile(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json");

    masodik::tests::require(profile.split_mode, "A b580-manual-split-cut23-tail profilnak split_mode=true kell legyen.");
    masodik::tests::require(profile.split_stage_1_device == "GPU.1", "A cut23-tail stage-1 device nem vart.");
    masodik::tests::require(profile.split_stage_2_device == "GPU.0", "A cut23-tail stage-2 device nem vart.");
    masodik::tests::require(
        profile.desired_context_tokens == 32768,
        "A b580-manual-split-cut23-tail profilnak 32768 context tokent kell kernie.");
    masodik::tests::require(profile.kv_cache_precision == "u8", "A cut23-tail profilnak u8 KV-cache precisiont kell kernie.");
    masodik::tests::require(profile.reject_cpu_fallback, "A cut23-tail profilnak tiltania kell a CPU fallbackot.");
}
