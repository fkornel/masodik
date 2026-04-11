#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "masodik/api/openai_contract.hpp"
#include "masodik/genai/device_profile_evaluator.hpp"
#include "masodik/runtime/readiness_reporter.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/runtime/service_dependency_validator.hpp"
#include "masodik/runtime/startup_state_machine.hpp"
#include "masodik/split/split_infer_chain.hpp"

namespace masodik::genai {

struct ChatGenerationResult {
    std::string content;
    std::vector<masodik::api::ToolCall> tool_calls;
    std::string finish_reason = "stop";
};

struct StreamingChunk {
    std::string delta_content;
    std::vector<masodik::api::ToolCallChunkDelta> tool_call_deltas;
    std::string finish_reason;
    bool terminal = false;
};

using StreamingChunkCallback = std::function<bool(const StreamingChunk&)>;
using StreamingCancellationCheck = std::function<bool()>;

[[nodiscard]] masodik::api::RequestDecisionPath classify_request_decision_path(
    const masodik::api::ChatCompletionRequest& request);

enum class SplitExecutionStageId {
    stage_1,
    stage_2
};

enum class SplitStageState {
    uninitialized,
    loading,
    ready,
    degraded,
    failed
};

enum class SplitHandoffState {
    idle,
    created,
    transferred,
    consumed,
    failed,
    backpressured,
    cancelled
};

struct SplitExecutionStageSnapshot {
    SplitExecutionStageId stage_id = SplitExecutionStageId::stage_1;
    std::string device_id;
    std::filesystem::path artifact_path;
    SplitStageState load_state = SplitStageState::uninitialized;
    SplitStageState warm_state = SplitStageState::uninitialized;
    bool placeholder_mode = false;
    double memory_budget_gb = 0.0;
    std::string last_error_message;
};

struct SplitHandoffSnapshot {
    std::string request_id;
    std::string sequence_id;
    SplitHandoffState state = SplitHandoffState::idle;
    std::string producer_stage_id = "stage-1";
    std::string consumer_stage_id = "stage-2";
};

[[nodiscard]] inline std::string_view to_string(const SplitStageState state) noexcept {
    switch (state) {
    case SplitStageState::uninitialized:
        return "uninitialized";
    case SplitStageState::loading:
        return "starting";
    case SplitStageState::ready:
        return "ready";
    case SplitStageState::degraded:
        return "degraded";
    case SplitStageState::failed:
        return "failed";
    }
    return "unknown";
}

[[nodiscard]] inline std::string_view to_string(const SplitHandoffState state) noexcept {
    switch (state) {
    case SplitHandoffState::idle:
        return "idle";
    case SplitHandoffState::created:
        return "created";
    case SplitHandoffState::transferred:
        return "ready";
    case SplitHandoffState::consumed:
        return "consumed";
    case SplitHandoffState::failed:
        return "failed";
    case SplitHandoffState::backpressured:
        return "backpressured";
    case SplitHandoffState::cancelled:
        return "cancelled";
    }
    return "unknown";
}

class LlmPipelineService {
public:
    explicit LlmPipelineService(masodik::runtime::ServiceConfig service_config);
    ~LlmPipelineService();

    void initialize();

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] const std::string& state() const noexcept;
    [[nodiscard]] const std::string& status_message() const noexcept;
    [[nodiscard]] const masodik::runtime::ServiceConfig& service_config() const noexcept;
    [[nodiscard]] masodik::runtime::RuntimeError last_error() const noexcept;

    [[nodiscard]] masodik::api::HealthResponse make_health_response() const;
    [[nodiscard]] std::vector<masodik::api::ModelCard> list_models() const;
    [[nodiscard]] ChatGenerationResult generate_chat_result(const masodik::api::ChatCompletionRequest& request) const;
    [[nodiscard]] masodik::api::ChatCompletionResponse generate_chat_completion(
        const masodik::api::ChatCompletionRequest& request) const;
    void stream_chat_completion(
        const masodik::api::ChatCompletionRequest& request,
        StreamingChunkCallback on_chunk,
        StreamingCancellationCheck should_cancel = {}) const;

private:
    /// Belso generalasi implementacio opcionalis tokenenkenti streaming callback-kel.
    /// Ha on_chunk nem nullptr, a content chunkok a generacio soran emittalodnak (valodi streaming).
    /// Tool call eredmenyek mindig a visszateresi ertekben jonnek (nem streamelve).
    [[nodiscard]] ChatGenerationResult generate_chat_result_impl(
        const masodik::api::ChatCompletionRequest& request,
        StreamingChunkCallback on_chunk,
        StreamingCancellationCheck should_cancel) const;

    struct SplitStageLoadPlan {
        SplitExecutionStageId stage_id = SplitExecutionStageId::stage_1;
        std::string device_id;
        std::filesystem::path artifact_path;
        double memory_budget_gb = 0.0;
    };

    struct SplitStageLoadResult {
        SplitStageState load_state = SplitStageState::uninitialized;
        SplitStageState warm_state = SplitStageState::uninitialized;
        std::string status_message;
        bool placeholder_mode = false;
    };

    struct SplitStartupPlan {
        SplitStageLoadPlan stage_1;
        SplitStageLoadPlan stage_2;
    };

    struct Impl;
    [[nodiscard]] SplitStartupPlan build_split_startup_plan() const;
    [[nodiscard]] SplitStageLoadResult initialize_split_stage(const SplitStageLoadPlan& load_plan);
    void note_runtime_failure(const std::exception& error) const;
    [[nodiscard]] masodik::runtime::RuntimeState effective_runtime_state() const noexcept;
    [[nodiscard]] bool effective_ready() const noexcept;
    [[nodiscard]] const std::string& effective_status_message() const noexcept;

    masodik::runtime::ServiceConfig service_config_;
    std::unique_ptr<Impl> impl_;
    mutable masodik::genai::DeviceProfileEvaluation device_profile_evaluation_;
    masodik::runtime::StartupStateMachine startup_state_machine_;
    masodik::runtime::ServiceDependencyValidator dependency_validator_;
    masodik::runtime::ReadinessReporter readiness_reporter_;
    bool initialized_ = false;
    mutable bool ready_ = false;
    mutable std::string state_ = "discovered";
    mutable std::string status_message_ = "A pipeline meg nem inicializalt.";
    mutable bool runtime_failure_detected_ = false;
    mutable masodik::runtime::RuntimeState runtime_failure_state_ = masodik::runtime::RuntimeState::discovered;
    mutable masodik::runtime::RuntimeError runtime_failure_error_ = masodik::runtime::RuntimeError::none;
    mutable std::string runtime_failure_message_;
    mutable SplitExecutionStageSnapshot split_stage_1_;
    mutable SplitExecutionStageSnapshot split_stage_2_{.stage_id = SplitExecutionStageId::stage_2};
    mutable SplitHandoffSnapshot split_handoff_;
    mutable masodik::runtime::RuntimeFailureScope split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::none;
    mutable bool split_fallback_detected_ = false;
    mutable masodik::runtime::SplitMemorySummary split_memory_summary_;
    mutable std::string split_stage_1_execution_devices_summary_;
    mutable std::string split_stage_2_execution_devices_summary_;
    mutable bool split_compiled_request_path_active_ = false;
    mutable std::atomic<std::uint64_t> split_runtime_request_counter_{1};
};

}  // namespace masodik::genai
