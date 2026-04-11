#include "masodik/ovms/ovms_process.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdexcept>
#include <string>

namespace masodik::ovms {
namespace {

std::wstring quote_if_needed(const std::wstring& value) {
    if (value.find(L' ') == std::wstring::npos && value.find(L'"') == std::wstring::npos) {
        return value;
    }

    std::wstring quoted = L"\"";
    for (const wchar_t character : value) {
        if (character == L'"') {
            quoted += L"\\\"";
        } else {
            quoted += character;
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring build_command_line(const OvmsLaunchOptions& options) {
    std::wstring command_line = quote_if_needed(options.executable_path.wstring());
    for (const auto& argument : options.arguments) {
        command_line += L' ';
        command_line += quote_if_needed(std::wstring{argument.begin(), argument.end()});
    }
    return command_line;
}

}  // namespace

OvmsProcess::OvmsProcess(OvmsProcess&& other) noexcept { *this = std::move(other); }

OvmsProcess& OvmsProcess::operator=(OvmsProcess&& other) noexcept {
    if (this != &other) {
        close_handles();
        process_handle_ = other.process_handle_;
        thread_handle_ = other.thread_handle_;
        log_handle_ = other.log_handle_;
        process_id_ = other.process_id_;
        graceful_shutdown_timeout_ = other.graceful_shutdown_timeout_;
        log_path_ = std::move(other.log_path_);

        other.process_handle_ = nullptr;
        other.thread_handle_ = nullptr;
        other.log_handle_ = nullptr;
        other.process_id_ = 0;
    }
    return *this;
}

OvmsProcess::~OvmsProcess() { close_handles(); }

void OvmsProcess::start(const OvmsLaunchOptions& options) {
    if (is_running()) {
        throw std::runtime_error("Az OVMS folyamat mar fut.");
    }
    if (!std::filesystem::exists(options.executable_path)) {
        throw std::runtime_error("Az ovms.exe nem talalhato: " + options.executable_path.string());
    }

    log_path_ = options.log_path;
    graceful_shutdown_timeout_ = options.graceful_shutdown_timeout;

    SECURITY_ATTRIBUTES security_attributes{};
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.bInheritHandle = TRUE;

    if (!log_path_.empty()) {
        std::filesystem::create_directories(log_path_.parent_path());
        log_handle_ = CreateFileW(
            log_path_.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security_attributes,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (log_handle_ == INVALID_HANDLE_VALUE) {
            log_handle_ = nullptr;
            throw std::runtime_error("A log fajl nem nyithato meg: " + log_path_.string());
        }
    }

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    if (log_handle_ != nullptr) {
        startup_info.dwFlags |= STARTF_USESTDHANDLES;
        startup_info.hStdOutput = static_cast<HANDLE>(log_handle_);
        startup_info.hStdError = static_cast<HANDLE>(log_handle_);
        startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }

    PROCESS_INFORMATION process_info{};
    auto command_line = build_command_line(options);
    std::wstring working_directory =
        options.working_directory.empty() ? options.executable_path.parent_path().wstring() : options.working_directory.wstring();

    const auto success = CreateProcessW(
        options.executable_path.c_str(),
        command_line.data(),
        nullptr,
        nullptr,
        log_handle_ != nullptr,
        CREATE_NO_WINDOW,
        nullptr,
        working_directory.c_str(),
        &startup_info,
        &process_info);

    if (!success) {
        close_handles();
        throw std::runtime_error("Az OVMS folyamat letrehozasa sikertelen.");
    }

    process_handle_ = process_info.hProcess;
    thread_handle_ = process_info.hThread;
    process_id_ = process_info.dwProcessId;
}

void OvmsProcess::stop() {
    if (!is_running()) {
        close_handles();
        return;
    }

    const auto wait_result = WaitForSingleObject(static_cast<HANDLE>(process_handle_), static_cast<DWORD>(graceful_shutdown_timeout_.count()));
    if (wait_result == WAIT_TIMEOUT) {
        if (!TerminateProcess(static_cast<HANDLE>(process_handle_), 1U)) {
            throw std::runtime_error("Az OVMS folyamat nem allithato le.");
        }
        WaitForSingleObject(static_cast<HANDLE>(process_handle_), INFINITE);
    }

    close_handles();
}

bool OvmsProcess::is_running() const {
    if (process_handle_ == nullptr) {
        return false;
    }
    return WaitForSingleObject(static_cast<HANDLE>(process_handle_), 0U) == WAIT_TIMEOUT;
}

std::uint32_t OvmsProcess::process_id() const noexcept { return process_id_; }

std::optional<int> OvmsProcess::wait_for_exit(const std::chrono::milliseconds timeout) const {
    if (process_handle_ == nullptr) {
        return 0;
    }

    const auto wait_result = WaitForSingleObject(static_cast<HANDLE>(process_handle_), static_cast<DWORD>(timeout.count()));
    if (wait_result == WAIT_TIMEOUT) {
        return std::nullopt;
    }

    DWORD exit_code = 0;
    if (!GetExitCodeProcess(static_cast<HANDLE>(process_handle_), &exit_code)) {
        throw std::runtime_error("Az OVMS kilepesi kodja nem kerdezheto le.");
    }
    return static_cast<int>(exit_code);
}

const std::filesystem::path& OvmsProcess::log_path() const noexcept { return log_path_; }

void OvmsProcess::close_handles() noexcept {
    if (thread_handle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(thread_handle_));
        thread_handle_ = nullptr;
    }
    if (process_handle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(process_handle_));
        process_handle_ = nullptr;
    }
    if (log_handle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(log_handle_));
        log_handle_ = nullptr;
    }
    process_id_ = 0;
}

}  // namespace masodik::ovms
