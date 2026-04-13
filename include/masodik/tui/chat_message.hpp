#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace masodik::tui {

// Egy chat uzenet — a felhasznalo vagy az asszisztens altal irt szoveg.
struct ChatMessage {
    enum class Role { user, assistant };

    Role role = Role::user;
    std::string name;              // Megjelenitheto nev (pl. "Kornel", "Szilvabot")
    std::string content;
    std::string thinking_content;  // A modell gondolkodasi resze (<think> blokk)
    bool is_streaming = false;     // Jelenleg streamelodik-e a valasz
    bool show_thinking = false;    // A gondolkodas resz lathatosaga
};

// A chat kepernyo teljes allapota.
struct ChatState {
    std::vector<ChatMessage> messages;
    std::string current_input;
    std::size_t scroll_offset = 0;
    bool generating = false;
};

}  // namespace masodik::tui
