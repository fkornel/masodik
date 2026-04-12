#pragma once

#include <string>

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

// A betoltesi kepernyo allapota — stage-enkent kulon allapotjelzo.
struct LoadingState {
    std::string profile_name;
    std::string model_name;
    std::string embedding_model_name;  // Az embedding modell neve (pl. Qwen3-Embedding-0.6B)
    std::string stage_1 = "Varakozas...";
    std::string stage_2 = "Varakozas...";
    std::string embedding = "Varakozas...";
    int stage_1_percent = 0;   // 0-100, ha elerheto
    int stage_2_percent = 0;
    int embedding_percent = 0;
};

// A betoltesi kepernyot rendereli.
ftxui::Element render_loading_screen(const LoadingState& state);

}  // namespace masodik::tui
