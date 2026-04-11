#pragma once

#include <string>
#include <vector>

#include "masodik/runtime/error_category.hpp"
#include "masodik/runtime/service_config.hpp"

namespace masodik::runtime {

struct ServiceDependencyValidation {
    bool valid = false;
    RuntimeError error = RuntimeError::none;
    RuntimeFailureScope failure_scope = RuntimeFailureScope::none;
    std::vector<std::string> notes;
};

class ServiceDependencyValidator {
public:
    [[nodiscard]] ServiceDependencyValidation validate(const ServiceConfig& service_config) const;
    [[nodiscard]] ServiceDependencyValidation validate_split_dependencies(const ServiceConfig& service_config) const;
    void ensure_valid(const ServiceConfig& service_config) const;
};

}  // namespace masodik::runtime
