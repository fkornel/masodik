#include "manual_split_test_support.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/split/split_artifact_builder.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(manual_split_compiled_request_path_uses_generated_artifact_and_surfaces_compiled_health) {
    const auto source_fixture = masodik::tests::create_manual_split_source_fixture("manual-split-compiled-request-path");
    const auto output_root = source_fixture.root_path / "generated-artifact";
    const auto artifact = masodik::split::build_split_artifact({
        .source_model_root = source_fixture.root_path,
        .output_root = output_root,
        .validated_context_tokens = 131072,
    });
    const auto profile_path =
        masodik::tests::write_manual_split_profile_for_root(artifact.artifact_root, "b580-manual-split", 29173, 131072);

    const auto service_config = masodik::runtime::build_service_config(
        profile_path,
        artifact.artifact_root,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::api::ChatCompletionRequest request;
    request.model = service_config.active_model;
    request.max_tokens = 64;
    request.messages.push_back({.role = "user", .content = "Fordits egy rovid statuszmondatot a compiled split lancrol."});

    const auto response = pipeline_service.generate_chat_completion(request);
    const auto health = pipeline_service.make_health_response();

    masodik::tests::require(!response.choices.empty(), "A compiled split valasznak legalabb egy choice-t kell adnia.");
    masodik::tests::require(
        response.choices.front().message.content.find("Placeholder valasz") == std::string::npos,
        "A compiled split request-path nem maradhat placeholder.");
    masodik::tests::require(
        response.choices.front().message.content.find("Compiled split infer sikeres") != std::string::npos,
        "A compiled split request-path valaszaban a compiled handoff osszefoglalas vart.");
    masodik::tests::require(
        health.request_path_mode == "compiled",
        "A compiled split request utan a health request_path_mode mezoben compiled vart.");
    masodik::tests::require(health.handoff_state == "consumed", "A compiled split request utan consumed handoff vart.");
    masodik::tests::require(
        health.validated_context_tokens == 131072,
        "A compiled split request-path tesztben a validated_context_tokens nem vart.");
}
