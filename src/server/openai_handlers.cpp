#include "masodik/server/openai_handlers.hpp"

#include <sstream>
#include <stdexcept>

#include "masodik/api/openai_contract.hpp"
#include "masodik/server/openai_error_mapper.hpp"

namespace masodik::server {
namespace {

std::shared_ptr<masodik::diagnostics::Logger> make_request_audit_logger(
    const masodik::runtime::ServiceConfig& service_config) {
    try {
        return std::make_shared<masodik::diagnostics::Logger>(
            masodik::diagnostics::Logger::create_file_logger(service_config.serving_profile.runtime_log_path));
    } catch (const std::exception&) {
        return std::make_shared<masodik::diagnostics::Logger>(masodik::diagnostics::Logger::create_console_logger());
    }
}

void ensure_request_audit_support(
    const masodik::runtime::ServiceConfig& service_config,
    const std::shared_ptr<OpenAIRouteSupport>& route_support) {
    if (!route_support->request_audit_logger) {
        route_support->request_audit_logger = make_request_audit_logger(service_config);
    }
    if (!route_support->request_audit_sequence) {
        route_support->request_audit_sequence = std::make_shared<std::atomic<std::uint64_t>>(1U);
    }
    if (route_support->request_audit_prefix.empty()) {
        route_support->request_audit_prefix = "chatreq-local";
    }
}

std::string next_request_audit_id(const OpenAIRouteSupport& route_support) {
    const auto ordinal = route_support.request_audit_sequence->fetch_add(1U);
    std::ostringstream stream;
    stream << route_support.request_audit_prefix << '-' << ordinal;
    return stream.str();
}

void emit_runtime_failure_log(
    const std::shared_ptr<OpenAIRouteSupport>& route_support,
    const std::string_view phase,
    const std::string_view request_id,
    const ErrorResponseMapping& mapping,
    const masodik::api::RequestAuditRecord& audit_record,
    const std::exception& error) {
    if (!route_support || !route_support->request_audit_logger) {
        return;
    }

    route_support->request_audit_logger->error(
        std::string{phase} + " request_id=" + std::string{request_id} + " status_code=" +
        std::to_string(mapping.status_code) + " code=" + mapping.code + " error=\"" + error.what() +
        "\" audit=" + masodik::api::to_json(audit_record));
}

void attach_request_id_header(HttpResponse& response, const std::string_view request_id) {
    response.headers.push_back({"X-Request-Id", std::string{request_id}});
}

void emit_request_audit(
    const std::shared_ptr<OpenAIRouteSupport>& route_support,
    const masodik::api::RequestAuditRecord& record) {
    if (route_support->request_audit_logger) {
        route_support->request_audit_logger->request_audit(record);
    }
}

masodik::api::RequestDecisionPath resolve_request_audit_decision_path(
    const masodik::api::ChatCompletionRequest& request) {
    const auto runtime_decision_path = masodik::genai::classify_request_decision_path(request);
    if (!request.stream && request.tool_choice.kind == masodik::api::ToolChoiceKind::auto_select) {
        return masodik::api::RequestDecisionPath::content_only;
    }
    return runtime_decision_path;
}

void note_stream_failure_once(
    const std::shared_ptr<OpenAIRouteSupport>& route_support,
    const int socket_error,
    bool& failure_recorded) {
    if (failure_recorded) {
        return;
    }
    route_support->queue_metrics->note_stream_failure();
    if (socket_error != 0) {
        route_support->queue_metrics->set_last_disconnect_error(socket_error);
    }
    failure_recorded = true;
}

std::shared_ptr<OpenAIRouteSupport> make_default_route_support(const masodik::runtime::ServiceConfig& service_config) {
    auto route_support = std::make_shared<OpenAIRouteSupport>();
    route_support->queue_metrics = std::make_shared<masodik::diagnostics::QueueMetrics>();

    // A jelenlegi GenAI pipeline egy megosztott infer requestet hasznal, ezert a route reteg
    // tudatosan sorositja a generate hivasokat akkor is, ha a kulso queue policy nagyobb
    // parhuzamossagot engedne.
    route_support->request_scheduler = std::make_shared<masodik::runtime::RequestScheduler>(
        service_config.serving_profile.queue_policy,
        *route_support->queue_metrics,
        1U);
    route_support->degraded_state_guard = std::make_shared<masodik::runtime::DegradedStateGuard>(
        service_config.serving_profile.queue_policy);
    route_support->request_timeout_policy = std::make_shared<masodik::runtime::RequestTimeoutPolicy>();
    ensure_request_audit_support(service_config, route_support);
    return route_support;
}

}  // namespace

HttpResponse handle_health_request(
    const HttpRequest&,
    const masodik::runtime::ServiceConfig& service_config,
    masodik::genai::LlmPipelineService& pipeline_service,
    const OpenAIRouteSupport& route_support,
    masodik::genai::EmbeddingPipelineService* embedding_service) {
    auto health_response = pipeline_service.make_health_response();
    const auto queue_snapshot = route_support.queue_metrics->snapshot();
    health_response.queue_depth = queue_snapshot.queued_requests;
    health_response.state = route_support.degraded_state_guard->effective_state(
        health_response.state,
        service_config,
        queue_snapshot,
        health_response.gpu_offload_active);
    // Embedding pipeline allapot hozzaadasa a health valaszhoz
    if (embedding_service != nullptr) {
        const auto info = embedding_service->model_info();
        health_response.embedding_state = std::string{embedding_service->state_string()};
        health_response.embedding_model = info.model_name;
        health_response.embedding_device = info.device;
    }
    return {.status_code = 200, .body = masodik::api::to_json(health_response)};
}

HttpResponse handle_models_request(
    const HttpRequest&,
    const masodik::runtime::ServiceConfig&,
    masodik::genai::LlmPipelineService& pipeline_service,
    masodik::genai::EmbeddingPipelineService* embedding_service) {
    auto models = pipeline_service.list_models();
    // Embedding modell hozzaadasa a modelllistahoz, ha az embedding pipeline elerheto
    if (embedding_service != nullptr && embedding_service->ready()) {
        const auto info = embedding_service->model_info();
        models.push_back({.id = info.model_name, .object = "model", .owned_by = "local", .type = "embedding"});
    }
    return {.status_code = 200, .body = masodik::api::to_json(models)};
}

HttpResponse handle_chat_completion_request(
    const HttpRequest& request,
    const masodik::runtime::ServiceConfig& service_config,
    masodik::genai::LlmPipelineService& pipeline_service,
    std::shared_ptr<OpenAIRouteSupport> route_support) {
    if (!route_support) {
        route_support = make_default_route_support(service_config);
    } else {
        ensure_request_audit_support(service_config, route_support);
    }
    const auto request_id = next_request_audit_id(*route_support);

    const auto tool_call_requested_by_payload = request.body.find("\"tools\"") != std::string::npos ||
                                                request.body.find("\"tool_choice\"") != std::string::npos;

    masodik::api::ChatCompletionRequest completion_request;
    try {
        completion_request = masodik::api::parse_chat_completion_request(request.body);
        const auto decision_path = resolve_request_audit_decision_path(completion_request);
        if (completion_request.model != service_config.active_model) {
            throw std::invalid_argument("A kert model nem egyezik a helyi aktiv modellel.");
        }
        if (!pipeline_service.ready()) {
            throw std::logic_error(pipeline_service.status_message());
        }
        const auto timeout_budget = route_support->request_timeout_policy->timeout_budget(service_config, completion_request);
        auto lease = std::make_shared<masodik::runtime::RequestScheduler::Lease>(
            route_support->request_scheduler->acquire(
                timeout_budget,
                completion_request.stream ? masodik::runtime::RequestKind::streaming : masodik::runtime::RequestKind::standard));

        if (!completion_request.stream) {
            const auto completion_response = pipeline_service.generate_chat_completion(completion_request);
            for (const auto& choice : completion_response.choices) {
                if (!choice.message.tool_calls.empty()) {
                    route_support->queue_metrics->note_tool_call_response(choice.message.tool_calls.size());
                }
            }
            emit_request_audit(
                route_support,
                masodik::api::derive_request_audit_record(
                    completion_request,
                    request_id,
                    decision_path,
                    masodik::api::RequestValidationOutcome::accepted));
            HttpResponse response{.status_code = 200, .body = masodik::api::to_json(completion_response)};
            attach_request_id_header(response, request_id);
            return response;
        }

        std::ostringstream id_builder;
        id_builder << "chatcmpl-local-stream-" << lease->request_id();
        const auto response_id = id_builder.str();

        HttpResponse response;
        response.status_code = 200;
        response.content_type = "text/event-stream";
        response.chunked_transfer = true;
        response.headers.push_back({"Cache-Control", "no-cache"});
        response.headers.push_back({"X-Accel-Buffering", "no"});
        response.headers.push_back({"X-Request-Id", request_id});
        emit_request_audit(
            route_support,
            masodik::api::derive_request_audit_record(
                completion_request,
                request_id,
                decision_path,
                masodik::api::RequestValidationOutcome::accepted));
         response.stream_writer =
            [completion_request,
             &pipeline_service,
             route_support = std::move(route_support),
             lease,
             request_id,
             response_id,
             model = service_config.active_model](HttpStreamWriter& writer) {
                 lease->begin_streaming();
                 bool failure_recorded = false;
                 bool tool_call_response_recorded = false;

                const auto role_chunk = masodik::api::make_chat_completion_chunk_response(
                    response_id,
                    model,
                    0,
                    "assistant",
                    {});
                if (!writer.write_chunk(masodik::api::to_sse_json_event(masodik::api::to_json(role_chunk)))) {
                    note_stream_failure_once(route_support, writer.last_socket_error(), failure_recorded);
                    lease->cancellation_token().cancel();
                    lease->end_streaming();
                    return;
                }

                try {
                    pipeline_service.stream_chat_completion(
                        completion_request,
                        [&](const masodik::genai::StreamingChunk& chunk) {
                            if (!writer.client_connected()) {
                                note_stream_failure_once(route_support, writer.last_socket_error(), failure_recorded);
                                lease->cancellation_token().cancel();
                                return false;
                            }

                             if (chunk.terminal) {
                                if (!tool_call_response_recorded && chunk.finish_reason == "tool_calls") {
                                    route_support->queue_metrics->note_tool_call_response();
                                    tool_call_response_recorded = true;
                                }
                                const auto terminal_chunk = masodik::api::make_terminal_chat_completion_chunk_response(
                                    response_id,
                                    model,
                                    0,
                                    chunk.finish_reason.empty()
                                        ? (chunk.tool_call_deltas.empty() ? "stop" : "tool_calls")
                                        : chunk.finish_reason,
                                    chunk.tool_call_deltas);
                                if (!writer.write_chunk(
                                        masodik::api::to_sse_json_event(masodik::api::to_json(terminal_chunk)))) {
                                    note_stream_failure_once(route_support, writer.last_socket_error(), failure_recorded);
                                    lease->cancellation_token().cancel();
                                    return false;
                                }
                                if (!writer.write_chunk(masodik::api::to_sse_done_event())) {
                                    note_stream_failure_once(route_support, writer.last_socket_error(), failure_recorded);
                                    lease->cancellation_token().cancel();
                                    return false;
                                }
                                return true;
                            }

                            if (chunk.delta_content.empty() && chunk.tool_call_deltas.empty()) {
                                return true;
                            }

                            if (!chunk.tool_call_deltas.empty()) {
                                route_support->queue_metrics->note_tool_call_stream_delta(chunk.tool_call_deltas.size());
                                if (!tool_call_response_recorded) {
                                    route_support->queue_metrics->note_tool_call_response();
                                    tool_call_response_recorded = true;
                                }
                            }

                            masodik::api::ChatCompletionChunkDelta delta;
                            delta.content = chunk.delta_content;
                            delta.tool_calls = chunk.tool_call_deltas;
                            const auto delta_chunk = masodik::api::make_chat_completion_chunk_response(
                                response_id, model, 0, std::move(delta));
                            if (!writer.write_chunk(masodik::api::to_sse_json_event(masodik::api::to_json(delta_chunk)))) {
                                note_stream_failure_once(route_support, writer.last_socket_error(), failure_recorded);
                                lease->cancellation_token().cancel();
                                return false;
                            }
                            return true;
                        },
                        [&]() { return lease->cancellation_token().cancelled() || !writer.client_connected(); });
                } catch (const std::exception& error) {
                    note_stream_failure_once(route_support, writer.last_socket_error(), failure_recorded);
                    const auto audit_record = masodik::api::derive_request_audit_record(
                        completion_request,
                        request_id,
                        resolve_request_audit_decision_path(completion_request),
                        masodik::server::classify_validation_outcome(
                            error,
                            {.tool_call_related = masodik::api::has_tool_definitions(completion_request), .streaming = true}));
                    const auto mapping = classify_exception(
                        error,
                        {.tool_call_related = masodik::api::has_tool_definitions(completion_request), .streaming = true});
                    emit_runtime_failure_log(route_support, "stream_runtime_failure", request_id, mapping, audit_record, error);
                    if (writer.client_connected()) {
                        const auto ignored = writer.write_chunk(map_exception_to_terminal_sse_event(
                                                 error,
                                                 {.tool_call_related = masodik::api::has_tool_definitions(completion_request),
                                                  .streaming = true})) &&
                                             writer.write_chunk(masodik::api::to_sse_done_event());
                        (void)ignored;
                    }
                }

                lease->end_streaming();
            };
        return response;
    } catch (const std::exception& error) {
        if (tool_call_requested_by_payload && dynamic_cast<const std::invalid_argument*>(&error) != nullptr) {
            route_support->queue_metrics->note_tool_call_validation_failure();
        }
        const auto error_context =
            ErrorMappingContext{.tool_call_related =
                                    tool_call_requested_by_payload || masodik::api::has_tool_definitions(completion_request)};
        const auto mapping = classify_exception(error, error_context);
        const auto validation_outcome = mapping.validation_outcome;
        masodik::api::RequestAuditRecord audit_record;
        if (completion_request.messages.empty()) {
            auto probed_record = masodik::api::derive_request_audit_record_from_payload(
                request.body,
                {},
                masodik::api::RequestDecisionPath::unknown,
                validation_outcome);
            const auto decision_path =
                masodik::api::infer_request_decision_path(probed_record.tool_choice, probed_record.tools_present);
            audit_record = masodik::api::derive_request_audit_record_from_payload(
                request.body,
                request_id,
                decision_path,
                validation_outcome);
        } else {
            audit_record = masodik::api::derive_request_audit_record(
                completion_request,
                request_id,
                resolve_request_audit_decision_path(completion_request),
                validation_outcome);
        }
        emit_request_audit(route_support, audit_record);

        emit_runtime_failure_log(route_support, "request_runtime_failure", request_id, mapping, audit_record, error);
        auto response = map_exception_to_response(error, error_context);
        attach_request_id_header(response, request_id);
        return response;
    }
}

HttpResponse handle_embeddings_request(
    const HttpRequest& request,
    const masodik::runtime::ServiceConfig& service_config,
    masodik::genai::EmbeddingPipelineService& embedding_service) {
    try {
        const auto embedding_request = masodik::api::parse_embedding_request(request.body);

        // Modell validacio
        if (embedding_request.model != service_config.serving_profile.embedding_model_name) {
            return {.status_code = 404,
                    .body = masodik::api::to_json_error("not_found", "A kert modell nem elerheto.")};
        }

        // Pipeline keszenleti ellenorzes
        if (!embedding_service.ready()) {
            return {.status_code = 503,
                    .body = masodik::api::to_json_error("service_unavailable", "Az embedding szolgaltatas nem elerheto.")};
        }

        // Embedding generalas
        const auto batch_result = embedding_service.embed(embedding_request.input);

        // Valasz osszeallitas
        masodik::api::EmbeddingResponse response;
        response.model = embedding_request.model;
        for (std::size_t i = 0; i < batch_result.results.size(); ++i) {
            response.data.push_back({
                .object = "embedding",
                .embedding = batch_result.results[i].embedding,
                .index = static_cast<int>(i)
            });
        }
        response.usage.prompt_tokens = batch_result.total_tokens;
        response.usage.total_tokens = batch_result.total_tokens;

        return {.status_code = 200, .body = masodik::api::to_json(response)};
    } catch (const std::invalid_argument& error) {
        return {.status_code = 400,
                .body = masodik::api::to_json_error("bad_request", error.what())};
    } catch (const std::exception& error) {
        return {.status_code = 500,
                .body = masodik::api::to_json_error("internal_error", error.what())};
    }
}

void register_openai_routes(
    HttpServer& server,
    const masodik::runtime::ServiceConfig& service_config,
    masodik::genai::LlmPipelineService& pipeline_service,
    std::shared_ptr<OpenAIRouteSupport> route_support,
    masodik::genai::EmbeddingPipelineService* embedding_service) {
    if (!route_support) {
        route_support = make_default_route_support(service_config);
    } else {
        ensure_request_audit_support(service_config, route_support);
    }
    server.register_route(
        HttpMethod::get,
        service_config.health_endpoint_path,
        [route_support, &service_config, &pipeline_service, embedding_service](const HttpRequest& request) {
            return handle_health_request(request, service_config, pipeline_service, *route_support, embedding_service);
        });
    server.register_route(
        HttpMethod::get,
        service_config.models_endpoint_path,
        [&service_config, &pipeline_service, embedding_service](const HttpRequest& request) {
            return handle_models_request(request, service_config, pipeline_service, embedding_service);
        });
    server.register_route(
        HttpMethod::post,
        service_config.chat_endpoint_path,
        [route_support, &service_config, &pipeline_service](const HttpRequest& request) {
            return handle_chat_completion_request(request, service_config, pipeline_service, route_support);
        });

    // Embedding endpoint regisztracio, ha az embedding pipeline elerheto
    if (embedding_service != nullptr && service_config.serving_profile.embedding_enabled) {
        server.register_route(
            HttpMethod::post,
            service_config.serving_profile.embedding_endpoint_path,
            [&service_config, embedding_service](const HttpRequest& request) {
                return handle_embeddings_request(request, service_config, *embedding_service);
            });
    }
}

}  // namespace masodik::server
