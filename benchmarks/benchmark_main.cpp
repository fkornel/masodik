#include <iostream>
#include <string>

#include "benchmark_registry.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Hasznalat: masodik-benchmarks <scenario>\n";
        std::cerr << "Elheto scenariok:\n";
        for (const auto& scenario : masodik::benchmarks::registry()) {
            std::cerr << "  - " << scenario.name << '\n';
        }
        return 1;
    }

    const std::string requested = argv[1];
    for (const auto& scenario : masodik::benchmarks::registry()) {
        if (scenario.name == requested) {
            return scenario.body(argc - 1, argv + 1);
        }
    }

    std::cerr << "Ismeretlen benchmark scenario: " << requested << '\n';
    return 1;
}
