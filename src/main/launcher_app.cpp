#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "launcher_app.hpp"

#include <crtdbg.h>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include "masodik/config/default_paths.hpp"
#include "masodik/config/json_loader.hpp"
#include "masodik/diagnostics/logger.hpp"
#include "masodik/diagnostics/runtime_error_reporter.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/service_lifecycle.hpp"
#include "masodik/runtime/service_config.hpp"

namespace {

std::atomic<bool> stop_requested = false;
masodik::diagnostics::Logger* active_logger = nullptr;

struct LaunchArguments {
    std::filesystem::path profile_path = std::filesystem::path{"config"} / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json";
    std::filesystem::path model_root = masodik::config::default_paths::default_model_root;
    std::filesystem::path repo_root = std::filesystem::current_path();
    bool validate_and_stop = false;
};

LaunchArguments parse_arguments(const int argc, char** argv) {
    LaunchArguments arguments;

    // Alapertekek a config/masodik.json fajlbol
    const auto config_path = arguments.repo_root / "config" / "masodik.json";
    if (std::filesystem::exists(config_path)) {
        try {
            const auto config_value = masodik::config::JsonLoader::load_file(config_path);
            if (config_value.is_object()) {
                const auto& obj = config_value.as_object();
                const auto profile = masodik::config::get_string_or(obj, "profile", "");
                const auto model = masodik::config::get_string_or(obj, "model_root", "");
                if (!profile.empty()) arguments.profile_path = profile;
                if (!model.empty()) arguments.model_root = model;
            }
        } catch (...) {}
    }

    for (int index = 1; index < argc; ++index) {
        const std::string current = argv[index];
        if (current == "--profile" && index + 1 < argc) {
            arguments.profile_path = argv[++index];
            continue;
        }
        if (current == "--model-root" && index + 1 < argc) {
            arguments.model_root = argv[++index];
            continue;
        }
        if (current == "--repo-root" && index + 1 < argc) {
            arguments.repo_root = argv[++index];
            continue;
        }
        if (current == "--validate-and-stop") {
            arguments.validate_and_stop = true;
            continue;
        }
        throw std::invalid_argument("Ismeretlen vagy hianyos argumentum: " + current);
    }

    // Relativ utvonalak feloldasa a repo_root-hoz kepest
    if (arguments.profile_path.is_relative()) {
        arguments.profile_path = arguments.repo_root / arguments.profile_path;
    }
    if (arguments.model_root.is_relative()) {
        arguments.model_root = arguments.repo_root / arguments.model_root;
    }
    return arguments;
}

masodik::runtime::RuntimeFailureScope parse_failure_scope(const std::string_view raw_scope) {
    if (raw_scope == "stage-1") {
        return masodik::runtime::RuntimeFailureScope::stage_1;
    }
    if (raw_scope == "stage-2") {
        return masodik::runtime::RuntimeFailureScope::stage_2;
    }
    if (raw_scope == "handoff") {
        return masodik::runtime::RuntimeFailureScope::handoff;
    }
    if (raw_scope == "startup") {
        return masodik::runtime::RuntimeFailureScope::startup;
    }
    return masodik::runtime::RuntimeFailureScope::none;
}

BOOL WINAPI handle_console_signal(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        stop_requested = true;
        return TRUE;
    }
    return FALSE;
}

void log_if_available(const std::string_view message) {
    if (active_logger != nullptr) {
        active_logger->error(message);
    }
}

void disable_error_dialogs() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
}

void __cdecl handle_invalid_parameter(
    const wchar_t*,
    const wchar_t*,
    const wchar_t*,
    unsigned int,
    uintptr_t) {
    log_if_available("CRT invalid parameter hiba tortent. A szolgaltatas leall.");
    stop_requested = true;
    std::abort();
}

LONG WINAPI handle_unhandled_exception(EXCEPTION_POINTERS* ep) {
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    const void* const instr = ep->ExceptionRecord->ExceptionAddress;

    // Azonosítsuk melyik DLL-ben van a crash
    HMODULE crash_module = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        static_cast<LPCSTR>(static_cast<const void*>(instr)),
        &crash_module);
    char module_name[MAX_PATH] = {};
    GetModuleFileNameA(crash_module, module_name, MAX_PATH);
    const auto module_base = reinterpret_cast<uintptr_t>(crash_module);
    const auto instr_offset = reinterpret_cast<uintptr_t>(instr) - module_base;

    std::ostringstream stream;
    stream << "Nem kezelt Windows exception. Kod=0x" << std::hex << code
           << " Cim=0x" << reinterpret_cast<uintptr_t>(instr) << std::dec
           << ". A szolgaltatas leall.";
    log_if_available(stream.str());

    {
        std::ostringstream mod;
        mod << "DLL: " << module_name
            << " Alap=0x" << std::hex << module_base
            << " Ofszet=0x" << instr_offset << std::dec;
        log_if_available(mod.str());
    }

    // ACCESS_VIOLATION (0xC0000005) esetén a rossz pointer is loggolható
    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
        const bool is_write = (ep->ExceptionRecord->ExceptionInformation[0] == 1);
        const uintptr_t bad_addr = ep->ExceptionRecord->ExceptionInformation[1];
        std::ostringstream details;
        details << "Access violation: " << (is_write ? "IRAS" : "OLVASAS")
                << " rossz cim=0x" << std::hex << bad_addr << std::dec;
        log_if_available(details.str());
    }

    stop_requested = true;
    return EXCEPTION_EXECUTE_HANDLER;
}

void handle_terminate() {
    if (const auto exception = std::current_exception(); exception != nullptr) {
        try {
            std::rethrow_exception(exception);
        } catch (const std::exception& error) {
            log_if_available(std::string{"Nem kezelt C++ kivetel miatt terminate tortent: "} + error.what());
        } catch (...) {
            log_if_available("Ismeretlen nem kezelt C++ kivetel miatt terminate tortent.");
        }
    } else {
        log_if_available("std::terminate futott le aktiv kivetel nelkul.");
    }
    stop_requested = true;
    std::abort();
}

void handle_signal(const int signal) {
    std::ostringstream stream;
    stream << "Fatal signal erkezett: " << signal << ". A szolgaltatas leall.";
    log_if_available(stream.str());
    stop_requested = true;
    std::_Exit(128 + signal);
}

void install_crash_handlers(masodik::diagnostics::Logger& logger) {
    active_logger = &logger;
    disable_error_dialogs();
    _set_invalid_parameter_handler(handle_invalid_parameter);
    SetUnhandledExceptionFilter(handle_unhandled_exception);
    std::set_terminate(handle_terminate);
    // SIGSEGV és SIGILL szándékosan NEM kerül ide: Windows/MSVC-n a CRT POSIX
    // signal handlere megelőzi az SEH __try/__except blokkokat, ezért az OpenVINO
    // GPU plugin belső hibakezelői sosem futhatnának le. Az access violation /
    // illegal instruction típusú kivételeket a SetUnhandledExceptionFilter kezeli.
    std::signal(SIGABRT, handle_signal);
}

}  // namespace

int run_launcher_app(int argc, char** argv) {
    const auto arguments = parse_arguments(argc, argv);
    const auto service_config =
        masodik::runtime::build_service_config(arguments.profile_path, arguments.model_root, arguments.repo_root);
    auto logger = masodik::diagnostics::Logger::create_file_logger(service_config.serving_profile.runtime_log_path);
    install_crash_handlers(logger);
    logger.info(std::string{"Runtime log fajl: "} + service_config.serving_profile.runtime_log_path.string());
    logger.info(std::string{"Startup profil: "} + arguments.profile_path.string());
    logger.info("ServiceConfig sikeresen felepult.");


    masodik::runtime::ServiceLifecycle lifecycle{service_config};
    logger.info("ServiceLifecycle.start() meghivasa kovetkezik.");
    lifecycle.start();
    logger.info("ServiceLifecycle.start() sikeresen visszatert.");
    const auto& pipeline_service = lifecycle.pipeline_service();

    logger.info(std::string{"Aktiv profil: "} + service_config.active_profile);
    logger.info(std::string{"Aktiv modell: "} + service_config.active_model);
    logger.info(std::string{"Effektiv eszkozprofil: "} + service_config.effective_device);
    logger.info(std::string{"Pipeline allapot: "} + pipeline_service.state());
    logger.info(std::string{"Pipeline uzenet: "} + pipeline_service.status_message());
    const auto health = pipeline_service.make_health_response();
    if (health.split_mode) {
        logger.info(
            "Split startup allapot: profile=" + health.split_profile +
            " stage_1=" + health.stage_1_state +
            " stage_2=" + health.stage_2_state +
            " handoff=" + health.handoff_state +
            " fallback_detected=" + std::string{health.fallback_detected ? "true" : "false"});
        logger.info(
            "Split long-context limitek: context_cap=" +
            std::to_string(service_config.generation_config.split_context_token_cap(
                service_config.model_repository.split_artifact_manifest.max_context_tokens)) +
            " stage_1_budget_gb=" + std::to_string(service_config.generation_config.stage_1_memory_budget_gb) +
            " stage_2_budget_gb=" + std::to_string(service_config.generation_config.stage_2_memory_budget_gb) +
            " queue_overflow=" + masodik::runtime::to_string(service_config.serving_profile.queue_policy.overflow_behavior) +
            " max_queued=" + std::to_string(service_config.serving_profile.queue_policy.max_queued_requests));
    }
    if (!pipeline_service.ready()) {
        logger.error(masodik::diagnostics::build_runtime_error_report(
            pipeline_service.last_error(),
            pipeline_service.status_message(),
            service_config,
            {.split_mode = health.split_mode,
             .failure_scope = parse_failure_scope(health.last_failure_scope),
             .fallback_detected = health.fallback_detected,
             .stage_1_state = health.stage_1_state,
             .stage_2_state = health.stage_2_state,
             .handoff_state = health.handoff_state}));
    }

    if (arguments.validate_and_stop) {
        return pipeline_service.ready() ? 0 : 1;
    }

    if (!SetConsoleCtrlHandler(handle_console_signal, TRUE)) {
        throw std::runtime_error("A konzol leallitasi handler nem allithato be.");
    }

    logger.info(std::string{"Health endpoint: "} + service_config.health_url);
    logger.info(std::string{"Models endpoint: "} + service_config.models_url);
    logger.info(std::string{"Chat endpoint: "} + service_config.chat_url);

    while (!stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds{250});
    }
    lifecycle.stop();
    return 0;
}
