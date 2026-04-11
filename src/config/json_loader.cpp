#include "masodik/config/json_loader.hpp"

#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>

namespace masodik::config {

JsonValue::JsonValue(std::nullptr_t value) : storage_(value) {}
JsonValue::JsonValue(const bool value) : storage_(value) {}
JsonValue::JsonValue(const double value) : storage_(value) {}
JsonValue::JsonValue(std::string value) : storage_(std::move(value)) {}
JsonValue::JsonValue(Array value) : storage_(std::move(value)) {}
JsonValue::JsonValue(Object value) : storage_(std::move(value)) {}

bool JsonValue::is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(storage_); }
bool JsonValue::is_bool() const noexcept { return std::holds_alternative<bool>(storage_); }
bool JsonValue::is_number() const noexcept { return std::holds_alternative<double>(storage_); }
bool JsonValue::is_string() const noexcept { return std::holds_alternative<std::string>(storage_); }
bool JsonValue::is_array() const noexcept { return std::holds_alternative<Array>(storage_); }
bool JsonValue::is_object() const noexcept { return std::holds_alternative<Object>(storage_); }

bool JsonValue::as_bool() const { return std::get<bool>(storage_); }
double JsonValue::as_number() const { return std::get<double>(storage_); }
const std::string& JsonValue::as_string() const { return std::get<std::string>(storage_); }
const JsonValue::Array& JsonValue::as_array() const { return std::get<Array>(storage_); }
const JsonValue::Object& JsonValue::as_object() const { return std::get<Object>(storage_); }

namespace {

class Parser {
public:
    explicit Parser(const std::string_view document) : document_(document) {}

    JsonValue parse() {
        skip_whitespace();
        auto value = parse_value();
        skip_whitespace();
        if (position_ != document_.size()) {
            throw std::runtime_error("A JSON dokumentum vegen maradt feldolgozatlan tartalom.");
        }
        return value;
    }

private:
    JsonValue parse_value() {
        if (position_ >= document_.size()) {
            throw std::runtime_error("Varatlan JSON EOF.");
        }

        const char token = document_[position_];
        if (token == '{') {
            return parse_object();
        }
        if (token == '[') {
            return parse_array();
        }
        if (token == '"') {
            return JsonValue{parse_string()};
        }
        if (token == 't') {
            consume_literal("true");
            return JsonValue{true};
        }
        if (token == 'f') {
            consume_literal("false");
            return JsonValue{false};
        }
        if (token == 'n') {
            consume_literal("null");
            return JsonValue{nullptr};
        }
        if (token == '-' || std::isdigit(static_cast<unsigned char>(token)) != 0) {
            return JsonValue{parse_number()};
        }

        throw std::runtime_error("Ismeretlen JSON token.");
    }

    JsonValue parse_object() {
        JsonValue::Object object;
        expect('{');
        skip_whitespace();
        if (peek('}')) {
            expect('}');
            return JsonValue{std::move(object)};
        }

        while (true) {
            skip_whitespace();
            auto key = parse_string();
            skip_whitespace();
            expect(':');
            skip_whitespace();
            object.emplace(std::move(key), parse_value());
            skip_whitespace();
            if (peek('}')) {
                expect('}');
                break;
            }
            expect(',');
        }

        return JsonValue{std::move(object)};
    }

    JsonValue parse_array() {
        JsonValue::Array array;
        expect('[');
        skip_whitespace();
        if (peek(']')) {
            expect(']');
            return JsonValue{std::move(array)};
        }

        while (true) {
            skip_whitespace();
            array.emplace_back(parse_value());
            skip_whitespace();
            if (peek(']')) {
                expect(']');
                break;
            }
            expect(',');
        }

        return JsonValue{std::move(array)};
    }

    std::string parse_string() {
        expect('"');
        std::string result;
        while (position_ < document_.size()) {
            const char character = document_[position_++];
            if (character == '"') {
                return result;
            }
            if (character == '\\') {
                if (position_ >= document_.size()) {
                    throw std::runtime_error("Hibas escape a JSON stringben.");
                }
                const char escape = document_[position_++];
                switch (escape) {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escape);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case 'u': {
                    // \uXXXX unicode escape → UTF-8 konverzio
                    if (position_ + 4 > document_.size()) {
                        throw std::runtime_error("Hianyos \\uXXXX escape a JSON stringben.");
                    }
                    const auto hex_str = document_.substr(position_, 4);
                    position_ += 4;
                    unsigned long codepoint = 0;
                    try {
                        codepoint = std::stoul(std::string{hex_str}, nullptr, 16);
                    } catch (...) {
                        throw std::runtime_error("Ervenytelen \\uXXXX ertek a JSON stringben.");
                    }
                    // UTF-16 surrogate pair kezeles (\uD800-\uDBFF + \uDC00-\uDFFF)
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                        if (position_ + 6 <= document_.size() &&
                            document_[position_] == '\\' && document_[position_ + 1] == 'u') {
                            position_ += 2;
                            const auto low_hex = document_.substr(position_, 4);
                            position_ += 4;
                            unsigned long low_surrogate = 0;
                            try {
                                low_surrogate = std::stoul(std::string{low_hex}, nullptr, 16);
                            } catch (...) {
                                throw std::runtime_error("Ervenytelen surrogate pair a JSON stringben.");
                            }
                            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low_surrogate - 0xDC00);
                        }
                    }
                    // Codepoint → UTF-8
                    if (codepoint <= 0x7F) {
                        result.push_back(static_cast<char>(codepoint));
                    } else if (codepoint <= 0x7FF) {
                        result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                    } else if (codepoint <= 0xFFFF) {
                        result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                    } else if (codepoint <= 0x10FFFF) {
                        result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                        result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                    }
                    break;
                }
                default:
                    throw std::runtime_error("Ismeretlen escape a JSON stringben.");
                }
                continue;
            }
            result.push_back(character);
        }
        throw std::runtime_error("Lezaratlan JSON string.");
    }

    double parse_number() {
        const std::size_t start = position_;
        if (document_[position_] == '-') {
            ++position_;
        }
        while (position_ < document_.size() && std::isdigit(static_cast<unsigned char>(document_[position_])) != 0) {
            ++position_;
        }
        if (position_ < document_.size() && document_[position_] == '.') {
            ++position_;
            while (position_ < document_.size() &&
                   std::isdigit(static_cast<unsigned char>(document_[position_])) != 0) {
                ++position_;
            }
        }
        if (position_ < document_.size() && (document_[position_] == 'e' || document_[position_] == 'E')) {
            ++position_;
            if (position_ < document_.size() && (document_[position_] == '+' || document_[position_] == '-')) {
                ++position_;
            }
            while (position_ < document_.size() &&
                   std::isdigit(static_cast<unsigned char>(document_[position_])) != 0) {
                ++position_;
            }
        }

        const auto token = std::string{document_.substr(start, position_ - start)};
        double value = 0.0;
        const auto [_, error] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (error != std::errc{}) {
            throw std::runtime_error("Ervenytelen JSON szam.");
        }
        return value;
    }

    void consume_literal(const std::string_view literal) {
        if (document_.substr(position_, literal.size()) != literal) {
            throw std::runtime_error("Ervenytelen JSON literal.");
        }
        position_ += literal.size();
    }

    void skip_whitespace() {
        while (position_ < document_.size() &&
               std::isspace(static_cast<unsigned char>(document_[position_])) != 0) {
            ++position_;
        }
    }

    void expect(const char expected) {
        if (position_ >= document_.size() || document_[position_] != expected) {
            throw std::runtime_error("Hibas JSON szintaxis.");
        }
        ++position_;
    }

    bool peek(const char expected) const {
        return position_ < document_.size() && document_[position_] == expected;
    }

    std::string_view document_;
    std::size_t position_ = 0;
};

}  // namespace

JsonValue JsonLoader::parse(const std::string_view document) { return Parser{document}.parse(); }

JsonValue JsonLoader::load_file(const std::filesystem::path& file_path) {
    std::ifstream stream{file_path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error("A JSON fajl nem nyithato meg: " + file_path.string());
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return parse(buffer.str());
}

const JsonValue* find_member(const JsonValue::Object& object, const std::string_view key) {
    const auto iterator = object.find(key);
    if (iterator == object.end()) {
        return nullptr;
    }
    return &iterator->second;
}

const JsonValue& require_member(const JsonValue::Object& object, const std::string_view key) {
    const auto* value = find_member(object, key);
    if (value == nullptr) {
        throw std::runtime_error("Hianyzik a kotelezo JSON mezo: " + std::string{key});
    }
    return *value;
}

std::string get_string(const JsonValue::Object& object, const std::string_view key) {
    const auto& value = require_member(object, key);
    if (!value.is_string()) {
        throw std::runtime_error("A JSON mezo nem string: " + std::string{key});
    }
    return value.as_string();
}

std::string get_string_or(const JsonValue::Object& object, const std::string_view key, std::string fallback) {
    const auto* value = find_member(object, key);
    if (value == nullptr) {
        return fallback;
    }
    if (!value->is_string()) {
        throw std::runtime_error("A JSON mezo nem string: " + std::string{key});
    }
    return value->as_string();
}

double get_number(const JsonValue::Object& object, const std::string_view key) {
    const auto& value = require_member(object, key);
    if (!value.is_number()) {
        throw std::runtime_error("A JSON mezo nem szam: " + std::string{key});
    }
    return value.as_number();
}

double get_number_or(const JsonValue::Object& object, const std::string_view key, const double fallback) {
    const auto* value = find_member(object, key);
    if (value == nullptr) {
        return fallback;
    }
    if (!value->is_number()) {
        throw std::runtime_error("A JSON mezo nem szam: " + std::string{key});
    }
    return value->as_number();
}

bool get_bool(const JsonValue::Object& object, const std::string_view key) {
    const auto& value = require_member(object, key);
    if (!value.is_bool()) {
        throw std::runtime_error("A JSON mezo nem bool: " + std::string{key});
    }
    return value.as_bool();
}

bool get_bool_or(const JsonValue::Object& object, const std::string_view key, const bool fallback) {
    const auto* value = find_member(object, key);
    if (value == nullptr) {
        return fallback;
    }
    if (!value->is_bool()) {
        throw std::runtime_error("A JSON mezo nem bool: " + std::string{key});
    }
    return value->as_bool();
}

const JsonValue::Object& get_object(const JsonValue::Object& object, const std::string_view key) {
    const auto& value = require_member(object, key);
    if (!value.is_object()) {
        throw std::runtime_error("A JSON mezo nem objektum: " + std::string{key});
    }
    return value.as_object();
}

const JsonValue::Object* get_object_if_present(const JsonValue::Object& object, const std::string_view key) {
    const auto* value = find_member(object, key);
    if (value == nullptr) {
        return nullptr;
    }
    if (!value->is_object()) {
        throw std::runtime_error("A JSON mezo nem objektum: " + std::string{key});
    }
    return &value->as_object();
}

}  // namespace masodik::config
