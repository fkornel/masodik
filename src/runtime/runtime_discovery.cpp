#include "masodik/runtime/runtime_discovery.hpp"

#include <stdexcept>

#include "masodik/config/default_paths.hpp"

namespace masodik::runtime {
namespace {

std::string build_url(
    const std::string_view host,
    const std::uint16_t port,
    const std::string_view path) {
    return "http://" + std::string{host} + ":" + std::to_string(port) + std::string{path};
}

}  // namespace

void RuntimeDiscovery::validate() const {
    if (ovms_exe_path.empty() || !std::filesystem::exists(ovms_exe_path)) {
        throw std::invalid_argument("Az OVMS futtathato allomany nem talalhato.");
    }
    if (graph_template_path.empty() || !std::filesystem::exists(graph_template_path)) {
        throw std::invalid_argument("A graph template nem talalhato.");
    }
    if (runtime_graph_path.empty()) {
        throw std::invalid_argument("A runtime graph path nem lehet ures.");
    }
    if (ready_url.empty() || chat_url.empty() || models_url.empty()) {
        throw std::invalid_argument("A runtime endpoint URL-ek nem lehetnek uresek.");
    }
}

RuntimeDiscovery discover_runtime(
    const masodik::config::ToolchainProfile&,
    const masodik::config::ModelRepository& model_repository,
    const masodik::config::ServingProfile& serving_profile,
    const std::filesystem::path& repo_root) {
    RuntimeDiscovery discovery;
    discovery.ovms_exe_path = masodik::config::default_paths::default_ovms_exe_path;
    discovery.graph_template_path =
        std::filesystem::exists(model_repository.graph_path) ? model_repository.graph_path : serving_profile.ovms_graph_path;
    discovery.runtime_root = repo_root / "build" / "runtime" / serving_profile.profile_name;
    discovery.runtime_graph_dir = discovery.runtime_root / "servable";
    discovery.runtime_graph_path = discovery.runtime_graph_dir / "graph.pbtxt";
    discovery.runtime_log_path = discovery.runtime_root / "ovms.log";
    discovery.runtime_pid_path = discovery.runtime_root / "ovms.pid";
    discovery.grpc_port = static_cast<std::uint16_t>(serving_profile.bind_port + 1U);
    discovery.ready_url = build_url(serving_profile.bind_host, serving_profile.bind_port, serving_profile.ready_endpoint);
    discovery.chat_url = build_url(serving_profile.bind_host, serving_profile.bind_port, serving_profile.chat_endpoint);
    discovery.models_url = build_url(serving_profile.bind_host, serving_profile.bind_port, serving_profile.models_endpoint);
    discovery.chat_fallback_url = build_url(serving_profile.bind_host, serving_profile.bind_port, "/v3/chat/completions");
    discovery.models_fallback_url = build_url(serving_profile.bind_host, serving_profile.bind_port, "/v3/models");
    discovery.validate();
    return discovery;
}

}  // namespace masodik::runtime
