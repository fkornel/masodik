#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace masodik::ovms {

struct OvmsLaunchOptions {
    std::filesystem::path executable_path;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
    std::filesystem::path log_path;
    std::chrono::milliseconds graceful_shutdown_timeout{5000};
};

class OvmsProcess {
public:
    OvmsProcess() = default;
    OvmsProcess(const OvmsProcess&) = delete;
    OvmsProcess& operator=(const OvmsProcess&) = delete;
    OvmsProcess(OvmsProcess&& other) noexcept;
    OvmsProcess& operator=(OvmsProcess&& other) noexcept;
    ~OvmsProcess();

    void start(const OvmsLaunchOptions& options);
    void stop();
    [[nodiscard]] bool is_running() const;
    [[nodiscard]] std::uint32_t process_id() const noexcept;
    [[nodiscard]] std::optional<int> wait_for_exit(std::chrono::milliseconds timeout) const;
    [[nodiscard]] const std::filesystem::path& log_path() const noexcept;

private:
    void close_handles() noexcept;

    void* process_handle_ = nullptr;
    void* thread_handle_ = nullptr;
    void* log_handle_ = nullptr;
    std::uint32_t process_id_ = 0;
    std::chrono::milliseconds graceful_shutdown_timeout_{5000};
    std::filesystem::path log_path_;
};

}  // namespace masodik::ovms
