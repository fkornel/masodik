#include "test_registry.hpp"

#include "masodik/config/serving_profile.hpp"

MASODIK_TEST(serving_profile_loads_cut23_tail_split_configuration) {
    const auto profile = masodik::config::load_serving_profile(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json");

    masodik::tests::require(profile.profile_name == "b580-manual-split-cut23-tail-32k-u8", "A profile_name nem vart.");
    masodik::tests::require(profile.split_mode, "A manual split profilnak split_mode=true kell legyen.");
    masodik::tests::require(
        profile.split_stage_1_device == "GPU.1", "A stage-1 device-nak GPU.1-nek kell lennie.");
    masodik::tests::require(
        profile.split_stage_2_device == "GPU.0", "A stage-2 device-nak GPU.0-nek kell lennie.");
    masodik::tests::require(
        profile.queue_policy.max_concurrent_requests == 1U, "A split queue policy max 1 kerест kell korlatozzon.");
}
