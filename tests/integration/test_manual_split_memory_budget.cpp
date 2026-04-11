#include <stdexcept>
#include <string>

#include "manual_split_test_support.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(manual_split_memory_budget_rejection_does_not_poison_pipeline_readiness) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-memory-budget-fail",
         .bind_port = 29152,
         .manifest_max_context_tokens = 8192,
         .profile_desired_context_tokens = 8192,
         .stage_1_memory_budget_gb = 0.4,
         .stage_2_memory_budget_gb = 0.4});

    const auto service_config = masodik::runtime::build_service_config(
        fixture.profile_path,
        fixture.root_path,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::api::ChatCompletionRequest request;
    request.model = service_config.active_model;
    request.max_tokens = 512;
    request.messages.push_back({.role = "user", .content = std::string(12000, 'b')});

    bool threw = false;
    try {
        (void)pipeline_service.generate_chat_completion(request);
    } catch (const std::runtime_error& error) {
        threw = std::string{error.what()}.find("memory budget") != std::string::npos;
    }

    masodik::tests::require(threw, "A split memoria-budget tullepest a kereskiszolgalasnak el kell utasitania.");
    masodik::tests::require(pipeline_service.ready(), "A memoria-budget elutasitas nem teheti unhealthy-va a pipeline-t.");
    const auto health = pipeline_service.make_health_response();
    masodik::tests::require(health.stage_1_state == "ready", "A memoria-budget elutasitas utan a stage-1 startup allapotnak ready-nek kell maradnia.");
    masodik::tests::require(health.handoff_state == "failed", "A memoria-budget elutasitas utan a handoff allapotnak failed-nek kell lennie.");
}
