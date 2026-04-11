#pragma once

#include <chrono>
#include <string>

#include "masodik/runtime/error_category.hpp"
#include "masodik/runtime/runtime_state.hpp"

namespace masodik::runtime {

struct SplitMemorySummary {
    std::string stage_1;
    std::string stage_2;
    std::string system;
};

struct HealthSnapshot {
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    RuntimeState runtime_state = RuntimeState::discovered;
    std::string active_profile;
    std::string active_model;
    std::size_t queue_depth = 0;
    std::size_t active_streams = 0;
    std::size_t peak_active_streams = 0;
    bool ready = false;
    RuntimeError last_error_category = RuntimeError::none;
    std::string last_error_message;
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
    RuntimeFailureScope last_failure_scope = RuntimeFailureScope::none;
    bool fallback_detected = false;
    SplitMemorySummary memory_summary;
    std::string last_failure_summary;
};

}  // namespace masodik::runtime
