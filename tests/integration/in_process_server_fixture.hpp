#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <filesystem>

#include "manual_split_test_support.hpp"
#include "test_registry.hpp"

#include "masodik/config/default_paths.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/health_monitor.hpp"
#include "masodik/runtime/network_binding.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/server/http_server.hpp"
#include "masodik/server/openai_handlers.hpp"

namespace masodik::tests {

class InProcessServerFixture {
public:
    explicit InProcessServerFixture(
        const std::filesystem::path& profile_path = masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        const unsigned short port = 29124,
        const std::filesystem::path& model_root = masodik::config::default_paths::default_model_root) {
        service_config_ = masodik::runtime::build_service_config(
            profile_path,
            model_root,
            masodik::tests::find_repo_root());
        masodik::runtime::apply_network_binding(service_config_, service_config_.bind_host, port);

        pipeline_service_ = std::make_unique<masodik::genai::LlmPipelineService>(service_config_);
        pipeline_service_->initialize();
        masodik::tests::require(pipeline_service_->ready(), pipeline_service_->status_message());

        masodik::server::register_openai_routes(server_, service_config_, *pipeline_service_);
        server_.start({.bind_host = service_config_.bind_host, .bind_port = service_config_.bind_port});

        masodik::runtime::HealthMonitor health_monitor;
        const auto snapshot = health_monitor.wait_until_ready(
            service_config_.health_url,
            std::chrono::minutes{10},
            service_config_.active_profile,
            service_config_.active_model);
        masodik::tests::require(snapshot.ready, "A valodi GenAI szerver nem valt ready allapotba.");
    }

    ~InProcessServerFixture() { server_.stop(); }

    [[nodiscard]] const masodik::runtime::ServiceConfig& service_config() const noexcept { return service_config_; }

private:
    masodik::runtime::ServiceConfig service_config_;
    std::unique_ptr<masodik::genai::LlmPipelineService> pipeline_service_;
    masodik::server::HttpServer server_;
};

// Megosztott in-process szerver statikus split fixture-rel (placeholder mod)
inline InProcessServerFixture& shared_in_process_server() {
    static const auto fixture_paths = masodik::tests::create_manual_split_fixture(
        {.name = "shared-server", .bind_port = 29124});
    _putenv_s("MASODIK_DISABLE_OPENVINO_GENAI", "1");
    static InProcessServerFixture fixture{fixture_paths.profile_path, 29124, fixture_paths.root_path};
    return fixture;
}

}  // namespace masodik::tests
