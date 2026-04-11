#include "manual_split_test_support.hpp"

#include "masodik/config/model_repository.hpp"

MASODIK_TEST(model_repository_loads_valid_split_manifest) {
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "split-manifest-valid"});
    const auto repository = masodik::config::inspect_model_repository(
        fixture.root_path,
        "AI Playground",
        true,
        true,
        "split-manifest.json");

    masodik::tests::require(repository.split_mode, "A split manifesthez split_mode=true repository vart.");
    masodik::tests::require(
        repository.split_artifact_manifest.validated_for_profile == "b580-manual-split",
        "A validated_for_profile nem vart.");
    masodik::tests::require(
        repository.split_artifact_manifest.source_model_checksum == "sha256:test-fixture",
        "A source_model_checksum nem vart.");
    masodik::tests::require(
        repository.split_artifact_manifest.cut_after_layer == "layers.17 -> layers.18",
        "A cut_after_layer nem vart.");
    masodik::tests::require(
        repository.split_artifact_manifest.validated_context_tokens == 32768,
        "A validated_context_tokens nem vart.");
    masodik::tests::require(
        repository.split_artifact_manifest.stage_1.expected_output ==
            repository.split_artifact_manifest.stage_2.expected_input,
        "A split manifest handoff szerzodesenek egyeznie kell.");
    masodik::tests::require(
        repository.split_artifact_manifest.stage_1.layer_range == "0-17",
        "A stage_1.layer_range nem vart.");
    masodik::tests::require(
        repository.split_artifact_manifest.stage_2.kv_budget_gb == 18.0,
        "A stage_2.kv_budget_gb nem vart.");
}

MASODIK_TEST(model_repository_rejects_missing_split_stage_artifact) {
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "split-manifest-missing-stage-2", .create_stage_2 = false});

    bool threw = false;
    try {
        (void)masodik::config::inspect_model_repository(
            fixture.root_path,
            "AI Playground",
            true,
            true,
            "split-manifest.json");
    } catch (const std::exception&) {
        threw = true;
    }

    masodik::tests::require(threw, "A hianyzo stage-2 artifactot a split manifest loadernek el kell utasitania.");
}
