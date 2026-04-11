#include <stdexcept>
#include <string>

#include "manual_split_test_support.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/split/split_artifact_builder.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(manual_split_128k_budget_surfaces_validated_context_tokens) {
    const auto source_fixture = masodik::tests::create_manual_split_source_fixture("manual-split-128k-budget-ok");
    const auto output_root = source_fixture.root_path / "generated-artifact";
    const auto artifact = masodik::split::build_split_artifact({
        .source_model_root = source_fixture.root_path,
        .output_root = output_root,
        .validated_context_tokens = 131072,
    });
    const auto profile_path =
        masodik::tests::write_manual_split_profile_for_root(artifact.artifact_root, "b580-manual-split", 29153, 131072);

    const auto service_config = masodik::runtime::build_service_config(
        profile_path,
        artifact.artifact_root,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::api::ChatCompletionRequest request;
    request.model = service_config.active_model;
    request.max_tokens = 256;
    request.messages.push_back({.role = "user", .content = std::string(12000, 'c')});

    const auto response = pipeline_service.generate_chat_completion(request);
    const auto health = pipeline_service.make_health_response();
    masodik::tests::require(!response.choices.empty(), "A 128k split valasznak legalabb egy choice-t kell adnia.");
    masodik::tests::require(health.validated_context_tokens == 131072, "A validated_context_tokens nem vart.");
    masodik::tests::require(health.request_path_mode == "compiled", "A compiled split request_path_mode nem vart.");
    masodik::tests::require(health.stage_1_device == "GPU.1", "A stage_1_device nem vart.");
    masodik::tests::require(health.stage_2_device == "GPU.0", "A stage_2_device nem vart.");
    masodik::tests::require(health.split_artifact_version == "split-v1", "A split_artifact_version nem vart.");
}

MASODIK_TEST(manual_split_128k_budget_rejects_request_above_validated_context_cap) {
    const auto source_fixture = masodik::tests::create_manual_split_source_fixture("manual-split-128k-budget-cap");
    const auto output_root = source_fixture.root_path / "generated-artifact";
    const auto artifact = masodik::split::build_split_artifact({
        .source_model_root = source_fixture.root_path,
        .output_root = output_root,
        .validated_context_tokens = 131072,
    });
    const auto profile_path =
        masodik::tests::write_manual_split_profile_for_root(artifact.artifact_root, "b580-manual-split", 29154, 131072);

    const auto service_config = masodik::runtime::build_service_config(
        profile_path,
        artifact.artifact_root,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::api::ChatCompletionRequest request;
    request.model = service_config.active_model;
    request.max_tokens = 2048;
    request.messages.push_back({.role = "user", .content = std::string(450000, 'd')});

    bool threw = false;
    try {
        (void)pipeline_service.generate_chat_completion(request);
    } catch (const std::runtime_error& error) {
        threw = std::string{error.what()}.find("context budget") != std::string::npos;
    }

    masodik::tests::require(threw, "A validalt 128k context cap fole meno keresnek loud failure-rel kell megallnia.");
}
