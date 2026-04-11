#pragma once

#include <functional>
#include <memory>

#include "test_registry.hpp"

#include "masodik/diagnostics/logger.hpp"
#include "masodik/config/default_paths.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/network_binding.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/server/openai_handlers.hpp"

namespace masodik::tests {

inline std::shared_ptr<masodik::server::OpenAIRouteSupport> make_route_support(
    const masodik::runtime::ServiceConfig& service_config,
    const std::size_t execution_slots = 1U) {
    auto route_support = std::make_shared<masodik::server::OpenAIRouteSupport>();
    route_support->queue_metrics = std::make_shared<masodik::diagnostics::QueueMetrics>();
    route_support->request_scheduler = std::make_shared<masodik::runtime::RequestScheduler>(
        service_config.serving_profile.queue_policy,
        *route_support->queue_metrics,
        execution_slots);
    route_support->degraded_state_guard = std::make_shared<masodik::runtime::DegradedStateGuard>(
        service_config.serving_profile.queue_policy);
    route_support->request_timeout_policy = std::make_shared<masodik::runtime::RequestTimeoutPolicy>();
    route_support->request_audit_logger = std::make_shared<masodik::diagnostics::Logger>(
        masodik::diagnostics::Logger::create_console_logger());
    route_support->request_audit_sequence = std::make_shared<std::atomic<std::uint64_t>>(1U);
    route_support->request_audit_prefix = "chatreq-test";
    return route_support;
}

class ConfigurableInProcessServerFixture {
public:
    ConfigurableInProcessServerFixture(
        const unsigned short port,
        std::function<void(masodik::runtime::ServiceConfig&)> configure = {},
        std::shared_ptr<masodik::server::OpenAIRouteSupport> route_support = {},
        const bool require_ready = true)
        : route_support_(std::move(route_support)) {
        service_config_ = masodik::runtime::build_service_config(
            masodik::tests::find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
            masodik::config::default_paths::default_model_root,
            masodik::tests::find_repo_root());
        if (configure) {
            configure(service_config_);
        }
        masodik::runtime::apply_network_binding(service_config_, service_config_.bind_host, port);

        pipeline_service_ = std::make_unique<masodik::genai::LlmPipelineService>(service_config_);
        pipeline_service_->initialize();
        if (require_ready) {
            masodik::tests::require(pipeline_service_->ready(), pipeline_service_->status_message());
        }

        masodik::server::register_openai_routes(server_, service_config_, *pipeline_service_, route_support_);
        server_.start({.bind_host = service_config_.bind_host, .bind_port = service_config_.bind_port});
    }

    ~ConfigurableInProcessServerFixture() { server_.stop(); }

    [[nodiscard]] const masodik::runtime::ServiceConfig& service_config() const noexcept { return service_config_; }
    [[nodiscard]] std::shared_ptr<masodik::server::OpenAIRouteSupport> route_support() const noexcept { return route_support_; }

private:
    masodik::runtime::ServiceConfig service_config_;
    std::shared_ptr<masodik::server::OpenAIRouteSupport> route_support_;
    std::unique_ptr<masodik::genai::LlmPipelineService> pipeline_service_;
    masodik::server::HttpServer server_;
};

}  // namespace masodik::tests
