#pragma once

#include "masodik/tui/chat_message.hpp"

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

// A chat kepernyot rendereli — uzenetek listaja scrollable teruleten.
// Az iter szam a fejlecben jelenik meg (build iteracio).
ftxui::Element render_chat_screen(const ChatState& state, int iter = 0);

}  // namespace masodik::tui
