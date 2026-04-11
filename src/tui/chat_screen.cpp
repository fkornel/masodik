#include "masodik/tui/chat_screen.hpp"

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

ftxui::Element render_chat_screen(const ChatState& state) {
    using namespace ftxui;

    Elements message_elements;

    if (state.messages.empty()) {
        message_elements.push_back(
            text("  Ird be az elso uzeneted es nyomj Enter-t!") | dim | center);
    }

    for (const auto& msg : state.messages) {
        const auto role_color = msg.role == ChatMessage::Role::user ? Color::Green : Color::Cyan;
        const auto role_label = msg.role == ChatMessage::Role::user ? "[Te]" : "[AI]";

        Elements msg_parts;

        // Gondolkodas resz — halvany szinnel, csak amig a vegso valasz nem erkezett
        if (!msg.thinking_content.empty()) {
            if (msg.show_thinking) {
                // Meg gondolkodik — a thinking latszik halvany szinnel
                msg_parts.push_back(
                    vbox({
                        text("  Gondolkodas...") | bold | dim | color(Color::GrayLight),
                        paragraph(msg.thinking_content) | dim | color(Color::GrayDark),
                    }));
            }
            // Ha show_thinking == false, a thinking elrejtve (vegso valasz megerkezett)
        }

        // Fo tartalom (vegso valasz)
        if (!msg.content.empty()) {
            msg_parts.push_back(paragraph(msg.content));
        }

        // Streaming jelzo — ha meg sem a thinking sem a content nem ures,
        // es streaming aktiv, jelezzuk a varakozast
        if (msg.is_streaming && msg.content.empty() && msg.thinking_content.empty()) {
            msg_parts.push_back(text(" Varakozas...") | blink | color(Color::Yellow));
        }

        message_elements.push_back(hbox({
            text(std::string{role_label} + " ") | bold | color(role_color),
            vbox(std::move(msg_parts)) | flex,
        }));
        message_elements.push_back(text(""));
    }

    auto chat_area = vbox(std::move(message_elements)) | flex | yframe;

    return vbox({
        text(" masodik chat ") | bold | color(Color::Cyan) | center,
        separator(),
        chat_area,
    });
}

}  // namespace masodik::tui
