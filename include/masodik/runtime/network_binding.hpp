#pragma once

#include <cstdint>
#include <string>

#include "masodik/runtime/service_config.hpp"

namespace masodik::runtime {

void validate_network_binding(std::string_view bind_host, std::uint16_t bind_port);
void apply_network_binding(ServiceConfig& service_config, std::string bind_host, std::uint16_t bind_port);

}  // namespace masodik::runtime
