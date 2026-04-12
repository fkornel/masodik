#pragma once

#include <string>

#include "ftxui/dom/elements.hpp"
#include "masodik/tui/resource_monitor.hpp"

namespace masodik::tui {

// Az allapotsor tartalma — eroforras monitor + context + tok/s + web.
struct StatusBarInfo {
    std::string model_name;       // Csak F12 diagnosztikaban jelenik meg
    std::string device_profile;   // Csak F12 diagnosztikaban jelenik meg
    int context_used = 0;
    int context_total = 0;
    double tokens_per_second = 0.0;
    std::string state = "Betoltes...";
    bool web_server_active = false;
    int web_server_port = 0;
    int iter = 0;                 // Build iteracio szam
    int uptime_seconds = 0;       // Alkalmazas futo ideje masodpercben
    std::string hint_message;     // Dinamikus hint uzenet (halvany sarga, eltunik)
    ResourceSnapshot resources;   // CPU, RAM, dGPU, iGPU valos ideju adatok
};

// Az allapotsort rendereli — eroforras bar-ok + context + tok/s.
ftxui::Element render_status_bar(const StatusBarInfo& info);

}  // namespace masodik::tui
