#include "masodik/tui/loading_screen.hpp"

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

namespace {

// Allapotjelzo szin es szimbolum
ftxui::Element status_indicator(const std::string& label, const std::string& state) {
    using namespace ftxui;
    auto state_color = Color::Yellow;
    std::string symbol = "...";
    if (state == "Kesz") { state_color = Color::Green; symbol = " OK"; }
    else if (state == "Hiba") { state_color = Color::Red; symbol = " !!"; }
    else if (state == "Betoltes...") { state_color = Color::Cyan; symbol = " >>"; }

    return hbox({
        text("  " + label + ": ") | bold,
        text("[" + state + symbol + "]") | color(state_color),
    });
}

}  // namespace

ftxui::Element render_loading_screen(const LoadingState& state) {
    using namespace ftxui;
    return vbox({
        text("") | size(HEIGHT, EQUAL, 2),
        text("  masodik") | bold | color(Color::Cyan) | center,
        text("  Helyi hibrid AI kiszolgalo") | dim | center,
        text("") | size(HEIGHT, EQUAL, 1),
        separator(),
        text("") | size(HEIGHT, EQUAL, 1),
        text("  Profil: " + state.profile_name) | dim,
        text("  Modell: " + state.model_name) | dim,
        text("") | size(HEIGHT, EQUAL, 1),
        separator(),
        text("") | size(HEIGHT, EQUAL, 1),
        text("  Modell betoltese...") | bold,
        text("") | size(HEIGHT, EQUAL, 1),
        status_indicator("Stage-1 (dGPU)", state.stage_1),
        status_indicator("Stage-2 (iGPU)", state.stage_2),
        status_indicator("Embedding (NPU)", state.embedding),
        text("") | size(HEIGHT, EQUAL, 1),
        separator(),
        text("  Varakozas a betoltesre... Nyomj Esc-et a megszakitashoz.") | dim,
    }) | border | center;
}

}  // namespace masodik::tui
