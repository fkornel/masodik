#pragma once

#include "masodik/tui/chat_message.hpp"

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

// A chat kepernyot rendereli — uzenetek listaja scrollable teruleten.
ftxui::Element render_chat_screen(const ChatState& state);

}  // namespace masodik::tui
