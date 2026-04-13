#pragma once

#include <string>

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

// Markdown szoveg → FTXUI Element konverzio.
// Tamogatott jelolok: **bold**, `inline kod`, ``` kodblokk ```, \n ujsor.
ftxui::Element render_markdown(const std::string& text);

}  // namespace masodik::tui
