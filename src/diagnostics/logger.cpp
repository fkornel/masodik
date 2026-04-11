#include "masodik/diagnostics/logger.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "masodik/api/openai_contract.hpp"

namespace masodik::diagnostics {
namespace {

std::string_view to_string(const LogLevel level) {
    switch (level) {
    case LogLevel::debug:
        return "DEBUG";
    case LogLevel::info:
        return "INFO";
    case LogLevel::warning:
        return "WARN";
    case LogLevel::error:
        return "ERROR";
    }
    return "UNKNOWN";
}

std::string build_line(const LogLevel level, const std::string_view message) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
    localtime_s(&local_time, &time);

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << " [" << to_string(level) << "] " << message;
    return stream.str();
}

}  // namespace

Logger::Logger(std::filesystem::path sink_path) : sink_path_(std::move(sink_path)) {}

Logger Logger::create_console_logger() { return Logger{}; }

Logger Logger::create_file_logger(const std::filesystem::path& file_path) {
    Logger logger{file_path};
    std::filesystem::create_directories(file_path.parent_path());
    logger.file_stream_ = std::make_shared<std::ofstream>(file_path, std::ios::app);
    if (!logger.file_stream_ || !(*logger.file_stream_)) {
        throw std::runtime_error("A log fajl nem nyithato meg: " + file_path.string());
    }
    return logger;
}

void Logger::log(const LogLevel level, const std::string_view message) {
    const auto line = build_line(level, message);
    std::scoped_lock lock{*mutex_};
    if (file_stream_ != nullptr && file_stream_->is_open()) {
        *file_stream_ << line << '\n';
        file_stream_->flush();
        return;
    }
    std::clog << line << '\n';
}

void Logger::debug(const std::string_view message) { log(LogLevel::debug, message); }
void Logger::info(const std::string_view message) { log(LogLevel::info, message); }
void Logger::warning(const std::string_view message) { log(LogLevel::warning, message); }
void Logger::error(const std::string_view message) { log(LogLevel::error, message); }
void Logger::request_audit(const masodik::api::RequestAuditRecord& record) { info(format_request_audit_message(record)); }
void Logger::split_stage(const SplitStageLogEntry& entry, const LogLevel level) { log(level, format_split_stage_message(entry)); }
void Logger::split_failure_summary(const SplitFailureSummaryEntry& entry) { log(LogLevel::error, format_split_failure_summary(entry)); }

const std::filesystem::path& Logger::sink_path() const noexcept { return sink_path_; }

std::string format_request_audit_message(const masodik::api::RequestAuditRecord& record) {
    return std::string{"request_audit "} + masodik::api::to_json(record);
}

std::string format_split_stage_message(const SplitStageLogEntry& entry) {
    std::ostringstream stream;
    stream << "split_stage"
           << " stage_id=" << entry.stage_id
           << " state=" << entry.state
           << " device=" << entry.device;
    if (!entry.request_id.empty()) {
        stream << " request_id=" << entry.request_id;
    }
    if (!entry.memory_state.empty()) {
        stream << " memory_state=" << entry.memory_state;
    }
    if (!entry.message.empty()) {
        stream << " message=\"" << entry.message << "\"";
    }
    return stream.str();
}

std::string format_split_failure_summary(const SplitFailureSummaryEntry& entry) {
    std::ostringstream stream;
    stream << "split_failure_summary"
           << " scope=" << entry.failure_scope
           << " stage_1=" << entry.stage_1_state
           << " stage_2=" << entry.stage_2_state
           << " handoff=" << entry.handoff_state
           << " fallback=" << (entry.fallback_detected ? "true" : "false");
    if (!entry.last_error.empty()) {
        stream << " error=\"" << entry.last_error << "\"";
    }
    return stream.str();
}

std::string format_split_proof_message(const SplitProofLogEntry& entry) {
    std::ostringstream stream;
    stream << "manual_split_proof"
           << " request_id=" << entry.request_id
           << " sequence_id=" << entry.sequence_id
           << " request_path_mode=" << entry.request_path_mode
           << " handoff_state=" << entry.handoff_state
           << " stage_1_device=" << entry.stage_1_device
           << " stage_2_device=" << entry.stage_2_device
           << " stage_1_execution_devices=\"" << entry.stage_1_execution_devices << "\""
           << " stage_2_execution_devices=\"" << entry.stage_2_execution_devices << "\""
           << " split_artifact_version=" << entry.split_artifact_version
           << " validated_context_tokens=" << entry.validated_context_tokens;
    if (!entry.memory_summary_system.empty()) {
        stream << " memory_system=\"" << entry.memory_summary_system << "\"";
    }
    return stream.str();
}

}  // namespace masodik::diagnostics
