#include <string>

#include "manual_split_test_support.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/split/split_artifact_builder.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(manual_split_long_context_request_succeeds_within_split_budgets) {
    const auto source_fixture = masodik::tests::create_manual_split_source_fixture("manual-split-long-context-ok");
    const auto output_root = source_fixture.root_path / "generated-artifact";
    const auto artifact = masodik::split::build_split_artifact({
        .source_model_root = source_fixture.root_path,
        .output_root = output_root,
        .max_context_tokens = 8192,
        .validated_context_tokens = 8192,
    });
    const auto profile_path =
        masodik::tests::write_manual_split_profile_for_root(artifact.artifact_root, "b580-manual-split", 29151, 8192);

    const auto service_config = masodik::runtime::build_service_config(
        profile_path,
        artifact.artifact_root,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::api::ChatCompletionRequest request;
    request.model = service_config.active_model;
    request.max_tokens = 128;
    request.messages.push_back({.role = "user", .content = std::string(9000, 'a')});

    const auto response = pipeline_service.generate_chat_completion(request);
    const auto health = pipeline_service.make_health_response();
    masodik::tests::require(!response.choices.empty(), "A long-context split valasznak legalabb egy choice-t kell adnia.");
    masodik::tests::require(
        response.choices.front().message.content.find("Placeholder valasz") == std::string::npos,
        "A compiled long-context valasz nem maradhat placeholder.");
    masodik::tests::require(health.handoff_state == "consumed", "A sikeres split long-context keres utan a handoff allapotnak consumed-nek kell lennie.");
    masodik::tests::require(health.last_failure_scope.empty(), "A sikeres split long-context keres nem hagyhat failure_scope-ot.");
    masodik::tests::require(health.request_path_mode == "compiled", "A sikeres split long-context keres compiled request_path_mode-ot var.");
}

