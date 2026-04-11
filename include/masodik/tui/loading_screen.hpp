#pragma once

#include <string>

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

// A betoltesi kepernyo allapota — stage-enkent kulon allapotjelzo.
struct LoadingState {
    std::string profile_name;
    std::string model_name;
    std::string stage_1 = "Varakozas...";
    std::string stage_2 = "Varakozas...";
    std::string embedding = "Varakozas...";
};

// A betoltesi kepernyot rendereli.
ftxui::Element render_loading_screen(const LoadingState& state);

}  // namespace masodik::tui
