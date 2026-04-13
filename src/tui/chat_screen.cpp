#include "masodik/tui/chat_screen.hpp"
#include "masodik/tui/markdown_render.hpp"

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

ftxui::Element render_chat_screen(const ChatState& state, int iter,
                                  int browse_index) {
    using namespace ftxui;

    Elements message_elements;

    if (state.messages.empty()) {
        message_elements.push_back(
            text("  Ird be az elso uzeneted es nyomj Enter-t!") | dim | center);
    }

    // browse_index → a felhasznaloi uzenetek kozotti navigacio pozicioja.
    // Meg kell szamolni a user uzenetek indexeit, hogy a vizualis kiemeles
    // a megfelelo pozicion jelenjen meg.
    int user_msg_counter = -1;

    for (std::size_t i = 0; i < state.messages.size(); ++i) {
        const auto& msg = state.messages[i];
        const auto role_color = msg.role == ChatMessage::Role::user ? Color::Green : Color::Cyan;

        // A nev mezo-bol olvassuk a megjelenitheto nevet (konfigbol tolti a tui_app)
        std::string role_label;
        if (!msg.name.empty()) {
            role_label = "[" + msg.name + "]";
        } else {
            role_label = msg.role == ChatMessage::Role::user ? "[Te]" : "[AI]";
        }

        if (msg.role == ChatMessage::Role::user) {
            ++user_msg_counter;
        }

        Elements msg_parts;

        // Gondolkodas resz — halvany szinnel, csak amig a vegso valasz nem erkezett
        if (!msg.thinking_content.empty()) {
            if (msg.show_thinking) {
                msg_parts.push_back(
                    vbox({
                        text("  Gondolkodas...") | bold | dim | color(Color::GrayLight),
                        paragraph(msg.thinking_content) | dim | color(Color::GrayDark),
                    }));
            }
        }

        // Fo tartalom — AI valaszoknál markdown rendereles, user uzeneteknel paragraph.
        // Hosszu felhasznaloi uzenetek osszevontan jelennek meg.
        if (!msg.content.empty()) {
            if (msg.role == ChatMessage::Role::assistant) {
                msg_parts.push_back(render_markdown(msg.content));
            } else {
                // Sorok szamolasa
                int line_count = 1;
                for (const char ch : msg.content) {
                    if (ch == '\n') ++line_count;
                }
                if (line_count > 5 || msg.content.size() > 500) {
                    // Osszevont megjelentes: elso sor + statisztika
                    auto first_nl = msg.content.find('\n');
                    auto first_line = (first_nl != std::string::npos)
                        ? msg.content.substr(0, first_nl) : msg.content;
                    if (first_line.size() > 80) first_line = first_line.substr(0, 77) + "...";
                    auto summary = "[" + std::to_string(line_count) + " sor, " +
                        std::to_string(msg.content.size()) + " karakter]";
                    msg_parts.push_back(vbox({
                        paragraph(first_line),
                        text("  " + summary) | dim | color(Color::GrayLight),
                    }));
                } else {
                    msg_parts.push_back(paragraph(msg.content));
                }
            }
        }

        // Streaming jelzo
        if (msg.is_streaming && msg.content.empty() && msg.thinking_content.empty()) {
            msg_parts.push_back(text(" Varakozas...") | blink | color(Color::Yellow));
        }

        auto msg_row = hbox({
            text(role_label + " ") | bold | color(role_color),
            vbox(std::move(msg_parts)) | flex,
        });

        // Browse navigacio kiemeles + scroll kovetés
        // Ha a felhasznaloi uzenet ki van valasztva, invertaljuk es rarakjuk a focus-t
        // igy az yframe odagorget. Ha nincs browse, a focus az aljara kerul.
        bool is_selected = (msg.role == ChatMessage::Role::user &&
                            browse_index >= 0 && user_msg_counter == browse_index);
        if (is_selected) {
            msg_row = msg_row | inverted | focus;
        }

        message_elements.push_back(std::move(msg_row));
        message_elements.push_back(text(""));
    }

    // Ha nincs browse mod (browse_index < 0), a focus az aljara kerul
    // igy a chat mindig a legfrissebb uzeneteknel all
    if (browse_index < 0) {
        message_elements.push_back(text("") | focus);
    }

    auto chat_area = vbox(std::move(message_elements)) | flex | yframe;

    return vbox({
        text(" masodik" + (iter > 0 ? " #" + std::to_string(iter) : "") + " ") | bold | color(Color::Cyan) | center,
        separator(),
        chat_area,
    });
}

}  // namespace masodik::tui
