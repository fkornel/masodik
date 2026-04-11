#include "test_registry.hpp"

#include "masodik/config/default_paths.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"

MASODIK_TEST(missing_model_package_surfaces_failed_runtime_state) {
    auto service_config = masodik::runtime::build_service_config(
        masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        masodik::config::default_paths::default_model_root,
        masodik::tests::find_repo_root());
    service_config.model_repository.root_path = masodik::tests::find_repo_root() / "build" / "missing-model-package";
    service_config.model_repository.model_xml_path.clear();
    service_config.model_repository.model_bin_path.clear();
    service_config.model_repository.graph_path.clear();
    service_config.runtime_dependencies.model_repository_exists = false;

    masodik::genai::LlmPipelineService pipeline_service{service_config};
    pipeline_service.initialize();

    masodik::tests::require(!pipeline_service.ready(), "Hianyzo modellnel a pipeline nem lehet ready.");
    masodik::tests::require(pipeline_service.state() == "failed", "Hianyzo modellnel failed allapot vart.");
    masodik::tests::require(
        pipeline_service.status_message().find("modell") != std::string::npos ||
            pipeline_service.status_message().find("root path") != std::string::npos,
        "A hianyzo modell hibauzenetenek egyertelmunek kell lennie.");
}
