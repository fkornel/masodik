#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace masodik::api {
struct RequestAuditRecord;
}

namespace masodik::diagnostics {

enum class LogLevel {
    debug,
    info,
    warning,
    error
};

struct SplitStageLogEntry {
    std::string stage_id;
    std::string state;
    std::string device;
    std::string request_id;
    std::string memory_state;
    std::string message;
};

struct SplitFailureSummaryEntry {
    std::string failure_scope;
    std::string stage_1_state;
    std::string stage_2_state;
    std::string handoff_state;
    bool fallback_detected = false;
    std::string last_error;
};

struct SplitProofLogEntry {
    std::string request_id;
    std::string sequence_id;
    std::string request_path_mode;
    std::string handoff_state;
    std::string stage_1_device;
    std::string stage_2_device;
    std::string stage_1_execution_devices;
    std::string stage_2_execution_devices;
    std::string split_artifact_version;
    int validated_context_tokens = 0;
    std::string memory_summary_system;
};

class Logger {
public:
    Logger() = default;

    static Logger create_console_logger();
    static Logger create_file_logger(const std::filesystem::path& file_path);

    void log(LogLevel level, std::string_view message);
    void debug(std::string_view message);
    void info(std::string_view message);
    void warning(std::string_view message);
    void error(std::string_view message);
    void request_audit(const masodik::api::RequestAuditRecord& record);
    void split_stage(const SplitStageLogEntry& entry, LogLevel level = LogLevel::info);
    void split_failure_summary(const SplitFailureSummaryEntry& entry);

    [[nodiscard]] const std::filesystem::path& sink_path() const noexcept;

private:
    explicit Logger(std::filesystem::path sink_path);

    std::filesystem::path sink_path_;
    std::shared_ptr<std::ofstream> file_stream_;
    std::shared_ptr<std::mutex> mutex_ = std::make_shared<std::mutex>();
};

[[nodiscard]] std::string format_request_audit_message(const masodik::api::RequestAuditRecord& record);
[[nodiscard]] std::string format_split_stage_message(const SplitStageLogEntry& entry);
[[nodiscard]] std::string format_split_failure_summary(const SplitFailureSummaryEntry& entry);
[[nodiscard]] std::string format_split_proof_message(const SplitProofLogEntry& entry);

}  // namespace masodik::diagnostics
