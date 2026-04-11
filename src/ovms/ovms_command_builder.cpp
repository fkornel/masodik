#include "masodik/ovms/ovms_command_builder.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "masodik/runtime/launcher_config.hpp"

namespace masodik::ovms {
namespace {

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error("A graph template nem nyithato meg: " + path.string());
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

void write_text_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        throw std::runtime_error("A runtime graph fajl nem hozhato letre: " + path.string());
    }
    stream << content;
}

void replace_first(std::string& text, const std::string_view needle, const std::string& replacement) {
    const auto position = text.find(needle);
    if (position != std::string::npos) {
        text.replace(position, needle.size(), replacement);
    }
}

std::string escape_for_pbtxt_string(const std::filesystem::path& path) {
    std::string value = path.string();
    std::string escaped;
    escaped.reserve(value.size() * 2U);
    for (const char character : value) {
        if (character == '\\') {
            escaped += "\\\\";
        } else if (character == '"') {
            escaped += "\\\"";
        } else {
            escaped += character;
        }
    }
    return escaped;
}

std::string build_plugin_config_json(const masodik::runtime::LauncherConfig& launcher_config) {
    std::string json = "{\"CACHE_DIR\":\"cache\"";
    if (!launcher_config.serving_profile.model_distribution_policy.empty()) {
        json += ",\"MODEL_DISTRIBUTION_POLICY\":\"" + launcher_config.serving_profile.model_distribution_policy + "\"";
    }
    json += "}";
    return json;
}

std::string materialize_graph(const masodik::runtime::LauncherConfig& launcher_config, const std::string& plugin_config_json) {
    std::string graph = read_text_file(launcher_config.runtime_discovery.graph_template_path);
    replace_first(
        graph,
        "models_path: \"./\"",
        "models_path: \"" + escape_for_pbtxt_string(launcher_config.model_repository.root_path) + "\"");
    replace_first(graph, "device: \"GPU.0\"", "device: \"" + launcher_config.effective_device + "\"");
    replace_first(
        graph,
        "max_num_seqs:256",
        "max_num_seqs:" + std::to_string(launcher_config.serving_profile.max_num_seqs));
    replace_first(
        graph,
        "enable_prefix_caching: true",
        std::string{"enable_prefix_caching: "} +
            (launcher_config.serving_profile.prefix_caching_enabled ? "true" : "false"));
    replace_first(
        graph,
        "plugin_config: '{\"CACHE_DIR\":\"cache\"}'",
        "plugin_config: '" + plugin_config_json + "'");
    return graph;
}

}  // namespace

OvmsCommandBuildResult build_ovms_launch_options(const masodik::runtime::LauncherConfig& launcher_config) {
    const auto plugin_config_json = build_plugin_config_json(launcher_config);
    const auto graph = materialize_graph(launcher_config, plugin_config_json);
    write_text_file(launcher_config.runtime_discovery.runtime_graph_path, graph);

    OvmsLaunchOptions options;
    options.executable_path = launcher_config.runtime_discovery.ovms_exe_path;
    options.working_directory = launcher_config.runtime_discovery.runtime_graph_dir;
    options.log_path = launcher_config.runtime_discovery.runtime_log_path;
    options.arguments = {
        "--model_path",
        launcher_config.runtime_discovery.runtime_graph_dir.string(),
        "--model_name",
        launcher_config.serving_profile.model_name,
        "--port",
        std::to_string(launcher_config.runtime_discovery.grpc_port),
        "--rest_port",
        std::to_string(launcher_config.serving_profile.bind_port),
        "--rest_bind_address",
        launcher_config.serving_profile.bind_host,
        "--log_path",
        launcher_config.runtime_discovery.runtime_log_path.string(),
        "--log_level",
        "DEBUG"};

    return {
        .launch_options = std::move(options),
        .materialized_graph_path = launcher_config.runtime_discovery.runtime_graph_path,
        .effective_device = launcher_config.effective_device,
        .plugin_config_json = plugin_config_json};
}

}  // namespace masodik::ovms
