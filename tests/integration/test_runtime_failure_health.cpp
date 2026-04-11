#include "test_registry.hpp"

#include "masodik/config/default_paths.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"

#include "streaming_test_support.hpp"

MASODIK_TEST(runtime_generation_failure_marks_health_unready) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    masodik::tests::ScopedEnvVar force_failure{"MASODIK_TEST_GENERATE_FAILURE_MODE", "runtime_error"};

    auto service_config = masodik::runtime::build_service_config(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        masodik::config::default_paths::default_model_root,
        masodik::tests::find_repo_root());

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::api::ChatCompletionRequest request;
    request.model = service_config.active_model;
    request.messages.push_back({.role = "user", .content = "hello"});

    bool threw = false;
    try {
        (void)pipeline_service.generate_chat_completion(request);
    } catch (const std::exception&) {
        threw = true;
    }

    masodik::tests::require(threw, "A kenyszeritett runtime failure-nek kivetelt kellett dobnia.");
    masodik::tests::require(!pipeline_service.ready(), "Runtime hiba utan a pipeline nem maradhat ready.");

    const auto health = pipeline_service.make_health_response();
    masodik::tests::require(!health.ready, "Runtime hiba utan a health ready mezonek false-nak kell lennie.");
    masodik::tests::require(health.state == "failed", "Runtime hiba utan a health allapotnak failed-nek kell lennie.");
    masodik::tests::require(
        pipeline_service.status_message().find("runtime") != std::string::npos ||
            pipeline_service.status_message().find("GPU") != std::string::npos,
        "A runtime failure status_message nem eleg beszedes.");
}
