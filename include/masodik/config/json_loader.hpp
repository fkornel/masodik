#pragma once

#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace masodik::config {

class JsonValue {
public:
    using Object = std::map<std::string, JsonValue, std::less<>>;
    using Array = std::vector<JsonValue>;
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    JsonValue() = default;
    JsonValue(std::nullptr_t value);
    JsonValue(bool value);
    JsonValue(double value);
    JsonValue(std::string value);
    JsonValue(Array value);
    JsonValue(Object value);

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_bool() const noexcept;
    [[nodiscard]] bool is_number() const noexcept;
    [[nodiscard]] bool is_string() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;

    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] double as_number() const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] const Object& as_object() const;

private:
    Storage storage_{nullptr};
};

class JsonLoader {
public:
    static JsonValue parse(std::string_view document);
    static JsonValue load_file(const std::filesystem::path& file_path);
};

const JsonValue* find_member(const JsonValue::Object& object, std::string_view key);
const JsonValue& require_member(const JsonValue::Object& object, std::string_view key);
std::string get_string(const JsonValue::Object& object, std::string_view key);
std::string get_string_or(const JsonValue::Object& object, std::string_view key, std::string fallback);
double get_number(const JsonValue::Object& object, std::string_view key);
double get_number_or(const JsonValue::Object& object, std::string_view key, double fallback);
bool get_bool(const JsonValue::Object& object, std::string_view key);
bool get_bool_or(const JsonValue::Object& object, std::string_view key, bool fallback);
const JsonValue::Object& get_object(const JsonValue::Object& object, std::string_view key);
const JsonValue::Object* get_object_if_present(const JsonValue::Object& object, std::string_view key);

}  // namespace masodik::config
