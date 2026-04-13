#pragma once

#include <string>

#include "masodik/tui/chat_message.hpp"

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

// A chat kepernyot rendereli — uzenetek listaja scrollable teruleten.
// Az iter szam a fejlecben jelenik meg (build iteracio).
// A browse_index az uzenet szerkesztes navigacio jelenlegi pozicioja (-1 = nincs).
ftxui::Element render_chat_screen(const ChatState& state, int iter = 0,
                                  int browse_index = -1);

}  // namespace masodik::tui
