#include "manual_split_test_support.hpp"

#include "masodik/config/json_loader.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/split/split_artifact_builder.hpp"

#include "test_registry.hpp"

MASODIK_TEST(manual_split_hardware_proof_health_surfaces_compiled_proof_metadata) {
    const auto source_fixture = masodik::tests::create_manual_split_source_fixture("manual-split-hardware-proof");
    const auto output_root = source_fixture.root_path / "generated-artifact";
    const auto artifact = masodik::split::build_split_artifact({
        .source_model_root = source_fixture.root_path,
        .output_root = output_root,
        .validated_context_tokens = 131072,
    });
    const auto profile_path =
        masodik::tests::write_manual_split_profile_for_root(artifact.artifact_root, "b580-manual-split", 29188, 131072);

    const auto service_config = masodik::runtime::build_service_config(
        profile_path,
        artifact.artifact_root,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::api::ChatCompletionRequest request;
    request.model = service_config.active_model;
    request.max_tokens = 32;
    request.messages.push_back({.role = "user", .content = "Adj egy rovid compiled split proof valaszt."});

    const auto response = pipeline_service.generate_chat_completion(request);
    const auto health = pipeline_service.make_health_response();
    const auto health_json = masodik::config::JsonLoader::parse(masodik::api::to_json(health)).as_object();

    masodik::tests::require(response.choices.front().message.content.find("Placeholder valasz") == std::string::npos,
                            "A hardware-proof health teszt nem maradhat placeholder valaszutban.");
    masodik::tests::require(
        masodik::config::get_string(health_json, "request_path_mode") == "compiled",
        "A hardware-proof health payloadnak compiled request_path_mode-ot kell mutatnia.");
    masodik::tests::require(
        !masodik::config::get_string(health_json, "stage_1_execution_devices").empty(),
        "A hardware-proof health payloadbol hianyzik a stage_1_execution_devices mezo.");
    masodik::tests::require(
        !masodik::config::get_string(health_json, "stage_2_execution_devices").empty(),
        "A hardware-proof health payloadbol hianyzik a stage_2_execution_devices mezo.");
    masodik::tests::require(
        !masodik::config::get_string(health_json, "last_split_request_id").empty(),
        "A hardware-proof health payloadbol hianyzik a last_split_request_id mezo.");
    masodik::tests::require(
        !masodik::config::get_string(health_json, "last_split_sequence_id").empty(),
        "A hardware-proof health payloadbol hianyzik a last_split_sequence_id mezo.");
}

