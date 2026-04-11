#pragma once

#include <memory>

#include "masodik/genai/embedding_pipeline_service.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/runtime/shutdown_controller.hpp"
#include "masodik/runtime/startup_validation.hpp"
#include "masodik/server/http_server.hpp"

namespace masodik::runtime {

class ServiceLifecycle {
public:
    explicit ServiceLifecycle(ServiceConfig service_config);

    void start();
    void stop() noexcept;
    void restart();

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] const ServiceConfig& service_config() const noexcept;
    [[nodiscard]] const masodik::genai::LlmPipelineService& pipeline_service() const;
    [[nodiscard]] masodik::genai::EmbeddingPipelineService* embedding_service() noexcept;
    [[nodiscard]] const StartupValidationSummary& startup_summary() const noexcept;
    [[nodiscard]] ShutdownController& shutdown_controller() noexcept;

private:
    ServiceConfig service_config_;
    std::unique_ptr<masodik::genai::LlmPipelineService> pipeline_service_;
    std::unique_ptr<masodik::genai::EmbeddingPipelineService> embedding_service_;
    masodik::server::HttpServer server_;
    ShutdownController shutdown_controller_;
    StartupValidationSummary startup_summary_;
    bool running_ = false;
};

}  // namespace masodik::runtime
