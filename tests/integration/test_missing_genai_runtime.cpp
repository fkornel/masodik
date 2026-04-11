#include "test_registry.hpp"

#include "masodik/config/default_paths.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"

MASODIK_TEST(missing_genai_runtime_surfaces_failed_runtime_state) {
    auto service_config = masodik::runtime::build_service_config(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        masodik::config::default_paths::default_model_root,
        masodik::tests::find_repo_root());
    service_config.runtime_dependencies.openvino_genai_dll_exists = false;
    service_config.runtime_dependencies.openvino_genai_c_dll_exists = false;

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    const auto health = pipeline_service.make_health_response();
    masodik::tests::require(!pipeline_service.ready(), "Hianyzo GenAI runtime mellett a pipeline nem lehet ready.");
    masodik::tests::require(health.state == "failed", "Hianyzo runtime mellett failed health allapot vart.");
    masodik::tests::require(
        pipeline_service.status_message().find("GenAI") != std::string::npos ||
            pipeline_service.status_message().find("runtime") != std::string::npos,
        "A runtime-hiany hibauzenet nem eleg beszedes.");
}
