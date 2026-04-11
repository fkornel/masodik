#include "manual_split_test_support.hpp"

#include "masodik/config/serving_profile.hpp"

MASODIK_TEST(serving_profile_loads_split_cut23_tail_32k_u8_profile) {
    const auto profile = masodik::config::load_serving_profile(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json");

    masodik::tests::require(profile.split_mode, "A cut23 tail profilnak split_mode=true kell legyen.");
    masodik::tests::require(profile.split_stage_1_device == "GPU.1", "A cut23 tail profil stage-1 device nem vart.");
    masodik::tests::require(profile.split_stage_2_device == "GPU.0", "A cut23 tail profil stage-2 device nem vart.");
    masodik::tests::require(profile.desired_context_tokens == 32768, "A cut23 tail profilnak 32768 context tokent kell kernie.");
    masodik::tests::require(profile.kv_cache_precision == "u8", "A cut23 tail profilnak u8 KV-cache precisiont kell kernie.");
}

MASODIK_TEST(serving_profile_rejects_same_device_split_topology) {
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "split-profile-same-device", .stage_2_device = "GPU.1"});

    bool threw = false;
    try {
        (void)masodik::config::load_serving_profile(fixture.profile_path);
    } catch (const std::exception&) {
        threw = true;
    }

    masodik::tests::require(threw, "Az azonos stage device-u split profilt el kell utasitani.");
}
