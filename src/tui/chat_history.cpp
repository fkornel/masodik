#include "masodik/tui/chat_history.hpp"

#include <fstream>
#include <sstream>
#include <chrono>

#include "masodik/config/json_loader.hpp"

namespace masodik::tui::chat_history {

namespace {

// Egyetlen uzenet JSONL sorra alakitasa (egy sor, nincs sortores a tartalomban).
// Egyszeru JSON serializer — nincs kulso fuggoseg.
std::string escape_json_string(const std::string& input) {
    std::string result;
    result.reserve(input.size() + 16);
    for (const char ch : input) {
        switch (ch) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += ch; break;
        }
    }
    return result;
}

std::string message_to_jsonl(const ChatMessage& msg) {
    std::string role_str = msg.role == ChatMessage::Role::user ? "user" : "assistant";
    std::string line = "{\"role\":\"" + role_str + "\"";
    if (!msg.name.empty()) {
        line += ",\"name\":\"" + escape_json_string(msg.name) + "\"";
    }
    line += ",\"content\":\"" + escape_json_string(msg.content) + "\"";
    if (!msg.thinking_content.empty()) {
        line += ",\"thinking\":\"" + escape_json_string(msg.thinking_content) + "\"";
    }
    line += "}";
    return line;
}

}  // namespace

std::vector<ChatMessage> load(const std::filesystem::path& path) {
    std::vector<ChatMessage> messages;
    if (!std::filesystem::exists(path)) return messages;

    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) return messages;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        try {
            auto value = masodik::config::JsonLoader::parse(line);
            if (!value.is_object()) continue;
            const auto& obj = value.as_object();

            ChatMessage msg;
            auto role_str = masodik::config::get_string_or(obj, "role", "user");
            msg.role = (role_str == "assistant")
                ? ChatMessage::Role::assistant
                : ChatMessage::Role::user;
            msg.name = masodik::config::get_string_or(obj, "name", "");
            msg.content = masodik::config::get_string_or(obj, "content", "");
            msg.thinking_content = masodik::config::get_string_or(obj, "thinking", "");

            if (!msg.content.empty() || !msg.thinking_content.empty()) {
                messages.push_back(std::move(msg));
            }
        } catch (...) {
            // Serult sor — kihagyjuk (FR-014: ures chattel indul, nem omlik ossze)
            continue;
        }
    }
    return messages;
}

void append(const std::filesystem::path& path, const ChatMessage& message) {
    // data/ konyvtar letrehozasa ha nem letezik
    auto parent = path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(path, std::ios::app);
    if (file.is_open()) {
        file << message_to_jsonl(message) << '\n';
    }
}

void rewrite(const std::filesystem::path& path, const std::vector<ChatMessage>& messages) {
    auto parent = path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(path, std::ios::trunc);
    if (file.is_open()) {
        for (const auto& msg : messages) {
            file << message_to_jsonl(msg) << '\n';
        }
    }
}

}  // namespace masodik::tui::chat_history
