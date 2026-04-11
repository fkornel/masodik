#pragma once

#include <functional>
#include <string>
#include <vector>

namespace masodik::benchmarks {

struct BenchmarkScenario {
    std::string name;
    std::function<int(int, char**)> body;
};

inline std::vector<BenchmarkScenario>& registry() {
    static std::vector<BenchmarkScenario> scenarios;
    return scenarios;
}

inline void register_benchmark(std::string name, std::function<int(int, char**)> body) {
    registry().push_back({std::move(name), std::move(body)});
}

}  // namespace masodik::benchmarks

#define MASODIK_BENCHMARK(Name)                                                                   \
    static int Name(int argc, char** argv);                                                       \
    namespace {                                                                                   \
    const bool Name##_registered = [] {                                                           \
        masodik::benchmarks::register_benchmark(#Name, &Name);                                    \
        return true;                                                                              \
    }();                                                                                          \
    }                                                                                             \
    static int Name(int argc, char** argv)
