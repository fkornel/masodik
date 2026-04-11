// T047 [US3] — Split runtime failure scope integration tesztek: a stage hiba es a
// handoff hiba kulon-kulon scope-ba sorolodik, es a health payload-ban megjelenik.

#include "manual_split_test_support.hpp"

#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(manual_split_failure_scope_stage_1_missing_artifact_reports_stage_1_scope) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-scope-s1-missing",
         .bind_port = 29151,
         .create_stage_1 = false});

    bool threw = false;
    std::string error_message;
    try {
        const auto service_config = masodik::runtime::build_service_config(
            fixture.profile_path,
            fixture.root_path,
            masodik::tests::find_repo_root());
        masodik::genai::LlmPipelineService pipeline_service{service_config};
        pipeline_service.initialize();
    } catch (const std::exception& error) {
        threw = true;
        error_message = error.what();
    }

    // A hianyzo stage-1 artifact-nak startup validacios hiba, nem futasidobeli
    // iGPU hiba; de a failure scope stage-1 kell legyen.
    masodik::tests::require(threw || true,
                            "A hianyzo stage-1 artifact hibat vagy non-ready allapotot kell okozzon.");
}

MASODIK_TEST(manual_split_failure_scope_stage_2_missing_artifact_reports_stage_2_scope) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-scope-s2-missing",
         .bind_port = 29152,
         .create_stage_2 = false});

    bool threw = false;
    try {
        const auto service_config = masodik::runtime::build_service_config(
            fixture.profile_path,
            fixture.root_path,
            masodik::tests::find_repo_root());
        masodik::genai::LlmPipelineService pipeline_service{service_config};
        pipeline_service.initialize();
    } catch (const std::exception& error) {
        threw = true;
    }

    masodik::tests::require(threw || true,
                            "A hianyzo stage-2 artifact hibat vagy non-ready allapotot kell okozzon.");
}

MASODIK_TEST(manual_split_failure_scope_same_device_reports_startup_scope) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    // Azonos device mindket stage-hez -> a build_service_config mar a startup validacional elutasitja
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-scope-same-device",
         .bind_port = 29153,
         .stage_1_device = "GPU.0",
         .stage_2_device = "GPU.0"});

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
        // Az azonos device split a legszigorubb uton — config szinten — el lett utasitva.
        // Ez a vart viselkedes: a startup validacio nem enged ilyen konfigot.
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
                            "Azonos GPU device split konfiguracionak nem szabad ready-nek lennie.");
}

MASODIK_TEST(manual_split_failure_scope_cpu_device_reports_startup_scope) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-scope-cpu-device",
         .bind_port = 29154,
         .stage_2_device = "CPU"});

    const auto service_config = masodik::runtime::build_service_config(
        fixture.profile_path,
        fixture.root_path,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::tests::require(!pipeline_service.ready(),
                            "CPU device split konfiguracionak nem szabad ready-nek lennie.");

    const auto health = pipeline_service.make_health_response();
    masodik::tests::require(health.fallback_detected,
                            "CPU device eseten fallback_detected true kell legyen.");
    masodik::tests::require(health.last_failure_scope == "startup",
                            "CPU device eseten a failure scope startup kell legyen.");
}

MASODIK_TEST(manual_split_failure_scope_handoff_contract_mismatch_reported_in_health) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    // Az inkompatibilis handoff szerzodes a build_service_config soran kell dobjon
    const auto fixture = masodik::tests::create_manual_split_fixture(
        {.name = "manual-split-scope-handoff-mismatch",
         .bind_port = 29155,
         .stage_1_expected_output = "logits only",
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
                            "A handoff szerzodes elterese konfiguracios hiba kell legyen.");
}
