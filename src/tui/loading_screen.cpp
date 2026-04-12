#include "masodik/tui/loading_screen.hpp"

#include "ftxui/dom/elements.hpp"

#include <string>

namespace masodik::tui {

namespace {

// Allapotjelzo szin, szimbolum es szazalek bar
ftxui::Element status_indicator(const std::string& label, const std::string& state, int percent) {
    using namespace ftxui;
    auto state_color = Color::Yellow;
    std::string symbol = " ...";
    if (state == "Kesz") { state_color = Color::Green; symbol = " OK"; }
    else if (state == "Hiba") { state_color = Color::Red; symbol = " !!"; }
    else if (state == "Betoltes...") { state_color = Color::Cyan; symbol = " >>"; }

    // Szazalekos progress bar ha van (nem 0 es nem 100)
    ftxui::Element progress_bar = text("");
    if (percent > 0 && percent < 100 && state == "Betoltes...") {
        const int bar_width = 20;
        const int filled = percent * bar_width / 100;
        std::string bar;
        for (int i = 0; i < bar_width; ++i) {
            bar += (i < filled) ? "\xe2\x96\x88" : "\xe2\x96\x91";  // █ es ░
        }
        progress_bar = hbox({
            text(" "),
            text(bar) | color(Color::Cyan),
            text(" " + std::to_string(percent) + "%") | dim,
        });
    }

    return hbox({
        text("  " + label + ": ") | bold,
        text("[" + state + symbol + "]") | color(state_color),
        progress_bar,
    });
}

}  // namespace

ftxui::Element render_loading_screen(const LoadingState& state) {
    using namespace ftxui;

    Elements model_info;
    model_info.push_back(text("  Chat modell: " + state.model_name) | dim);
    if (!state.embedding_model_name.empty()) {
        model_info.push_back(text("  Embedding modell: " + state.embedding_model_name) | dim);
    }

    return vbox({
        text("") | size(HEIGHT, EQUAL, 2),
        text("  masodik") | bold | color(Color::Cyan) | center,
        text("  Helyi hibrid AI kiszolgalo") | dim | center,
        text("") | size(HEIGHT, EQUAL, 1),
        separator(),
        text("") | size(HEIGHT, EQUAL, 1),
        text("  Profil: " + state.profile_name) | dim,
        vbox(std::move(model_info)),
        text("") | size(HEIGHT, EQUAL, 1),
        separator(),
        text("") | size(HEIGHT, EQUAL, 1),
        text("  Modell betoltese...") | bold,
        text("") | size(HEIGHT, EQUAL, 1),
        status_indicator("Stage-1 (dGPU)", state.stage_1, state.stage_1_percent),
        status_indicator("Stage-2 (iGPU)", state.stage_2, state.stage_2_percent),
        status_indicator("Embedding (NPU)", state.embedding, state.embedding_percent),
        text("") | size(HEIGHT, EQUAL, 1),
        separator(),
        text("  Varakozas a betoltesre... Nyomj Esc-et a megszakitashoz.") | dim,
    }) | border | center;
}

}  // namespace masodik::tui
