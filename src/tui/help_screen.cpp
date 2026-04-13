#include "masodik/tui/help_screen.hpp"

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

ftxui::Element render_help_screen() {
    using namespace ftxui;
    return vbox({
        text("") | size(HEIGHT, EQUAL, 1),
        text("  masodik — Billentyuzet sugo") | bold | color(Color::Cyan) | center,
        text("") | size(HEIGHT, EQUAL, 1),
        separator(),
        text("") | size(HEIGHT, EQUAL, 1),
        hbox({ text("  Enter       ") | bold | color(Color::Green), text("Uzenet kuldese") }),
        hbox({ text("  Alt+Enter   ") | bold | color(Color::Green), text("Uj sor a beviteli mezoben") }),
        hbox({ text("  Esc         ") | bold | color(Color::Green), text("Generalas megszakitasa") }),
        hbox({ text("  Fel nyil    ") | bold | color(Color::Green), text("Korabbi uzenet kivalasztasa") }),
        hbox({ text("  Le nyil     ") | bold | color(Color::Green), text("Kovetkezo uzenet / beviteli mezo") }),
        text("") | size(HEIGHT, EQUAL, 1),
        separator(),
        text("  Funkcio billentyuk") | bold,
        text("") | size(HEIGHT, EQUAL, 1),
        hbox({ text("  F1          ") | bold | color(Color::Cyan), text("Ez a surgo ablak") }),
        hbox({ text("  F2          ") | bold | color(Color::Cyan), text("Uj beszelgetes inditasa") }),
        hbox({ text("  F5          ") | bold | color(Color::Cyan), text("Modell ujratoltese") }),
        hbox({ text("  F9          ") | bold | color(Color::Cyan), text("HTTP webszerver be/ki") }),
        hbox({ text("  F10         ") | bold | color(Color::Yellow), text("Kilepes a programbol") }),
        hbox({ text("  F12         ") | bold | color(Color::Cyan), text("Diagnosztika (reszletes allapot)") }),
        text("") | size(HEIGHT, EQUAL, 1),
        separator(),
        text("  Nyomj Enter-t vagy Esc-et a bezarashoz.") | dim | center,
    }) | border | center;
}

}  // namespace masodik::tui
