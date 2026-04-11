#include "masodik/api/openai_contract.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

#include "masodik/config/json_loader.hpp"

namespace masodik::api {
namespace {

std::string escape_json(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

std::string serialize_json_value(const masodik::config::JsonValue& value) {
    if (value.is_null()) {
        return "null";
    }
    if (value.is_bool()) {
        return value.as_bool() ? "true" : "false";
    }
    if (value.is_number()) {
        std::ostringstream stream;
        stream << value.as_number();
        return stream.str();
    }
    if (value.is_string()) {
        return "\"" + escape_json(value.as_string()) + "\"";
    }
    if (value.is_array()) {
        std::ostringstream stream;
        stream << '[';
        const auto& array = value.as_array();
        for (std::size_t index = 0; index < array.size(); ++index) {
            if (index > 0) {
                stream << ',';
            }
            stream << serialize_json_value(array[index]);
        }
        stream << ']';
        return stream.str();
    }

    std::ostringstream stream;
    stream << '{';
    bool emitted_member = false;
    for (const auto& [key, member_value] : value.as_object()) {
        if (emitted_member) {
            stream << ',';
        }
        stream << "\"" << escape_json(key) << "\":" << serialize_json_value(member_value);
        emitted_member = true;
    }
    stream << '}';
    return stream.str();
}

void append_tool_calls_json(std::ostringstream& stream, const std::vector<ToolCall>& tool_calls) {
    stream << '[';
    for (std::size_t index = 0; index < tool_calls.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }

        const auto& tool_call = tool_calls[index];
        stream << "{\"id\":\"" << escape_json(tool_call.id) << "\",\"type\":\"" << escape_json(tool_call.type)
               << "\",\"function\":{\"name\":\"" << escape_json(tool_call.function.name)
               << "\",\"arguments\":\"" << escape_json(tool_call.function.arguments) << "\"}}";
    }
    stream << ']';
}

void append_tool_call_delta_json(std::ostringstream& stream, const std::vector<ToolCallChunkDelta>& tool_calls) {
    stream << '[';
    for (std::size_t index = 0; index < tool_calls.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }

        const auto& tool_call = tool_calls[index];
        stream << "{\"index\":" << tool_call.index;
        if (!tool_call.id.empty()) {
            stream << ",\"id\":\"" << escape_json(tool_call.id) << "\"";
        }
        if (!tool_call.type.empty()) {
            stream << ",\"type\":\"" << escape_json(tool_call.type) << "\"";
        }
        if (!tool_call.function.name.empty() || !tool_call.function.arguments.empty()) {
            stream << ",\"function\":{";
            bool emitted_member = false;
            if (!tool_call.function.name.empty()) {
                stream << "\"name\":\"" << escape_json(tool_call.function.name) << "\"";
                emitted_member = true;
            }
            if (!tool_call.function.arguments.empty()) {
                if (emitted_member) {
                    stream << ',';
                }
                stream << "\"arguments\":\"" << escape_json(tool_call.function.arguments) << "\"";
            }
            stream << '}';
        }
        stream << '}';
    }
    stream << ']';
}

std::string parse_message_content(const std::string_view role, const masodik::config::JsonValue* content_value) {
    if (content_value == nullptr || content_value->is_null()) {
        if (role == "assistant") {
            return {};
        }

        throw std::invalid_argument(
            "A messages[].content mezo csak string vagy text-part tomb lehet.");
    }

    if (content_value->is_string()) {
        return content_value->as_string();
    }

    if (!content_value->is_array()) {
        throw std::invalid_argument(
            "A messages[].content mezo csak string vagy text-part tomb lehet.");
    }

    std::string flattened_content;
    for (const auto& part : content_value->as_array()) {
        if (!part.is_object()) {
            throw std::invalid_argument(
                "A messages[].content tomb elemei csak objektumok lehetnek.");
        }

        const auto& part_object = part.as_object();
        const auto type = masodik::config::get_string(part_object, "type");
        if (type != "text") {
            throw std::invalid_argument(
                "A messages[].content csak type=text elemeket tamogat.");
        }

        if (!flattened_content.empty()) {
            flattened_content += '\n';
        }
        flattened_content += masodik::config::get_string(part_object, "text");
    }

    if (flattened_content.empty()) {
        throw std::invalid_argument(
            "A messages[].content text-part tomb nem lehet ures.");
    }

    return flattened_content;
}

std::vector<ToolCall> parse_message_tool_calls(const masodik::config::JsonValue::Object& message_object) {
    std::vector<ToolCall> tool_calls;
    const auto* tool_calls_value = masodik::config::find_member(message_object, "tool_calls");
    if (tool_calls_value == nullptr || tool_calls_value->is_null()) {
        return tool_calls;
    }
    if (!tool_calls_value->is_array()) {
        throw std::invalid_argument("A messages[].tool_calls mezo csak tomb lehet.");
    }

    for (const auto& tool_call_value : tool_calls_value->as_array()) {
        if (!tool_call_value.is_object()) {
            throw std::invalid_argument("A messages[].tool_calls elemei csak objektumok lehetnek.");
        }

        const auto& tool_call_object = tool_call_value.as_object();
        ToolCall tool_call;
        tool_call.id = masodik::config::get_string_or(tool_call_object, "id", {});
        tool_call.type = masodik::config::get_string_or(tool_call_object, "type", "function");

        const auto& function_object = masodik::config::get_object(tool_call_object, "function");
        tool_call.function.name = masodik::config::get_string(function_object, "name");
        if (const auto* arguments_value = masodik::config::find_member(function_object, "arguments");
            arguments_value != nullptr && !arguments_value->is_null()) {
            if (arguments_value->is_string()) {
                tool_call.function.arguments = arguments_value->as_string();
            } else {
                tool_call.function.arguments = serialize_json_value(*arguments_value);
            }
        }

        tool_calls.push_back(std::move(tool_call));
    }

    return tool_calls;
}

ToolDefinition parse_tool_definition(const masodik::config::JsonValue& tool_value) {
    if (!tool_value.is_object()) {
        throw std::invalid_argument("A tools tomb elemei csak objektumok lehetnek.");
    }

    const auto& tool_object = tool_value.as_object();
    ToolDefinition tool_definition;
    tool_definition.type = masodik::config::get_string(tool_object, "type");
    if (tool_definition.type != "function") {
        throw std::invalid_argument("A tools elemeknel csak type=function tamogatott.");
    }

    const auto& function_object = masodik::config::get_object(tool_object, "function");
    tool_definition.function.name = masodik::config::get_string(function_object, "name");
    tool_definition.function.description = masodik::config::get_string_or(function_object, "description", {});

    const auto& parameters_value = masodik::config::require_member(function_object, "parameters");
    if (!parameters_value.is_object()) {
        throw std::invalid_argument("A tools[].function.parameters csak JSON objektum lehet.");
    }

    const auto& parameters_object = parameters_value.as_object();
    const auto schema_type = masodik::config::get_string(parameters_object, "type");
    if (schema_type != "object") {
        throw std::invalid_argument("A tools[].function.parameters root schemaja csak type=object lehet.");
    }

    tool_definition.function.parameters_json = serialize_json_value(parameters_value);
    return tool_definition;
}

ToolChoice parse_tool_choice(
    const masodik::config::JsonValue::Object& object,
    const std::vector<ToolDefinition>& tools) {
    const auto* tool_choice_value = masodik::config::find_member(object, "tool_choice");
    if (tool_choice_value == nullptr) {
        return {.kind = tools.empty() ? ToolChoiceKind::none : ToolChoiceKind::auto_select};
    }

    if (tool_choice_value->is_string()) {
        const auto tool_choice = tool_choice_value->as_string();
        if (tool_choice == "none") {
            return {.kind = ToolChoiceKind::none};
        }
        if (tool_choice == "auto") {
            return {.kind = ToolChoiceKind::auto_select};
        }
        if (tool_choice == "required") {
            return {.kind = ToolChoiceKind::required};
        }
        throw std::invalid_argument("A tool_choice string erteke csak none, auto vagy required lehet.");
    }

    if (!tool_choice_value->is_object()) {
        throw std::invalid_argument("A tool_choice mezo csak string vagy objektum lehet.");
    }

    const auto& tool_choice_object = tool_choice_value->as_object();
    if (masodik::config::get_string(tool_choice_object, "type") != "function") {
        throw std::invalid_argument("Az objektum alapu tool_choice csak type=function lehet.");
    }

    const auto& function_object = masodik::config::get_object(tool_choice_object, "function");
    return {.kind = ToolChoiceKind::named_function, .function_name = masodik::config::get_string(function_object, "name")};
}

ToolChoiceSummary summarize_tool_choice_from_object(
    const masodik::config::JsonValue::Object& object,
    const bool tools_present) {
    ToolChoiceSummary summary;
    const auto* tool_choice_value = masodik::config::find_member(object, "tool_choice");
    if (tool_choice_value == nullptr) {
        summary.kind = tools_present ? ToolChoiceSummaryKind::auto_select : ToolChoiceSummaryKind::absent;
        return summary;
    }

    if (tool_choice_value->is_string()) {
        const auto tool_choice = tool_choice_value->as_string();
        if (tool_choice == "none") {
            summary.kind = ToolChoiceSummaryKind::none;
        } else if (tool_choice == "auto") {
            summary.kind = ToolChoiceSummaryKind::auto_select;
        } else if (tool_choice == "required") {
            summary.kind = ToolChoiceSummaryKind::required;
        } else {
            summary.kind = ToolChoiceSummaryKind::absent;
        }
        return summary;
    }

    if (!tool_choice_value->is_object()) {
        summary.kind = ToolChoiceSummaryKind::absent;
        return summary;
    }

    summary.kind = ToolChoiceSummaryKind::named_function;
    const auto& tool_choice_object = tool_choice_value->as_object();
    if (const auto* function_value = masodik::config::find_member(tool_choice_object, "function");
        function_value != nullptr && function_value->is_object()) {
        summary.function_name = masodik::config::get_string_or(function_value->as_object(), "name", {});
    }
    return summary;
}

std::string extract_preview_from_messages_value(const masodik::config::JsonValue& messages_value) {
    if (!messages_value.is_array()) {
        return {};
    }

    const auto& messages = messages_value.as_array();
    for (auto index = messages.rbegin(); index != messages.rend(); ++index) {
        if (!index->is_object()) {
            continue;
        }

        const auto& message_object = index->as_object();
        if (masodik::config::get_string_or(message_object, "role", {}) != "user") {
            continue;
        }

        if (const auto* content_value = masodik::config::find_member(message_object, "content"); content_value != nullptr) {
            if (content_value->is_string()) {
                return content_value->as_string();
            }

            if (content_value->is_array()) {
                std::string flattened_content;
                for (const auto& part : content_value->as_array()) {
                    if (!part.is_object()) {
                        continue;
                    }
                    const auto& part_object = part.as_object();
                    if (masodik::config::get_string_or(part_object, "type", {}) != "text") {
                        continue;
                    }
                    const auto part_text = masodik::config::get_string_or(part_object, "text", {});
                    if (part_text.empty()) {
                        continue;
                    }
                    if (!flattened_content.empty()) {
                        flattened_content += '\n';
                    }
                    flattened_content += part_text;
                }
                return flattened_content;
            }
        }
    }

    return {};
}

std::string build_request_audit_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
    localtime_s(&local_time, &time);

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

void append_sse_data_lines(std::ostringstream& stream, const std::string_view payload) {
    std::size_t start = 0;
    while (start <= payload.size()) {
        const auto end = payload.find('\n', start);
        const auto line_end = end == std::string_view::npos ? payload.size() : end;
        stream << "data: " << payload.substr(start, line_end - start) << "\n";
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
}

}  // namespace

ChatCompletionRequest parse_chat_completion_request(const std::string_view document) {
    const auto root = masodik::config::JsonLoader::parse(document);
    if (!root.is_object()) {
        throw std::invalid_argument("A chat completion keres JSON objektum kell legyen.");
    }

    const auto& object = root.as_object();
    ChatCompletionRequest request;
    request.model = masodik::config::get_string(object, "model");
    request.max_tokens = static_cast<int>(masodik::config::get_number_or(object, "max_tokens", request.max_tokens));
    request.stream = masodik::config::get_bool_or(object, "stream", request.stream);

    const auto& messages_value = masodik::config::require_member(object, "messages");
    if (!messages_value.is_array()) {
        throw std::invalid_argument("A messages mezo csak tomb lehet.");
    }

    for (const auto& item : messages_value.as_array()) {
        if (!item.is_object()) {
            throw std::invalid_argument("A messages elemei csak objektumok lehetnek.");
        }
        const auto& message_object = item.as_object();
        const auto role = masodik::config::get_string(message_object, "role");
        request.messages.push_back(
            {.role = role,
             .content = parse_message_content(role, masodik::config::find_member(message_object, "content")),
             .tool_calls = parse_message_tool_calls(message_object)});
    }

    if (request.messages.empty()) {
        throw std::invalid_argument("A messages tomb nem lehet ures.");
    }

    if (const auto* tools_value = masodik::config::find_member(object, "tools"); tools_value != nullptr) {
        if (!tools_value->is_array()) {
            throw std::invalid_argument("A tools mezo csak tomb lehet.");
        }

        for (const auto& tool_value : tools_value->as_array()) {
            request.tools.push_back(parse_tool_definition(tool_value));
        }
    }

    validate_tool_definitions(request.tools);
    request.tool_choice = parse_tool_choice(object, request.tools);
    if (tool_choice_requires_tool(request.tool_choice) && request.tools.empty()) {
        throw std::invalid_argument("A tool_choice csak akkor ervenyes, ha a keres tools tombot is tartalmaz.");
    }
    if (request.tool_choice.kind == ToolChoiceKind::named_function) {
        const auto matching_tool = std::find_if(
            request.tools.begin(),
            request.tools.end(),
            [&](const ToolDefinition& tool) { return tool.function.name == request.tool_choice.function_name; });
        if (matching_tool == request.tools.end()) {
            throw std::invalid_argument("A tool_choice.function.name nem egyezik egyetlen meghirdetett tool nevével sem.");
        }
    }

    return request;
}

void validate_tool_definitions(const std::vector<ToolDefinition>& tools) {
    std::set<std::string, std::less<>> names;
    for (const auto& tool : tools) {
        if (tool.type != "function") {
            throw std::invalid_argument("A tools elemeknel csak type=function tamogatott.");
        }
        if (tool.function.name.empty()) {
            throw std::invalid_argument("A tools[].function.name nem lehet ures.");
        }
        if (tool.function.parameters_json.empty()) {
            throw std::invalid_argument("A tools[].function.parameters schema nem lehet ures.");
        }
        if (!names.insert(tool.function.name).second) {
            throw std::invalid_argument("A tools tomb nem tartalmazhat ket azonos nevu function toolt.");
        }
    }
}

bool has_tool_definitions(const ChatCompletionRequest& request) noexcept { return !request.tools.empty(); }

bool tool_choice_requires_tool(const ToolChoice& tool_choice) noexcept {
    return tool_choice.kind == ToolChoiceKind::auto_select || tool_choice.kind == ToolChoiceKind::required ||
           tool_choice.kind == ToolChoiceKind::named_function;
}

ToolChoiceSummary summarize_tool_choice(const ChatCompletionRequest& request) noexcept {
    ToolChoiceSummary summary;
    summary.function_name = request.tool_choice.function_name;

    switch (request.tool_choice.kind) {
    case ToolChoiceKind::none:
        summary.kind = request.tools.empty() ? ToolChoiceSummaryKind::absent : ToolChoiceSummaryKind::none;
        break;
    case ToolChoiceKind::auto_select:
        summary.kind = ToolChoiceSummaryKind::auto_select;
        break;
    case ToolChoiceKind::required:
        summary.kind = ToolChoiceSummaryKind::required;
        break;
    case ToolChoiceKind::named_function:
        summary.kind = ToolChoiceSummaryKind::named_function;
        break;
    }

    return summary;
}

std::string truncate_request_audit_preview(const std::string_view content) {
    constexpr std::size_t max_preview_size = 160;
    if (content.size() <= max_preview_size) {
        return std::string{content};
    }

    return std::string{content.substr(0, max_preview_size)} + "...";
}

RequestDecisionPath infer_request_decision_path(const ToolChoiceSummary& tool_choice, const bool tools_present) noexcept {
    if (!tools_present) {
        return RequestDecisionPath::content_only;
    }

    switch (tool_choice.kind) {
    case ToolChoiceSummaryKind::required:
        return RequestDecisionPath::tool_call_required;
    case ToolChoiceSummaryKind::named_function:
        return RequestDecisionPath::tool_call_named;
    case ToolChoiceSummaryKind::auto_select:
        return RequestDecisionPath::tool_call_auto;
    case ToolChoiceSummaryKind::absent:
    case ToolChoiceSummaryKind::none:
        return RequestDecisionPath::content_only;
    }

    return RequestDecisionPath::unknown;
}

RequestAuditRecord derive_request_audit_record(
    const ChatCompletionRequest& request,
    std::string request_id,
    const RequestDecisionPath decision_path,
    const RequestValidationOutcome validation_outcome) {
    RequestAuditRecord record;
    record.request_id = std::move(request_id);
    record.stream = request.stream;
    record.tools_present = has_tool_definitions(request);
    record.tool_count = request.tools.size();
    record.tool_choice = summarize_tool_choice(request);
    record.decision_path = decision_path;
    record.validation_outcome = validation_outcome;
    record.created_at = build_request_audit_timestamp();

    std::string last_user_message;
    for (auto index = request.messages.rbegin(); index != request.messages.rend(); ++index) {
        if (index->role == "user") {
            last_user_message = index->content;
            break;
        }
    }
    if (last_user_message.empty() && !request.messages.empty()) {
        last_user_message = request.messages.back().content;
    }
    record.truncated_message_preview = truncate_request_audit_preview(last_user_message);
    return record;
}

RequestAuditRecord derive_request_audit_record_from_payload(
    const std::string_view document,
    std::string request_id,
    const RequestDecisionPath decision_path,
    const RequestValidationOutcome validation_outcome) {
    RequestAuditRecord record;
    record.request_id = std::move(request_id);
    record.decision_path = decision_path;
    record.validation_outcome = validation_outcome;
    record.created_at = build_request_audit_timestamp();

    try {
        const auto root = masodik::config::JsonLoader::parse(document);
        if (!root.is_object()) {
            return record;
        }

        const auto& object = root.as_object();
        record.stream = masodik::config::get_bool_or(object, "stream", false);
        if (const auto* tools_value = masodik::config::find_member(object, "tools");
            tools_value != nullptr && tools_value->is_array()) {
            record.tools_present = true;
            record.tool_count = tools_value->as_array().size();
        }

        record.tool_choice = summarize_tool_choice_from_object(object, record.tools_present);
        if (record.decision_path == RequestDecisionPath::unknown) {
            record.decision_path = infer_request_decision_path(record.tool_choice, record.tools_present);
        }
        if (const auto* messages_value = masodik::config::find_member(object, "messages"); messages_value != nullptr) {
            record.truncated_message_preview = truncate_request_audit_preview(extract_preview_from_messages_value(*messages_value));
        }
    } catch (const std::exception&) {
    }

    return record;
}

std::string_view to_string(const ToolChoiceSummaryKind kind) noexcept {
    switch (kind) {
    case ToolChoiceSummaryKind::absent:
        return "absent";
    case ToolChoiceSummaryKind::none:
        return "none";
    case ToolChoiceSummaryKind::auto_select:
        return "auto";
    case ToolChoiceSummaryKind::required:
        return "required";
    case ToolChoiceSummaryKind::named_function:
        return "named_function";
    }

    return "absent";
}

std::string_view to_string(const RequestDecisionPath decision_path) noexcept {
    switch (decision_path) {
    case RequestDecisionPath::unknown:
        return "unknown";
    case RequestDecisionPath::content_only:
        return "content_only";
    case RequestDecisionPath::tool_call_auto:
        return "tool_call_auto";
    case RequestDecisionPath::tool_call_required:
        return "tool_call_required";
    case RequestDecisionPath::tool_call_named:
        return "tool_call_named";
    }

    return "unknown";
}

std::string_view to_string(const RequestValidationOutcome validation_outcome) noexcept {
    switch (validation_outcome) {
    case RequestValidationOutcome::not_evaluated:
        return "not_evaluated";
    case RequestValidationOutcome::accepted:
        return "accepted";
    case RequestValidationOutcome::parser_error:
        return "parser_error";
    case RequestValidationOutcome::tool_schema_error:
        return "tool_schema_error";
    case RequestValidationOutcome::model_selection_error:
        return "model_selection_error";
    case RequestValidationOutcome::runtime_readiness_error:
        return "runtime_readiness_error";
    case RequestValidationOutcome::runtime_error:
        return "runtime_error";
    }

    return "not_evaluated";
}

std::string to_json(const ChatCompletionResponse& response) {
    std::ostringstream stream;
    stream << "{\"id\":\"" << escape_json(response.id) << "\",\"object\":\"chat.completion\",\"model\":\""
           << escape_json(response.model) << "\",\"choices\":[";
    for (std::size_t index = 0; index < response.choices.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        const auto& choice = response.choices[index];
        stream << "{\"index\":" << choice.index << ",\"message\":{\"role\":\"" << escape_json(choice.message.role)
               << "\",\"content\":";
        if (!choice.message.tool_calls.empty() && choice.message.content.empty()) {
            stream << "null";
        } else {
            stream << "\"" << escape_json(choice.message.content) << "\"";
        }
        if (!choice.message.tool_calls.empty()) {
            stream << ",\"tool_calls\":";
            append_tool_calls_json(stream, choice.message.tool_calls);
        }
        stream << "},\"finish_reason\":";
        if (choice.finish_reason.empty()) {
            stream << "null";
        } else {
            stream << "\"" << escape_json(choice.finish_reason) << "\"";
        }
        stream << '}';
    }
    stream << "]}";
    return stream.str();
}

std::string to_json(const ChatCompletionChunkResponse& response) {
    std::ostringstream stream;
    stream << "{\"id\":\"" << escape_json(response.id) << "\",\"object\":\"" << escape_json(response.object)
           << "\",\"model\":\"" << escape_json(response.model) << "\",\"choices\":[";
    for (std::size_t index = 0; index < response.choices.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        const auto& choice = response.choices[index];
        stream << "{\"index\":" << choice.index << ",\"delta\":{";

        bool emitted_field = false;
        if (!choice.delta.role.empty()) {
            stream << "\"role\":\"" << escape_json(choice.delta.role) << "\"";
            emitted_field = true;
        }
        if (!choice.delta.content.empty()) {
            if (emitted_field) {
                stream << ',';
            }
            stream << "\"content\":\"" << escape_json(choice.delta.content) << "\"";
            emitted_field = true;
        }
        if (!choice.delta.tool_calls.empty()) {
            if (emitted_field) {
                stream << ',';
            }
            stream << "\"tool_calls\":";
            append_tool_call_delta_json(stream, choice.delta.tool_calls);
        }

        stream << "},\"finish_reason\":";
        if (choice.finish_reason.empty()) {
            stream << "null";
        } else {
            stream << "\"" << escape_json(choice.finish_reason) << "\"";
        }
        stream << "}";
    }
    stream << "]}";
    return stream.str();
}

std::string to_json(const RequestAuditRecord& record) {
    std::ostringstream stream;
    stream << "{\"request_id\":\"" << escape_json(record.request_id) << "\""
           << ",\"stream\":" << (record.stream ? "true" : "false")
           << ",\"tools_present\":" << (record.tools_present ? "true" : "false")
           << ",\"tool_count\":" << record.tool_count
           << ",\"tool_choice_kind\":\"" << escape_json(to_string(record.tool_choice.kind)) << "\""
           << ",\"tool_choice_name\":";
    if (record.tool_choice.function_name.empty()) {
        stream << "null";
    } else {
        stream << "\"" << escape_json(record.tool_choice.function_name) << "\"";
    }
    stream << ",\"decision_path\":\"" << escape_json(to_string(record.decision_path)) << "\""
           << ",\"validation_outcome\":\"" << escape_json(to_string(record.validation_outcome)) << "\""
           << ",\"truncated_message_preview\":";
    if (record.truncated_message_preview.empty()) {
        stream << "null";
    } else {
        stream << "\"" << escape_json(record.truncated_message_preview) << "\"";
    }
    stream << ",\"created_at\":";
    if (record.created_at.empty()) {
        stream << "null";
    } else {
        stream << "\"" << escape_json(record.created_at) << "\"";
    }
    stream << "}";
    return stream.str();
}

ChatCompletionChunkResponse make_chat_completion_chunk_response(
    std::string id,
    std::string model,
    const int index,
    ChatCompletionChunkDelta delta,
    std::string finish_reason) {
    ChatCompletionChunkResponse response;
    response.id = std::move(id);
    response.model = std::move(model);
    response.choices.push_back(
        {.index = index, .delta = std::move(delta), .finish_reason = std::move(finish_reason)});
    return response;
}

ChatCompletionChunkResponse make_chat_completion_chunk_response(
    std::string id,
    std::string model,
    const int index,
    std::string role,
    std::string content,
    std::string finish_reason) {
    return make_chat_completion_chunk_response(
        std::move(id),
        std::move(model),
        index,
        {.role = std::move(role), .content = std::move(content)},
        std::move(finish_reason));
}

ChatCompletionChunkResponse make_terminal_chat_completion_chunk_response(
    std::string id,
    std::string model,
    const int index,
    std::string finish_reason,
    std::vector<ToolCallChunkDelta> tool_calls) {
    ChatCompletionChunkResponse response;
    response.id = std::move(id);
    response.model = std::move(model);
    response.choices.push_back(
        {.index = index, .delta = {.tool_calls = std::move(tool_calls)}, .finish_reason = std::move(finish_reason)});
    return response;
}

std::string to_json(const std::vector<ModelCard>& models) {
    std::ostringstream stream;
    stream << "{\"object\":\"list\",\"data\":[";
    for (std::size_t index = 0; index < models.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        const auto& model = models[index];
        stream << "{\"id\":\"" << escape_json(model.id) << "\",\"object\":\"" << escape_json(model.object)
               << "\",\"owned_by\":\"" << escape_json(model.owned_by) << "\"";
        if (model.split_mode) {
            stream << ",\"split_mode\":true,\"split_profile\":\"" << escape_json(model.split_profile) << "\"";
        }
        if (!model.type.empty()) {
            stream << ",\"type\":\"" << escape_json(model.type) << "\"";
        }
        stream << '}';
    }
    stream << "]}";
    return stream.str();
}

std::string to_json(const HealthResponse& response) {
    std::ostringstream stream;
    stream << "{\"ready\":" << (response.ready ? "true" : "false") << ",\"state\":\"" << escape_json(response.state)
           << "\",\"active_model\":\"" << escape_json(response.active_model) << "\",\"active_profile\":\""
           << escape_json(response.active_profile) << "\",\"effective_device\":\""
           << escape_json(response.effective_device) << "\",\"queue_depth\":" << response.queue_depth
           << ",\"active_streams\":" << response.active_streams
           << ",\"peak_active_streams\":" << response.peak_active_streams
           << ",\"total_stream_failures\":" << response.total_stream_failures
           << ",\"last_disconnect_error\":" << response.last_disconnect_error
           << ",\"gpu_offload_active\":" << (response.gpu_offload_active ? "true" : "false")
           << ",\"status_message\":\"" << escape_json(response.status_message) << "\""
           << ",\"split_mode\":" << (response.split_mode ? "true" : "false")
           << ",\"split_profile\":\"" << escape_json(response.split_profile) << "\""
           << ",\"stage_1_state\":\"" << escape_json(response.stage_1_state) << "\""
           << ",\"stage_2_state\":\"" << escape_json(response.stage_2_state) << "\""
           << ",\"handoff_state\":\"" << escape_json(response.handoff_state) << "\""
           << ",\"request_path_mode\":\"" << escape_json(response.request_path_mode) << "\""
           << ",\"stage_1_device\":\"" << escape_json(response.stage_1_device) << "\""
           << ",\"stage_2_device\":\"" << escape_json(response.stage_2_device) << "\""
           << ",\"stage_1_execution_devices\":\"" << escape_json(response.stage_1_execution_devices) << "\""
           << ",\"stage_2_execution_devices\":\"" << escape_json(response.stage_2_execution_devices) << "\""
           << ",\"split_artifact_version\":\"" << escape_json(response.split_artifact_version) << "\""
           << ",\"validated_context_tokens\":" << response.validated_context_tokens
           << ",\"last_split_request_id\":\"" << escape_json(response.last_split_request_id) << "\""
           << ",\"last_split_sequence_id\":\"" << escape_json(response.last_split_sequence_id) << "\""
           << ",\"last_failure_scope\":\"" << escape_json(response.last_failure_scope) << "\""
           << ",\"fallback_detected\":" << (response.fallback_detected ? "true" : "false")
           << ",\"memory_summary\":{\"stage_1\":\"" << escape_json(response.memory_summary.stage_1)
           << "\",\"stage_2\":\"" << escape_json(response.memory_summary.stage_2)
           << "\",\"system\":\"" << escape_json(response.memory_summary.system) << "\"}"
           << ",\"last_failure_summary\":\"" << escape_json(response.last_failure_summary) << "\"";
    if (!response.embedding_state.empty()) {
        stream << ",\"embedding_state\":\"" << escape_json(response.embedding_state) << "\""
               << ",\"embedding_model\":\"" << escape_json(response.embedding_model) << "\""
               << ",\"embedding_device\":\"" << escape_json(response.embedding_device) << "\"";
    }
    stream << "}";
    return stream.str();
}

std::string to_json_error(const std::string_view code, const std::string_view message) {
    std::ostringstream stream;
    stream << "{\"error\":{\"code\":\"" << escape_json(code) << "\",\"message\":\"" << escape_json(message)
           << "\"}}";
    return stream.str();
}

std::string to_sse_data_event(const std::string_view payload) {
    std::ostringstream stream;
    append_sse_data_lines(stream, payload);
    stream << '\n';
    return stream.str();
}

std::string to_sse_json_event(const std::string_view json_payload) { return to_sse_data_event(json_payload); }

std::string to_sse_done_event() { return "data: [DONE]\n\n"; }

std::string to_sse_error_event(const std::string_view code, const std::string_view message) {
    std::ostringstream stream;
    stream << "{\"error\":{\"code\":\"" << escape_json(code) << "\",\"message\":\"" << escape_json(message)
           << "\"},\"terminal\":true}";
    return to_sse_json_event(stream.str());
}

// --- Embedding API parse/serialize ---

EmbeddingRequest parse_embedding_request(const std::string_view document) {
    const auto root_value = masodik::config::JsonLoader::parse(document);
    if (!root_value.is_object()) {
        throw std::invalid_argument("Az embedding keres gyokere JSON objektum kell legyen.");
    }
    const auto& object = root_value.as_object();

    EmbeddingRequest request;
    request.model = masodik::config::get_string(object, "model");
    if (request.model.empty()) {
        throw std::invalid_argument("A model mezo kotelezo az embedding keresben.");
    }

    const auto* input_value = masodik::config::find_member(object, "input");
    if (input_value == nullptr) {
        throw std::invalid_argument("Az input mezo kotelezo az embedding keresben.");
    }

    if (input_value->is_string()) {
        const auto& text = input_value->as_string();
        if (text.empty()) {
            throw std::invalid_argument("Az input mezo nem lehet ures.");
        }
        request.input.push_back(text);
    } else if (input_value->is_array()) {
        for (const auto& element : input_value->as_array()) {
            if (!element.is_string()) {
                throw std::invalid_argument("Az input tomb csak string elemeket tartalmazhat.");
            }
            request.input.push_back(element.as_string());
        }
        if (request.input.empty()) {
            throw std::invalid_argument("Az input tomb nem lehet ures.");
        }
    } else {
        throw std::invalid_argument("Az input mezo string vagy string tomb kell legyen.");
    }

    request.encoding_format = masodik::config::get_string_or(object, "encoding_format", "float");
    if (request.encoding_format != "float") {
        throw std::invalid_argument("Jelenleg csak a 'float' encoding_format tamogatott.");
    }

    return request;
}

std::string to_json(const EmbeddingResponse& response) {
    std::ostringstream stream;
    stream << "{\"object\":\"" << escape_json(response.object) << "\",\"data\":[";
    for (std::size_t index = 0; index < response.data.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        const auto& item = response.data[index];
        stream << "{\"object\":\"" << escape_json(item.object) << "\",\"embedding\":[";
        for (std::size_t vector_index = 0; vector_index < item.embedding.size(); ++vector_index) {
            if (vector_index > 0) {
                stream << ',';
            }
            stream << item.embedding[vector_index];
        }
        stream << "],\"index\":" << item.index << '}';
    }
    stream << "],\"model\":\"" << escape_json(response.model)
           << "\",\"usage\":{\"prompt_tokens\":" << response.usage.prompt_tokens
           << ",\"total_tokens\":" << response.usage.total_tokens << "}}";
    return stream.str();
}

}  // namespace masodik::api
