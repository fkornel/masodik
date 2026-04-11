#include "manual_split_test_support.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/split/split_artifact_builder.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(manual_split_generated_artifact_is_startup_ready) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto source_fixture = masodik::tests::create_manual_split_source_fixture("manual-split-generated-artifact");
    const auto output_root = source_fixture.root_path / "generated-artifact";
    const auto artifact = masodik::split::build_split_artifact({
        .source_model_root = source_fixture.root_path,
        .output_root = output_root,
        .validated_context_tokens = 32768,
    });
    const auto profile_path =
        masodik::tests::write_manual_split_profile_for_root(artifact.artifact_root, "b580-manual-split", 29172, 32768);

    const auto service_config = masodik::runtime::build_service_config(
        profile_path,
        artifact.artifact_root,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    const auto health = pipeline_service.make_health_response();
    masodik::tests::require(pipeline_service.ready(), "A generated split artifact mellett a pipeline-nek ready-nek kell lennie.");
    masodik::tests::require(health.split_mode, "A generated split artifacttal a health split_mode=true allapotot var.");
    masodik::tests::require(
        health.split_artifact_version == "split-v1",
        "A generated split artifact version mezoje nem vart.");
    masodik::tests::require(
        health.validated_context_tokens == 32768,
        "A generated split artifact validated_context_tokens mezoje nem vart.");
}
