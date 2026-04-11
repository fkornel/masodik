#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace masodik::api {

struct ToolFunctionDefinition {
    std::string name;
    std::string description;
    std::string parameters_json = "{}";
};

struct ToolDefinition {
    std::string type = "function";
    ToolFunctionDefinition function;
};

enum class ToolChoiceKind {
    none,
    auto_select,
    required,
    named_function
};

struct ToolChoice {
    ToolChoiceKind kind = ToolChoiceKind::none;
    std::string function_name;
};

enum class ToolChoiceSummaryKind {
    absent,
    none,
    auto_select,
    required,
    named_function
};

struct ToolChoiceSummary {
    ToolChoiceSummaryKind kind = ToolChoiceSummaryKind::absent;
    std::string function_name;
};

enum class RequestDecisionPath {
    unknown,
    content_only,
    tool_call_auto,
    tool_call_required,
    tool_call_named
};

enum class RequestValidationOutcome {
    not_evaluated,
    accepted,
    parser_error,
    tool_schema_error,
    model_selection_error,
    runtime_readiness_error,
    runtime_error
};

struct ToolCallFunction {
    std::string name;
    std::string arguments;
};

struct ToolCall {
    std::string id;
    std::string type = "function";
    ToolCallFunction function;
    int index = 0;
};

struct ToolCallChunkFunctionDelta {
    std::string name;
    std::string arguments;
};

struct ToolCallChunkDelta {
    int index = 0;
    std::string id;
    std::string type = "function";
    ToolCallChunkFunctionDelta function;
};

struct ChatMessage {
    std::string role;
    std::string content;
    std::vector<ToolCall> tool_calls;
};

struct ChatCompletionRequest {
    std::string model;
    std::vector<ChatMessage> messages;
    int max_tokens = 1024;
    bool stream = false;
    std::vector<ToolDefinition> tools;
    ToolChoice tool_choice;
};

struct ChatCompletionChoice {
    int index = 0;
    ChatMessage message;
    std::string finish_reason = "stop";
};

struct ChatCompletionResponse {
    std::string id = "chatcmpl-local";
    std::string model;
    std::vector<ChatCompletionChoice> choices;
};

struct ChatCompletionChunkDelta {
    std::string role;
    std::string content;
    std::vector<ToolCallChunkDelta> tool_calls;
};

struct ChatCompletionChunkChoice {
    int index = 0;
    ChatCompletionChunkDelta delta;
    std::string finish_reason;
};

struct ChatCompletionChunkResponse {
    std::string id = "chatcmpl-local-chunk";
    std::string object = "chat.completion.chunk";
    std::string model;
    std::vector<ChatCompletionChunkChoice> choices;
};

struct RequestAuditRecord {
    std::string request_id;
    bool stream = false;
    bool tools_present = false;
    std::size_t tool_count = 0;
    ToolChoiceSummary tool_choice;
    RequestDecisionPath decision_path = RequestDecisionPath::unknown;
    RequestValidationOutcome validation_outcome = RequestValidationOutcome::not_evaluated;
    std::string truncated_message_preview;
    std::string created_at;
};

struct ModelCard {
    std::string id;
    std::string object = "model";
    std::string owned_by = "local";
    bool split_mode = false;
    std::string split_profile;
    std::string type;
};

// --- Embedding API (OpenAI /v1/embeddings kompatibilis) ---

struct EmbeddingRequest {
    std::string model;
    std::vector<std::string> input;
    std::string encoding_format = "float";
};

struct EmbeddingObject {
    std::string object = "embedding";
    std::vector<float> embedding;
    int index = 0;
};

struct EmbeddingUsage {
    int prompt_tokens = 0;
    int total_tokens = 0;
};

struct EmbeddingResponse {
    std::string object = "list";
    std::vector<EmbeddingObject> data;
    std::string model;
    EmbeddingUsage usage;
};

struct SplitMemorySummary {
    std::string stage_1;
    std::string stage_2;
    std::string system;
};

struct HealthResponse {
    bool ready = false;
    std::string state;
    std::string active_model;
    std::string active_profile;
    std::string effective_device;
    std::size_t queue_depth = 0;
    std::size_t active_streams = 0;
    std::size_t peak_active_streams = 0;
    std::uint64_t total_stream_failures = 0;
    int last_disconnect_error = 0;
    bool gpu_offload_active = false;
    std::string status_message;
    bool split_mode = false;
    std::string split_profile;
    std::string stage_1_state;
    std::string stage_2_state;
    std::string handoff_state;
    std::string request_path_mode;
    std::string stage_1_device;
    std::string stage_2_device;
    std::string stage_1_execution_devices;
    std::string stage_2_execution_devices;
    std::string split_artifact_version;
    int validated_context_tokens = 0;
    std::string last_split_request_id;
    std::string last_split_sequence_id;
    std::string last_failure_scope;
    bool fallback_detected = false;
    SplitMemorySummary memory_summary;
    std::string last_failure_summary;
    // Embedding pipeline allapot
    std::string embedding_state;
    std::string embedding_model;
    std::string embedding_device;
};

ChatCompletionRequest parse_chat_completion_request(std::string_view document);
void validate_tool_definitions(const std::vector<ToolDefinition>& tools);
[[nodiscard]] bool has_tool_definitions(const ChatCompletionRequest& request) noexcept;
[[nodiscard]] bool tool_choice_requires_tool(const ToolChoice& tool_choice) noexcept;
[[nodiscard]] ToolChoiceSummary summarize_tool_choice(const ChatCompletionRequest& request) noexcept;
[[nodiscard]] std::string truncate_request_audit_preview(std::string_view content);
[[nodiscard]] RequestDecisionPath infer_request_decision_path(const ToolChoiceSummary& tool_choice, bool tools_present) noexcept;
[[nodiscard]] RequestAuditRecord derive_request_audit_record(
    const ChatCompletionRequest& request,
    std::string request_id,
    RequestDecisionPath decision_path,
    RequestValidationOutcome validation_outcome);
[[nodiscard]] RequestAuditRecord derive_request_audit_record_from_payload(
    std::string_view document,
    std::string request_id,
    RequestDecisionPath decision_path,
    RequestValidationOutcome validation_outcome);
[[nodiscard]] std::string_view to_string(ToolChoiceSummaryKind kind) noexcept;
[[nodiscard]] std::string_view to_string(RequestDecisionPath decision_path) noexcept;
[[nodiscard]] std::string_view to_string(RequestValidationOutcome validation_outcome) noexcept;
std::string to_json(const ChatCompletionResponse& response);
std::string to_json(const ChatCompletionChunkResponse& response);
std::string to_json(const RequestAuditRecord& record);
ChatCompletionChunkResponse make_chat_completion_chunk_response(
    std::string id,
    std::string model,
    int index,
    ChatCompletionChunkDelta delta,
    std::string finish_reason = {});
ChatCompletionChunkResponse make_chat_completion_chunk_response(
    std::string id,
    std::string model,
    int index,
    std::string role,
    std::string content,
    std::string finish_reason = {});
ChatCompletionChunkResponse make_terminal_chat_completion_chunk_response(
    std::string id,
    std::string model,
    int index,
    std::string finish_reason,
    std::vector<ToolCallChunkDelta> tool_calls = {});
std::string to_json(const std::vector<ModelCard>& models);
std::string to_json(const HealthResponse& response);
EmbeddingRequest parse_embedding_request(std::string_view document);
std::string to_json(const EmbeddingResponse& response);
std::string to_json_error(std::string_view code, std::string_view message);
std::string to_sse_data_event(std::string_view payload);
std::string to_sse_json_event(std::string_view json_payload);
std::string to_sse_done_event();
std::string to_sse_error_event(std::string_view code, std::string_view message);

}  // namespace masodik::api
