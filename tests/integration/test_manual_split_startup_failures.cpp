#include "manual_split_test_support.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(manual_split_startup_fails_for_incompatible_stage_contract) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-incompatible-contract", .stage_2_expected_input = "logits only"});

    bool threw = false;
    try {
        (void)masodik::runtime::build_service_config(
            fixture.profile_path,
            fixture.root_path,
            masodik::tests::find_repo_root());
    } catch (const std::exception&) {
        threw = true;
    }

    masodik::tests::require(threw, "Az inkompatibilis split handoff szerzodest a startupnak el kell utasitani.");
}

MASODIK_TEST(manual_split_startup_fails_for_profile_manifest_mismatch) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-profile-mismatch", .validated_for_profile = "masik-profil"});

    bool threw = false;
    try {
        (void)masodik::runtime::build_service_config(
            fixture.profile_path,
            fixture.root_path,
            masodik::tests::find_repo_root());
    } catch (const std::exception&) {
        threw = true;
    }

    masodik::tests::require(threw, "A validated_for_profile eltereset a startupnak el kell utasitani.");
}
