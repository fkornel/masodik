#include "test_registry.hpp"

#include "masodik/config/serving_profile.hpp"

MASODIK_TEST(b580_cut23_tail_profile_loads_correctly) {
    const auto profile = masodik::config::load_serving_profile(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json");

    masodik::tests::require(
        profile.profile_name == "b580-manual-split-cut23-tail-32k-u8",
        "b580-manual-split-cut23-tail-32k-u8 profil neve nem vart.");
    masodik::tests::require(profile.split_mode, "b580-cut23-tail profilnak split_mode=true kell legyen.");
    masodik::tests::require(
        profile.split_stage_1_device == "GPU.1", "b580-cut23-tail stage-1 device-nak GPU.1-nek kell lennie.");
    masodik::tests::require(
        profile.split_stage_2_device == "GPU.0", "b580-cut23-tail stage-2 device-nak GPU.0-nek kell lennie.");
    masodik::tests::require(
        profile.desired_context_tokens == 32768, "b580-cut23-tail context meret nem vart.");
    masodik::tests::require(profile.bind_port == 29015U, "b580-cut23-tail portnak 29015-nek kell lennie.");
    masodik::tests::require(profile.reject_cpu_fallback, "b580-cut23-tail CPU fallback tiltasa nem aktiv.");
    masodik::tests::require(profile.kv_cache_precision == "u8", "b580-cut23-tail kv_cache_precision u8 kell legyen.");
}

MASODIK_TEST(b580_cut23_tail_profile_split_stage_devices_differ) {
    const auto profile = masodik::config::load_serving_profile(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json");

    masodik::tests::require(
        profile.split_stage_1_device != profile.split_stage_2_device,
        "A ket stage-nek kulon GPU-n kell futnia (GPU.1 vs GPU.0).");
}

MASODIK_TEST(b580_cut23_tail_profile_context_window_is_32k) {
    const auto profile = masodik::config::load_serving_profile(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json");

    masodik::tests::require(
        profile.desired_context_tokens == 32768U,
        "A cut23-tail profilnak 32768 token kontextust kell kerelmeznie.");
}

