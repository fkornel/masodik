// T048 [US3] — Split degraded-health integration tesztek: a runtime hiba utan a
// health payload korrekten jelzi a degradalt vagy failed allapotot es a scope-ot.

#include "manual_split_test_support.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(manual_split_health_shows_ready_then_degrades_after_runtime_failure) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    masodik::tests::ScopedEnvVar force_failure{"MASODIK_TEST_GENERATE_FAILURE_MODE", "runtime_error"};

    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-health-degrade", .bind_port = 29161});

    const auto service_config = masodik::runtime::build_service_config(
        fixture.profile_path,
        fixture.root_path,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    // Elofeltetel: a pipeline indulaskor ready
    masodik::tests::require(pipeline_service.ready(),
                            "A pipeline indulaskor ready kell legyen ervenyes fixture mellett.");

    const auto pre_health = pipeline_service.make_health_response();
    masodik::tests::require(pre_health.ready, "A pre-health readynek kell lennie.");
    masodik::tests::require(pre_health.split_mode, "A pre-health split_mode true kell legyen.");
    masodik::tests::require(pre_health.stage_1_state == "ready", "Stage-1 indulaskor ready.");
    masodik::tests::require(pre_health.stage_2_state == "ready", "Stage-2 indulaskor ready.");

    // Kenyszeritett runtime hiba
    masodik::api::ChatCompletionRequest request;
    request.model = service_config.active_model;
    request.messages.push_back({.role = "user", .content = "teszt"});

    bool threw = false;
    try {
        (void)pipeline_service.generate_chat_completion(request);
    } catch (const std::exception&) {
        threw = true;
    }

    masodik::tests::require(threw, "A kenyszeritett hibanak kivetelt kell dobnia.");

    // Runtime hiba utan a health nem maradhat ready
    masodik::tests::require(!pipeline_service.ready(),
                            "Runtime hiba utan a pipeline nem maradhat ready.");

    const auto post_health = pipeline_service.make_health_response();
    masodik::tests::require(!post_health.ready,
                            "A post-health nem lehet ready runtime hiba utan.");
    masodik::tests::require(post_health.state == "failed",
                            "A post-health allapotnak failed-nek kell lennie.");
    masodik::tests::require(post_health.split_mode,
                            "A split_mode runtime hiba utan is true marad.");
    masodik::tests::require(
        post_health.status_message.find("runtime") != std::string::npos ||
            post_health.status_message.find("Runtime") != std::string::npos ||
            post_health.status_message.find("unhealthy") != std::string::npos,
        "A status_message-nek tartalmaznia kell a runtime hiba jelzest.");
}

MASODIK_TEST(manual_split_health_same_device_prevents_ready_state) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};

    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-health-same-device-block",
         .bind_port = 29162,
         .stage_1_device = "GPU.1",
         .stage_2_device = "GPU.1"});

    bool config_threw = false;
    try {
        (void)masodik::runtime::build_service_config(
            fixture.profile_path,
            fixture.root_path,
            masodik::tests::find_repo_root());
    } catch (const std::exception&) {
        config_threw = true;
    }

    if (config_threw) {
        // Az azonos device split a config szinten el lett utasitva — ez a vart viselkedes.
        return;
    }

    // Ha a config megepul (nem szabadna), a pipeline-nek nem szabad ready-nek lennie
    const auto service_config = masodik::runtime::build_service_config(
        fixture.profile_path,
        fixture.root_path,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::tests::require(!pipeline_service.ready(),
                            "Azonos device split startup soran a pipeline nem lehet ready.");

    const auto health = pipeline_service.make_health_response();
    masodik::tests::require(!health.ready, "Health ready false kell legyen.");
    masodik::tests::require(health.state == "failed",
                            "Azonos device split eseten a state failed kell legyen.");
}

MASODIK_TEST(manual_split_health_incompatible_contract_never_reaches_ready) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};

    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-health-bad-contract",
         .bind_port = 29163,
         .stage_1_expected_output = "logits",
         .stage_2_expected_input = "hidden states + kv delta"});

    bool threw = false;
    try {
        (void)masodik::runtime::build_service_config(
            fixture.profile_path,
            fixture.root_path,
            masodik::tests::find_repo_root());
    } catch (const std::exception&) {
        threw = true;
    }

    masodik::tests::require(threw,
                            "Inkompatibilis handoff szerzodes eseten a service_config felepitese kell dobjon.");
}
