#include <exception>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_registry.hpp"

int main(int argc, char** argv) {
    std::string_view filter;
    if (argc > 1) {
        filter = argv[1];
    } else if (const char* env_filter = std::getenv("MASODIK_TEST_FILTER"); env_filter != nullptr) {
        filter = env_filter;
    }

    int failures = 0;
    for (const auto& test : masodik::tests::registry()) {
        if (!filter.empty() && test.name.find(filter) == std::string::npos) {
            continue;
        }
        std::cout << "[RUN ] " << test.name << '\n';
        try {
            test.body();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }

    return failures == 0 ? 0 : 1;
}
