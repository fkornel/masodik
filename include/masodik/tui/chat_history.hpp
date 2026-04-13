#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "masodik/tui/chat_message.hpp"

namespace masodik::tui {

// JSONL alapu chat history mentes es betoltes.
// Append-only iras (std::ofstream ios::app), kivetel: szerkesztes utan teljes ujrairas.
namespace chat_history {

// Betolti a JSONL fajlbol az osszes uzenetet. Serult sorok kihagyasra kerulnek.
std::vector<ChatMessage> load(const std::filesystem::path& path);

// Egyetlen uzenet hozzafuzese a JSONL fajlhoz (append).
void append(const std::filesystem::path& path, const ChatMessage& message);

// Teljes ujrairas — szerkesztes eseten hasznalt.
void rewrite(const std::filesystem::path& path, const std::vector<ChatMessage>& messages);

}  // namespace chat_history

}  // namespace masodik::tui
