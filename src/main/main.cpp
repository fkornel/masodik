#include "masodik/diagnostics/logger.hpp"
#include "launcher_app.hpp"
#include "tui_app.hpp"

#include <cstring>
#include <string>

// A masodik alkalmazas belepesi pontja.
// Alapertelmezetten TUI modban indul (interaktiv chat interfesz).
// A --serve kapcsoloval HTTP szerver modban indul (OpenAI-kompatibilis API).
int main(int argc, char** argv) {
    auto logger = masodik::diagnostics::Logger::create_console_logger();

    // Mod valasztas: --serve → HTTP szerver, egyebkent TUI
    bool serve_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--serve") == 0) {
            serve_mode = true;
            break;
        }
    }

    try {
        if (serve_mode) {
            return run_launcher_app(argc, argv);
        }
        return run_tui_app(argc, argv);
    } catch (const std::exception& error) {
        logger.error(error.what());
        return 1;
    }
}
