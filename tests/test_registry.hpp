#pragma once

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace masodik::tests {

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline void register_test(std::string name, std::function<void()> body) {
    registry().push_back({std::move(name), std::move(body)});
}

inline void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

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
    throw std::runtime_error("A teszt nem talalta meg a repository gyokeret.");
}

}  // namespace masodik::tests

#define MASODIK_TEST(Name)                                                                        \
    static void Name();                                                                           \
    namespace {                                                                                   \
    const bool Name##_registered = [] {                                                           \
        masodik::tests::register_test(#Name, &Name);                                              \
        return true;                                                                              \
    }();                                                                                          \
    }                                                                                             \
    static void Name()
