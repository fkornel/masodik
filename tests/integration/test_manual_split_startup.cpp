#include "manual_split_test_support.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(manual_split_startup_reports_ready_when_split_artifact_is_valid) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "manual-split-startup-ok", .bind_port = 29141});

    const auto service_config = masodik::runtime::build_service_config(
        fixture.profile_path,
        fixture.root_path,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    const auto health = pipeline_service.make_health_response();
    masodik::tests::require(pipeline_service.ready(), "Ervenyes split artifact mellett a pipeline-nek ready-nek kell lennie.");
    masodik::tests::require(pipeline_service.state() == "ready", "Ervenyes split artifact mellett ready allapot vart.");
    masodik::tests::require(health.split_mode, "A health payloadnak split_mode=true allapotot kell mutatnia.");
    masodik::tests::require(health.stage_1_state == "ready", "A split health stage_1_state mezoje nem vart.");
    masodik::tests::require(health.stage_2_state == "ready", "A split health stage_2_state mezoje nem vart.");
    masodik::tests::require(health.handoff_state == "ready", "A split health handoff_state mezoje nem vart.");
    masodik::tests::require(!health.fallback_detected, "Valid split startup mellett fallback nem eszlelheto.");
}
