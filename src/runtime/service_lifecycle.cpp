#include "masodik/runtime/service_lifecycle.hpp"

#include <stdexcept>

#include "masodik/runtime/network_binding.hpp"
#include "masodik/server/openai_handlers.hpp"

namespace masodik::runtime {

ServiceLifecycle::ServiceLifecycle(ServiceConfig service_config) : service_config_(std::move(service_config)) {}

void ServiceLifecycle::start() {
    if (running_) {
        throw std::logic_error("A service lifecycle mar fut.");
    }

    shutdown_controller_.reset();
    apply_network_binding(service_config_, service_config_.bind_host, service_config_.bind_port);

    pipeline_service_ = std::make_unique<masodik::genai::LlmPipelineService>(service_config_);
    pipeline_service_->initialize();
    startup_summary_ = validate_startup(service_config_, *pipeline_service_);
    if (!startup_summary_.valid) {
        throw std::runtime_error(startup_summary_.message);
    }

    // Embedding pipeline inicializalasa, ha a profil engedelyezi.
    // Az embedding hiba nem akadalyozza a chat serving inditasat (FR-007).
    if (service_config_.serving_profile.embedding_enabled) {
        embedding_service_ = std::make_unique<masodik::genai::EmbeddingPipelineService>();
        embedding_service_->initialize(
            service_config_.serving_profile.embedding_model_path,
            service_config_.serving_profile.embedding_device,
            service_config_.serving_profile.embedding_model_name,
            service_config_.serving_profile.embedding_reject_cpu_fallback);
    }

    server_.clear_routes();
    masodik::server::register_openai_routes(
        server_, service_config_, *pipeline_service_, {},
        embedding_service_.get());
    server_.start({.bind_host = service_config_.bind_host, .bind_port = service_config_.bind_port});
    running_ = true;
}

void ServiceLifecycle::stop() noexcept {
    shutdown_controller_.request_stop();
    server_.stop();
    embedding_service_.reset();
    pipeline_service_.reset();
    running_ = false;
}

void ServiceLifecycle::restart() {
    stop();
    start();
}

bool ServiceLifecycle::running() const noexcept { return running_; }

const ServiceConfig& ServiceLifecycle::service_config() const noexcept { return service_config_; }

const masodik::genai::LlmPipelineService& ServiceLifecycle::pipeline_service() const {
    if (!pipeline_service_) {
        throw std::logic_error("A pipeline service meg nincs inicializalva.");
    }
    return *pipeline_service_;
}

masodik::genai::EmbeddingPipelineService* ServiceLifecycle::embedding_service() noexcept {
    return embedding_service_.get();
}

const StartupValidationSummary& ServiceLifecycle::startup_summary() const noexcept { return startup_summary_; }

ShutdownController& ServiceLifecycle::shutdown_controller() noexcept { return shutdown_controller_; }

}  // namespace masodik::runtime
