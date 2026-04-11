#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <thread>

#include "masodik/config/default_paths.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/runtime/health_monitor.hpp"
#include "masodik/runtime/network_binding.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/server/http_server.hpp"
#include "masodik/server/openai_handlers.hpp"

namespace masodik::benchmarks {

inline std::filesystem::path find_repo_root() {
    auto current = std::filesystem::current_path();
    while (!current.empty()) {
        if (std::filesystem::exists(current / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json")) {
            return current;
        }
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    throw std::runtime_error("A benchmark nem talalta meg a repository gyokeret.");
}

class LocalBenchmarkServer {
public:
    explicit LocalBenchmarkServer(
        const unsigned short port,
        const std::filesystem::path& profile_path = find_repo_root() / "config" / "profiles" / "b580-manual-split-cut23-tail-32k-u8.json",
        const std::filesystem::path& model_root = masodik::config::default_paths::default_model_root) {
        service_config_ = masodik::runtime::build_service_config(
            profile_path,
            model_root,
            find_repo_root());
        masodik::runtime::apply_network_binding(service_config_, service_config_.bind_host, port);

        pipeline_service_ = std::make_unique<masodik::genai::LlmPipelineService>(service_config_);
        pipeline_service_->initialize();
        if (!pipeline_service_->ready()) {
            throw std::runtime_error(pipeline_service_->status_message());
        }

        masodik::server::register_openai_routes(server_, service_config_, *pipeline_service_);
        server_.start({.bind_host = service_config_.bind_host, .bind_port = service_config_.bind_port});

        masodik::runtime::HealthMonitor health_monitor;
        const auto snapshot = health_monitor.wait_until_ready(
            service_config_.health_url,
            std::chrono::minutes{10},
            service_config_.active_profile,
            service_config_.active_model);
        if (!snapshot.ready) {
            throw std::runtime_error(snapshot.last_error_message.empty()
                                         ? "A benchmark szerver nem valt elerhetove."
                                         : snapshot.last_error_message);
        }
    }

    ~LocalBenchmarkServer() { server_.stop(); }

    [[nodiscard]] const masodik::runtime::ServiceConfig& service_config() const noexcept { return service_config_; }

private:
    masodik::runtime::ServiceConfig service_config_;
    std::unique_ptr<masodik::genai::LlmPipelineService> pipeline_service_;
    masodik::server::HttpServer server_;
};

}  // namespace masodik::benchmarks
