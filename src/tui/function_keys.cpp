#include "masodik/tui/function_keys.hpp"

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

ftxui::Element render_function_key_bar() {
    using namespace ftxui;
    return hbox({
        text("F1") | bold | color(Color::Cyan), text(":Sugo ") | dim,
        text("F2") | bold | color(Color::Cyan), text(":Uj ") | dim,
        text("F3") | bold | color(Color::Cyan), text(":Profil ") | dim,
        text("F5") | bold | color(Color::Cyan), text(":Ujratolt ") | dim,
        text("F10") | bold | color(Color::Cyan), text(":Web ") | dim,
        text("F12") | bold | color(Color::Cyan), text(":Diag ") | dim,
        text("Esc") | bold | color(Color::Yellow), text(":Kilepes") | dim,
    });
}

}  // namespace masodik::tui
