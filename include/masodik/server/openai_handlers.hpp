#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "masodik/diagnostics/logger.hpp"
#include "masodik/diagnostics/queue_metrics.hpp"
#include "masodik/genai/embedding_pipeline_service.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/degraded_state_guard.hpp"
#include "masodik/runtime/request_scheduler.hpp"
#include "masodik/runtime/request_timeout_policy.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/server/http_server.hpp"

namespace masodik::server {

struct OpenAIRouteSupport {
    std::shared_ptr<masodik::runtime::RequestScheduler> request_scheduler;
    std::shared_ptr<masodik::diagnostics::QueueMetrics> queue_metrics;
    std::shared_ptr<masodik::runtime::DegradedStateGuard> degraded_state_guard;
    std::shared_ptr<masodik::runtime::RequestTimeoutPolicy> request_timeout_policy;
    std::shared_ptr<masodik::diagnostics::Logger> request_audit_logger;
    std::shared_ptr<std::atomic<std::uint64_t>> request_audit_sequence;
    std::string request_audit_prefix = "chatreq-local";
};

HttpResponse handle_health_request(
    const HttpRequest& request,
    const masodik::runtime::ServiceConfig& service_config,
    masodik::genai::LlmPipelineService& pipeline_service,
    const OpenAIRouteSupport& route_support,
    masodik::genai::EmbeddingPipelineService* embedding_service = nullptr);

HttpResponse handle_models_request(
    const HttpRequest& request,
    const masodik::runtime::ServiceConfig& service_config,
    masodik::genai::LlmPipelineService& pipeline_service,
    masodik::genai::EmbeddingPipelineService* embedding_service = nullptr);

HttpResponse handle_chat_completion_request(
    const HttpRequest& request,
    const masodik::runtime::ServiceConfig& service_config,
    masodik::genai::LlmPipelineService& pipeline_service,
    std::shared_ptr<OpenAIRouteSupport> route_support);

HttpResponse handle_embeddings_request(
    const HttpRequest& request,
    const masodik::runtime::ServiceConfig& service_config,
    masodik::genai::EmbeddingPipelineService& embedding_service);

void register_openai_routes(
    HttpServer& server,
    const masodik::runtime::ServiceConfig& service_config,
    masodik::genai::LlmPipelineService& pipeline_service,
    std::shared_ptr<OpenAIRouteSupport> route_support = {},
    masodik::genai::EmbeddingPipelineService* embedding_service = nullptr);

}  // namespace masodik::server
