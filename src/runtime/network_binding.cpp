#include "masodik/runtime/network_binding.hpp"

#include <stdexcept>

namespace masodik::runtime {
namespace {

std::string build_url(const std::string_view host, const std::uint16_t port, const std::string_view path) {
    return "http://" + std::string{host} + ":" + std::to_string(port) + std::string{path};
}

}  // namespace

void validate_network_binding(const std::string_view bind_host, const std::uint16_t bind_port) {
    if (bind_host.empty()) {
        throw std::invalid_argument("A bind_host nem lehet ures.");
    }
    if (bind_port == 0U) {
        throw std::invalid_argument("A bind_port nem lehet 0.");
    }
}

void apply_network_binding(ServiceConfig& service_config, std::string bind_host, const std::uint16_t bind_port) {
    validate_network_binding(bind_host, bind_port);

    service_config.bind_host = std::move(bind_host);
    service_config.bind_port = bind_port;
    service_config.health_url = build_url(service_config.bind_host, service_config.bind_port, service_config.health_endpoint_path);
    service_config.chat_url = build_url(service_config.bind_host, service_config.bind_port, service_config.chat_endpoint_path);
    service_config.models_url = build_url(service_config.bind_host, service_config.bind_port, service_config.models_endpoint_path);
}

}  // namespace masodik::runtime
