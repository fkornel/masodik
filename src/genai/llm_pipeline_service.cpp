#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/diagnostics/logger.hpp"
#include "masodik/diagnostics/queue_metrics.hpp"
#include "masodik/runtime/request_scheduler.hpp"
#include "masodik/split/split_infer_chain.hpp"

#if MASODIK_HAS_OPENVINO_GENAI
#include "openvino/core/graph_util.hpp"
#include "openvino/core/log.hpp"
#include "openvino/core/type/bfloat16.hpp"
#include "openvino/core/type/float16.hpp"
#include "openvino/genai/llm_pipeline.hpp"
#include "openvino/genai/tokenizer.hpp"
#include "openvino/openvino.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <set>
#include <thread>

namespace masodik::genai {
namespace {

std::string truncate_for_log(std::string_view value, const std::size_t limit = 200) {
    if (value.size() <= limit) {
        return std::string{value};
    }
    return std::string{value.substr(0, limit)} + "...";
}

std::string escape_json_string(const std::string_view value) {
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

std::string escape_shell_single_quoted(const std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        if (character == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(character);
        }
    }
    return escaped;
}

std::string find_last_user_message(const masodik::api::ChatCompletionRequest& request);

bool contains_ascii_case_insensitive(std::string_view haystack, std::string_view needle) {
    return std::search(
               haystack.begin(),
               haystack.end(),
               needle.begin(),
               needle.end(),
               [](const char left, const char right) {
                   return std::tolower(static_cast<unsigned char>(left)) ==
                          std::tolower(static_cast<unsigned char>(right));
               }) != haystack.end();
}

#if MASODIK_HAS_OPENVINO_GENAI
std::string describe_variable_state_tensor(const ov::Tensor& tensor) {
    try {
        return masodik::split::describe_tensor_shape(tensor);
    } catch (const std::exception&) {
        return "<dynamic>";
    }
}

std::string summarize_variable_states(ov::InferRequest& infer_request, const std::size_t max_entries = 6U) {
    try {
        std::ostringstream stream;
        const auto states = infer_request.query_state();
        stream << "count=" << states.size();

        std::size_t emitted = 0U;
        for (const auto& state : states) {
            const auto name = state.get_name();
            const bool interesting = contains_ascii_case_insensitive(name, "attention_mask") ||
                                     contains_ascii_case_insensitive(name, "past_key_values");
            if (!interesting) {
                continue;
            }

            try {
                stream << ";" << name << "=" << describe_variable_state_tensor(state.get_state());
            } catch (const std::exception&) {
                stream << ";" << name << "=<dynamic>";
            }
            ++emitted;
            if (emitted >= max_entries) {
                break;
            }
        }

        return stream.str();
    } catch (const std::exception&) {
        return "<unavailable>";
    }
}

ov::genai::ChatHistory build_chat_history(const masodik::api::ChatCompletionRequest& request) {
    std::ostringstream history_json;
    history_json << '[';
    for (std::size_t index = 0; index < request.messages.size(); ++index) {
        const auto& message = request.messages[index];
        if (index > 0U) {
            history_json << ',';
        }

        history_json << "{\"role\":\"" << escape_json_string(message.role) << "\",\"content\":\""
                     << escape_json_string(message.content) << "\"";
        if (!message.tool_calls.empty()) {
            history_json << ",\"tool_calls\":[";
            for (std::size_t tool_index = 0; tool_index < message.tool_calls.size(); ++tool_index) {
                const auto& tool_call = message.tool_calls[tool_index];
                if (tool_index > 0U) {
                    history_json << ',';
                }
                history_json << "{\"id\":\"" << escape_json_string(tool_call.id) << "\",\"type\":\""
                             << escape_json_string(tool_call.type) << "\",\"function\":{\"name\":\""
                             << escape_json_string(tool_call.function.name) << "\",\"arguments\":\""
                             << escape_json_string(tool_call.function.arguments) << "\"}}";
            }
            history_json << ']';
        }
        history_json << '}';
    }
    history_json << ']';
    return ov::genai::ChatHistory(ov::genai::JsonContainer::from_json_string(history_json.str()));
}

struct ChatPromptBuildResult {
    std::string prompt;
    bool used_chat_template = false;
    std::string error;
};

ChatPromptBuildResult build_chat_prompt_for_split(
    const masodik::api::ChatCompletionRequest& request,
    const ov::genai::Tokenizer& tokenizer) {
    ChatPromptBuildResult result;
    result.prompt = find_last_user_message(request);

    if (request.messages.empty()) {
        return result;
    }

    try {
        auto prompt = tokenizer.apply_chat_template(build_chat_history(request), true);
        if (!prompt.empty()) {
            result.prompt = std::move(prompt);
            result.used_chat_template = true;
        }
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}
#endif

void try_log_runtime_message(
    const masodik::runtime::ServiceConfig& service_config,
    const masodik::diagnostics::LogLevel level,
    const std::string& message) {
    try {
        auto logger = masodik::diagnostics::Logger::create_file_logger(service_config.serving_profile.runtime_log_path);
        logger.log(level, message);
    } catch (const std::exception&) {
    }
}

std::string join_notes(const std::vector<std::string>& notes) {
    if (notes.empty()) {
        return {};
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < notes.size(); ++index) {
        if (index > 0) {
            stream << " | ";
        }
        stream << notes[index];
    }
    return stream.str();
}

std::string find_last_user_message(const masodik::api::ChatCompletionRequest& request) {
    for (auto index = request.messages.rbegin(); index != request.messages.rend(); ++index) {
        if (index->role == "user") {
            return index->content;
        }
    }
    return request.messages.back().content;
}

std::size_t estimate_request_prompt_tokens(const masodik::api::ChatCompletionRequest& request) {
    std::size_t characters = 0;
    for (const auto& message : request.messages) {
        characters += message.content.size();
    }
    return std::max<std::size_t>(1U, characters / 3U);
}

double estimate_split_stage_kv_budget_gb(const std::size_t total_tokens) {
    return static_cast<double>(std::max<std::size_t>(1U, total_tokens)) / 4096.0;
}

std::string next_split_sequence_id() {
    static std::atomic<std::uint64_t> sequence_counter{1};
    return "split-seq-" + std::to_string(sequence_counter.fetch_add(1));
}

std::string split_runtime_request_id(const std::uint64_t request_id) {
    return "request-" + std::to_string(request_id);
}

bool genai_disabled_by_environment() {
    const char* value = std::getenv("MASODIK_DISABLE_OPENVINO_GENAI");
    return value != nullptr && std::string_view{value} == "1";
}

std::string_view stream_failure_mode_from_environment() {
    if (const char* value = std::getenv("MASODIK_TEST_STREAM_FAILURE_MODE"); value != nullptr) {
        return value;
    }
    return {};
}

std::string_view generate_failure_mode_from_environment() {
    if (const char* value = std::getenv("MASODIK_TEST_GENERATE_FAILURE_MODE"); value != nullptr) {
        return value;
    }
    return {};
}

int stream_chunk_delay_millis_from_environment() {
    if (const char* value = std::getenv("MASODIK_TEST_STREAM_CHUNK_DELAY_MILLIS"); value != nullptr) {
        try {
            return std::max(0, std::stoi(value));
        } catch (const std::exception&) {
            return 0;
        }
    }
    return 0;
}

std::vector<std::string> split_stream_chunks(const std::string_view text) {
    constexpr std::size_t target_chunk_size = 24;
    constexpr std::size_t minimum_chunk_size = 8;

    std::vector<std::string> chunks;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        auto next = std::min(text.size(), cursor + target_chunk_size);
        if (next < text.size()) {
            auto split = next;
            while (split > cursor + minimum_chunk_size && !std::isspace(static_cast<unsigned char>(text[split - 1]))) {
                --split;
            }
            if (split > cursor + minimum_chunk_size) {
                next = split;
            }
        }

        chunks.emplace_back(text.substr(cursor, next - cursor));
        cursor = next;
    }

    if (chunks.empty()) {
        chunks.emplace_back();
    }
    return chunks;
}

std::string lowercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool is_cpu_device(const std::string_view device_name) {
    return lowercase_ascii(std::string{device_name}).find("cpu") != std::string::npos;
}

std::filesystem::path resolve_split_path(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) {
    return candidate.is_absolute() ? candidate : root / candidate;
}

std::string split_device_summary(const masodik::runtime::ServiceConfig& service_config) {
    return service_config.generation_config.stage_1_device + "->" + service_config.generation_config.stage_2_device;
}

std::string_view split_stage_label(const SplitExecutionStageId stage_id) {
    switch (stage_id) {
    case SplitExecutionStageId::stage_1:
        return "stage-1";
    case SplitExecutionStageId::stage_2:
        return "stage-2";
    }
    return "stage-unknown";
}

bool is_hetero_device(std::string_view device_name) {
    return lowercase_ascii(std::string{device_name}).rfind("hetero:", 0) == 0;
}

#if MASODIK_HAS_OPENVINO_GENAI
std::filesystem::path resolve_split_model_xml_path(const std::filesystem::path& artifact_path) {
    if (!std::filesystem::exists(artifact_path)) {
        throw std::invalid_argument("A split stage artifact nem letezik: " + artifact_path.string());
    }

    if (artifact_path.extension() == ".xml") {
        return artifact_path;
    }

    if (std::filesystem::is_regular_file(artifact_path)) {
        throw std::invalid_argument("A split stage artifact csak konyvtar vagy .xml IR lehet: " + artifact_path.string());
    }

    const std::vector<std::filesystem::path> preferred_paths = {
        artifact_path / "openvino_model.xml",
        artifact_path / "model.xml",
    };
    for (const auto& candidate : preferred_paths) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator(artifact_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".xml") {
            return entry.path();
        }
    }

    throw std::invalid_argument("A split stage artifact konyvtarban nem talalhato OpenVINO IR XML: " + artifact_path.string());
}

ov::Shape materialize_warmup_shape(const ov::Output<const ov::Node>& input_port) {
    const auto partial_shape = input_port.get_partial_shape();
    if (partial_shape.rank().is_dynamic()) {
        return {1};
    }

    ov::Shape shape;
    shape.reserve(partial_shape.rank().get_length());
    for (const auto& dimension : partial_shape) {
        if (dimension.is_dynamic()) {
            shape.push_back(1);
            continue;
        }

        const auto length = static_cast<std::size_t>(std::max<std::int64_t>(1, dimension.get_length()));
        shape.push_back(length);
    }
    return shape;
}

std::string shape_to_string(const ov::Shape& shape) {
    std::ostringstream stream;
    stream << "[";
    for (std::size_t index = 0; index < shape.size(); ++index) {
        if (index > 0) {
            stream << "x";
        }
        stream << shape[index];
    }
    stream << "]";
    return stream.str();
}

std::string compiled_port_name(const ov::Output<const ov::Node>& port) {
    const auto& names = port.get_names();
    if (!names.empty()) {
        return *names.begin();
    }
    return port.get_node()->get_friendly_name() + ":" + std::to_string(port.get_index());
}

std::optional<ov::Output<const ov::Node>> find_compiled_input_port(
    const ov::CompiledModel& compiled_model,
    const std::string_view expected_name) {
    if (expected_name.empty()) {
        return std::nullopt;
    }
    for (const auto& input_port : compiled_model.inputs()) {
        if (compiled_port_name(input_port) == expected_name || input_port.get_node()->get_friendly_name() == expected_name) {
            return input_port;
        }
    }
    return std::nullopt;
}

std::optional<ov::Output<const ov::Node>> find_compiled_output_port(
    const ov::CompiledModel& compiled_model,
    const std::string_view expected_name) {
    if (expected_name.empty()) {
        return std::nullopt;
    }
    for (const auto& output_port : compiled_model.outputs()) {
        if (compiled_port_name(output_port) == expected_name || output_port.get_node()->get_friendly_name() == expected_name) {
            return output_port;
        }
    }
    return std::nullopt;
}

void prepare_runtime_stage_inputs(
    ov::InferRequest& infer_request,
    const ov::CompiledModel& compiled_model,
    const std::optional<ov::Output<const ov::Node>>& excluded_input = std::nullopt) {
    for (const auto& input_port : compiled_model.inputs()) {
        if (excluded_input.has_value() && compiled_port_name(input_port) == compiled_port_name(*excluded_input)) {
            continue;
        }
        const auto shape = materialize_warmup_shape(input_port);
        ov::Tensor tensor{input_port.get_element_type(), shape};
        std::memset(tensor.data(), 0, tensor.get_byte_size());
        infer_request.set_tensor(input_port, tensor);
    }
}

std::string format_uint64_hex(const std::uint64_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setfill('0') << std::setw(16) << value;
    return stream.str();
}

std::string build_compiled_split_response(
    const std::string_view prompt_preview,
    const std::string_view stage_1_output_name,
    const masodik::split::SplitTensorHandoffBuffer& handoff_buffer,
    const std::string_view stage_2_output_name,
    const ov::Tensor& stage_2_output_tensor) {
    std::ostringstream stream;
    stream << "Compiled split infer sikeres. "
           << "Stage-1 output=" << stage_1_output_name
           << " handoff_target=" << handoff_buffer.target_tensor_name
           << " handoff_shape=" << masodik::split::describe_tensor_shape(handoff_buffer.tensor)
           << " handoff_bytes=" << handoff_buffer.byte_size
           << " handoff_fingerprint=" << format_uint64_hex(handoff_buffer.fingerprint)
           << " | Stage-2 output=" << stage_2_output_name
           << " output_shape=" << masodik::split::describe_tensor_shape(stage_2_output_tensor)
           << " output_bytes=" << stage_2_output_tensor.get_byte_size()
           << " | prompt_preview=\"" << truncate_for_log(prompt_preview, 96) << "\"";
    return stream.str();
}

bool split_tokenizer_models_present(const std::filesystem::path& split_root) {
    return std::filesystem::exists(split_root / "openvino_tokenizer.xml") &&
        std::filesystem::exists(split_root / "openvino_detokenizer.xml");
}

bool split_tokenizer_models_partially_present(const std::filesystem::path& split_root) {
    const bool has_tokenizer = std::filesystem::exists(split_root / "openvino_tokenizer.xml");
    const bool has_detokenizer = std::filesystem::exists(split_root / "openvino_detokenizer.xml");
    return has_tokenizer != has_detokenizer;
}

std::size_t tensor_sequence_length(const ov::Tensor& tensor) {
    const auto& shape = tensor.get_shape();
    if (shape.empty()) {
        return 0U;
    }
    if (shape.size() == 1U) {
        return shape.front();
    }
    return shape[1];
}

ov::Tensor make_beam_idx_tensor(const std::size_t batch_size = 1U) {
    ov::Tensor tensor{ov::element::i32, ov::Shape{batch_size}};
    std::fill_n(tensor.data<std::int32_t>(), batch_size, 0);
    return tensor;
}

ov::Tensor make_attention_mask_tensor(const std::size_t token_count) {
    ov::Tensor tensor{ov::element::i64, ov::Shape{1U, std::max<std::size_t>(1U, token_count)}};
    std::fill_n(tensor.data<std::int64_t>(), tensor.get_size(), 1LL);
    return tensor;
}

ov::Tensor make_position_ids_tensor(const std::size_t start_position, const std::size_t length) {
    ov::Tensor tensor{ov::element::i64, ov::Shape{1U, std::max<std::size_t>(1U, length)}};
    auto* data = tensor.data<std::int64_t>();
    for (std::size_t index = 0; index < tensor_sequence_length(tensor); ++index) {
        data[index] = static_cast<std::int64_t>(start_position + index);
    }
    return tensor;
}

ov::Tensor make_single_token_tensor(const std::int64_t token_id) {
    ov::Tensor tensor{ov::element::i64, ov::Shape{1U, 1U}};
    tensor.data<std::int64_t>()[0] = token_id;
    return tensor;
}

ov::Tensor slice_sequence_tensor(const ov::Tensor& source, const std::size_t start, const std::size_t length) {
    const auto total_length = tensor_sequence_length(source);
    if (length == 0U || start >= total_length || start + length > total_length) {
        throw std::out_of_range("A sequence tensor szeletelesi tartomanya ervenytelen.");
    }

    ov::Shape target_shape = source.get_shape();
    if (target_shape.empty()) {
        throw std::invalid_argument("Ures ranku tensor nem szeletelheto.");
    }
    if (target_shape.size() == 1U) {
        target_shape[0] = length;
    } else {
        target_shape[1] = length;
    }

    ov::Tensor sliced{source.get_element_type(), target_shape};
    const auto stride_bytes = source.get_byte_size() / total_length;
    const auto offset_bytes = start * stride_bytes;
    const auto copy_bytes = length * stride_bytes;
    std::memcpy(sliced.data(), static_cast<const std::byte*>(source.data()) + offset_bytes, copy_bytes);
    return sliced;
}

void bind_named_tensor_if_present(
    ov::InferRequest& infer_request,
    const ov::CompiledModel& compiled_model,
    const std::string_view input_name,
    const ov::Tensor& tensor) {
    if (const auto input_port = find_compiled_input_port(compiled_model, input_name); input_port.has_value()) {
        infer_request.set_tensor(*input_port, tensor);
    }
}

void bind_split_generation_inputs(
    ov::InferRequest& infer_request,
    const ov::CompiledModel& compiled_model,
    const ov::Tensor& input_ids,
    const ov::Tensor& attention_mask,
    const ov::Tensor& position_ids) {
    bind_named_tensor_if_present(infer_request, compiled_model, "input_ids", input_ids);
    bind_named_tensor_if_present(infer_request, compiled_model, "attention_mask", attention_mask);
    bind_named_tensor_if_present(infer_request, compiled_model, "position_ids", position_ids);
    bind_named_tensor_if_present(infer_request, compiled_model, "beam_idx", make_beam_idx_tensor(input_ids.get_shape().front()));
}

template <typename ValueType>
std::int64_t argmax_token_index(const ValueType* logits, const std::size_t count) {
    if (count == 0U) {
        throw std::invalid_argument("Az argmax tokenvalasztashoz legalabb egy logits ertek kell.");
    }

    std::size_t best_index = 0U;
    auto best_value = static_cast<float>(logits[0]);
    for (std::size_t index = 1; index < count; ++index) {
        const auto candidate = static_cast<float>(logits[index]);
        if (candidate > best_value) {
            best_value = candidate;
            best_index = index;
        }
    }
    return static_cast<std::int64_t>(best_index);
}

// Mintaveteleses token valasztas: temperature scaling + top-k + top-p + repetition penalty.
// Racionále: a greedy argmax ismetlodo loop-okat okoz, a sampling-gel valtosatosabb
// es termeszetesebb valaszokat kapunk.
struct SamplingParams {
    float temperature = 1.0f;
    float repetition_penalty = 1.0f;
    float top_p = 1.0f;
    int top_k = 0;
    const std::vector<std::int64_t>* generated_tokens = nullptr;
};

template <typename ValueType>
std::int64_t sample_token_from_logits(
    const ValueType* raw_logits,
    const std::size_t vocab_size,
    const SamplingParams& params) {
    // Logits → float konverzió
    std::vector<float> logits(vocab_size);
    for (std::size_t i = 0; i < vocab_size; ++i) {
        logits[i] = static_cast<float>(raw_logits[i]);
    }

    // Repetition penalty alkalmazasa a mar generalt tokenekre
    if (params.repetition_penalty != 1.0f && params.generated_tokens != nullptr) {
        for (const auto token_id : *params.generated_tokens) {
            if (token_id >= 0 && static_cast<std::size_t>(token_id) < vocab_size) {
                if (logits[token_id] > 0.0f) {
                    logits[token_id] /= params.repetition_penalty;
                } else {
                    logits[token_id] *= params.repetition_penalty;
                }
            }
        }
    }

    // Temperature scaling
    if (params.temperature > 0.0f && params.temperature != 1.0f) {
        for (auto& logit : logits) {
            logit /= params.temperature;
        }
    }

    // Top-k szures: csak a top_k legmagasabb logits-u token marad
    std::vector<std::pair<float, std::int64_t>> scored(vocab_size);
    for (std::size_t i = 0; i < vocab_size; ++i) {
        scored[i] = {logits[i], static_cast<std::int64_t>(i)};
    }
    const auto effective_k = params.top_k > 0
        ? static_cast<std::size_t>(std::min(params.top_k, static_cast<int>(vocab_size)))
        : vocab_size;
    if (effective_k < vocab_size) {
        std::partial_sort(scored.begin(), scored.begin() + effective_k, scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
        scored.resize(effective_k);
    }

    // Softmax
    float max_logit = scored[0].first;
    for (const auto& s : scored) { max_logit = std::max(max_logit, s.first); }
    float sum = 0.0f;
    std::vector<float> probs(scored.size());
    for (std::size_t i = 0; i < scored.size(); ++i) {
        probs[i] = std::exp(scored[i].first - max_logit);
        sum += probs[i];
    }
    for (auto& p : probs) { p /= sum; }

    // Top-p (nucleus) szures
    if (params.top_p < 1.0f) {
        std::vector<std::size_t> indices(probs.size());
        std::iota(indices.begin(), indices.end(), 0U);
        std::sort(indices.begin(), indices.end(),
            [&probs](std::size_t a, std::size_t b) { return probs[a] > probs[b]; });
        float cumulative = 0.0f;
        std::size_t cutoff = indices.size();
        for (std::size_t i = 0; i < indices.size(); ++i) {
            cumulative += probs[indices[i]];
            if (cumulative >= params.top_p) {
                cutoff = i + 1;
                break;
            }
        }
        // Nullazzuk a cutoff utani tokeneket
        for (std::size_t i = cutoff; i < indices.size(); ++i) {
            probs[indices[i]] = 0.0f;
        }
        // Ujranormalizalas
        float resum = 0.0f;
        for (auto p : probs) { resum += p; }
        if (resum > 0.0f) {
            for (auto& p : probs) { p /= resum; }
        }
    }

    // Mintaveteles
    thread_local std::mt19937 rng{std::random_device{}()};
    std::discrete_distribution<std::size_t> dist(probs.begin(), probs.end());
    return scored[dist(rng)].second;
}

std::int64_t select_next_token_id_from_logits(
    const ov::Tensor& logits_tensor,
    const std::size_t sequence_index,
    const SamplingParams& sampling = {}) {
    const auto& shape = logits_tensor.get_shape();
    if (shape.size() < 2U) {
        throw std::runtime_error("A logits tensor rankje legalabb 2 kell legyen a tokenvalasztashoz.");
    }

    std::size_t vocab_size = shape.back();
    std::size_t row_index = 0U;
    if (shape.size() >= 3U) {
        const auto sequence_length = shape[shape.size() - 2U];
        if (sequence_length == 0U) {
            throw std::runtime_error("A logits tensor ures szekvenciahosszal erkezett vissza.");
        }
        row_index = std::min(sequence_index, sequence_length - 1U);
    }
    const std::size_t base_offset = row_index * vocab_size;

    const bool use_sampling = sampling.temperature > 0.0f &&
        (sampling.temperature != 1.0f || sampling.repetition_penalty != 1.0f ||
         sampling.top_p < 1.0f || sampling.top_k > 0);

    switch (logits_tensor.get_element_type()) {
    case ov::element::f32:
        return use_sampling
            ? sample_token_from_logits(logits_tensor.data<const float>() + base_offset, vocab_size, sampling)
            : argmax_token_index(logits_tensor.data<const float>() + base_offset, vocab_size);
    case ov::element::f16:
        return use_sampling
            ? sample_token_from_logits(logits_tensor.data<const ov::float16>() + base_offset, vocab_size, sampling)
            : argmax_token_index(logits_tensor.data<const ov::float16>() + base_offset, vocab_size);
    case ov::element::bf16:
        return use_sampling
            ? sample_token_from_logits(logits_tensor.data<const ov::bfloat16>() + base_offset, vocab_size, sampling)
            : argmax_token_index(logits_tensor.data<const ov::bfloat16>() + base_offset, vocab_size);
    default:
        throw std::runtime_error(
            "A split detokenizalashoz nem tamogatott logits precision erkezett: " +
            logits_tensor.get_element_type().get_type_name());
    }
}

std::string decode_generated_tokens(
    ov::genai::Tokenizer& tokenizer,
    const std::vector<std::int64_t>& generated_tokens) {
    if (generated_tokens.empty()) {
        return {};
    }

    const auto raw_decoded = tokenizer.decode(generated_tokens, ov::genai::skip_special_tokens(false));
    constexpr std::string_view final_marker = "<|channel|>final<|message|>";
    if (const auto final_pos = raw_decoded.find(final_marker); final_pos != std::string::npos) {
        auto content = raw_decoded.substr(final_pos + final_marker.size());
        if (const auto return_pos = content.find("<|return|>"); return_pos != std::string::npos) {
            content.erase(return_pos);
        }
        if (const auto end_pos = content.find("<|end|>"); end_pos != std::string::npos) {
            content.erase(end_pos);
        }
        if (const auto assistant_pos = content.rfind("<|start|>assistant"); assistant_pos != std::string::npos) {
            content.erase(assistant_pos);
        }
        if (!content.empty()) {
            return content;
        }
    }

    auto decoded = tokenizer.decode(generated_tokens, ov::genai::skip_special_tokens(true));
    // A modell "analysis...assistantfinal..." formatumban general.
    // Az "analysis" reszt <think> tagbe csomagoljuk, hogy az Open WebUI
    // (es hasonlo kliensek) vizualisan elkulonitsek a gondolkodast a vegso valasztol.
    constexpr std::string_view analysis_prefix = "analysis";
    constexpr std::string_view assistant_final_marker = "assistantfinal";
    if (const auto final_pos = decoded.find(assistant_final_marker); final_pos != std::string::npos) {
        auto thinking = decoded.substr(0, final_pos);
        auto answer = decoded.substr(final_pos + assistant_final_marker.size());
        // Az "analysis" prefix eltavolitasa a gondolkodas elejerol
        if (thinking.starts_with(analysis_prefix)) {
            thinking.erase(0, analysis_prefix.size());
        }
        // Csak akkor adunk <think> taget, ha van erdemi gondolkodas tartalom
        if (!thinking.empty()) {
            return "<think>" + thinking + "</think>\n" + answer;
        }
        return answer;
    }
    // Fallback: ha nincs "assistantfinal" marker, probaljuk a regi logikát
    if (const auto final_pos = decoded.rfind("final"); final_pos == 0U) {
        decoded.erase(0, std::string_view{"final"}.size());
    }
    if (decoded.empty()) {
        decoded = raw_decoded;
    }
    return decoded;
}

bool runtime_assignment_diagnostics_requested(const masodik::runtime::ServiceConfig& service_config) {
    if (const char* raw = std::getenv("MASODIK_ENABLE_RUNTIME_ASSIGNMENT_DIAGNOSTICS"); raw != nullptr) {
        const auto value = lowercase_ascii(raw);
        return value == "1" || value == "true" || value == "yes" || value == "on";
    }
    return service_config.serving_profile.enable_runtime_assignment_diagnostics;
}

bool pipeline_parallel_requested(const masodik::runtime::ServiceConfig& service_config) {
    return lowercase_ascii(service_config.serving_profile.model_distribution_policy) == "pipeline_parallel";
}

std::string any_to_string(const ov::Any& value) {
    if (value.empty()) {
        return "empty";
    }

    std::ostringstream stream;
    value.print(stream);
    auto text = stream.str();
    if (text.empty()) {
        text = value.type_info().name();
    }
    return text;
}

std::string join_strings(const std::vector<std::string>& values, const std::string_view separator = ", ") {
    if (values.empty()) {
        return {};
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << separator;
        }
        stream << values[index];
    }
    return stream.str();
}

std::string summarize_property_names(const ov::AnyMap& properties) {
    std::vector<std::string> names;
    names.reserve(properties.size());
    for (const auto& [name, value] : properties) {
        (void)value;
        names.push_back(name);
    }
    return join_strings(names);
}

void apply_kv_cache_precision_property(
    const masodik::runtime::ServiceConfig& service_config,
    ov::AnyMap& properties) {
    if (!service_config.generation_config.kv_cache_precision.empty()) {
        properties["KV_CACHE_PRECISION"] = service_config.generation_config.kv_cache_precision;
    }
}

std::filesystem::path build_runtime_assignment_path(
    const masodik::runtime::ServiceConfig& service_config,
    const std::string_view suffix) {
    auto directory = service_config.serving_profile.runtime_log_path.parent_path();
    if (directory.empty()) {
        directory = service_config.repo_root / "build" / "logs";
    }

    auto stem = service_config.serving_profile.runtime_log_path.stem().string();
    if (stem.empty()) {
        stem = "masodik-runtime";
    }

    return directory / (stem + std::string{suffix});
}

void emit_runtime_assignment_summary(
    const masodik::runtime::ServiceConfig& service_config,
    const std::shared_ptr<const ov::Model>& runtime_model) {
    if (!runtime_model) {
        try_log_runtime_message(
            service_config,
            masodik::diagnostics::LogLevel::warning,
            "genai_runtime_assignment_missing profile=" + service_config.active_profile +
                " effective_device=" + service_config.effective_device +
                " reason=\"Compiled model did not expose runtime model\"");
        return;
    }

    std::map<std::string, std::size_t> affinity_counts;
    std::size_t nodes_with_rt_info = 0;
    std::size_t nodes_with_known_assignment = 0;
    std::size_t logged_nodes = 0;
    constexpr std::size_t max_logged_nodes = 80;

    const std::vector<std::string> interesting_keys = {
        "affinity", "originalLayersNames", "primitiveType", "execType", "layerType", "runtimePrecision"};

    for (const auto& node : runtime_model->get_ordered_ops()) {
        const auto& rt_info = node->get_rt_info();
        if (rt_info.empty()) {
            continue;
        }

        ++nodes_with_rt_info;
        std::vector<std::string> fragments;
        for (const auto& key : interesting_keys) {
            const auto entry = rt_info.find(key);
            if (entry == rt_info.end()) {
                continue;
            }

            const auto value = truncate_for_log(any_to_string(entry->second), 180);
            fragments.push_back(key + "=\"" + value + "\"");
            if (key == "affinity") {
                ++affinity_counts[value];
            }
        }

        if (fragments.empty()) {
            continue;
        }

        ++nodes_with_known_assignment;
        if (logged_nodes < max_logged_nodes) {
            try_log_runtime_message(
                service_config,
                masodik::diagnostics::LogLevel::debug,
                "genai_runtime_node name=\"" + node->get_friendly_name() + "\" type=" + node->get_type_name() + " " +
                    join_strings(fragments, " "));
            ++logged_nodes;
        }
    }

    std::vector<std::string> affinity_fragments;
    for (const auto& [affinity, count] : affinity_counts) {
        affinity_fragments.push_back(affinity + ":" + std::to_string(count));
    }

    std::vector<std::string> model_rt_keys;
    model_rt_keys.reserve(runtime_model->get_rt_info().size());
    for (const auto& [key, value] : runtime_model->get_rt_info()) {
        (void)value;
        model_rt_keys.push_back(key);
    }

    try_log_runtime_message(
        service_config,
        masodik::diagnostics::LogLevel::info,
        "genai_runtime_assignment_summary profile=" + service_config.active_profile +
            " effective_device=" + service_config.effective_device +
            " runtime_nodes=" + std::to_string(runtime_model->get_ordered_ops().size()) +
            " nodes_with_rt_info=" + std::to_string(nodes_with_rt_info) +
            " nodes_with_known_assignment=" + std::to_string(nodes_with_known_assignment) +
            " affinity_counts=\"" + join_strings(affinity_fragments, "; ") + "\"" +
            " model_rt_keys=\"" + join_strings(model_rt_keys) + "\"");
}

void run_runtime_assignment_diagnostics(
    const masodik::runtime::ServiceConfig& service_config,
    const ov::AnyMap& pipeline_properties) {
    if (!runtime_assignment_diagnostics_requested(service_config)) {
        return;
    }

    try {
        auto diagnostic_properties = pipeline_properties;
        diagnostic_properties[ov::enable_profiling.name()] = true;

        std::filesystem::create_directories(service_config.serving_profile.runtime_log_path.parent_path());

        ov::Core core;
        auto compiled_model = core.compile_model(
            service_config.model_repository.model_xml_path,
            service_config.effective_device,
            diagnostic_properties);
        auto runtime_model = compiled_model.get_runtime_model();

        const auto runtime_graph_xml = build_runtime_assignment_path(service_config, "-exec-graph.xml");
        const auto runtime_graph_bin = build_runtime_assignment_path(service_config, "-exec-graph.bin");
        ov::serialize(runtime_model, runtime_graph_xml, runtime_graph_bin);

        try_log_runtime_message(
            service_config,
            masodik::diagnostics::LogLevel::info,
            "genai_runtime_assignment_dump profile=" + service_config.active_profile +
                " effective_device=" + service_config.effective_device +
                " model_xml=\"" + service_config.model_repository.model_xml_path.string() + "\"" +
                " diagnostic_properties=\"" + summarize_property_names(diagnostic_properties) + "\"" +
                " runtime_graph_xml=\"" + runtime_graph_xml.string() + "\"" +
                " runtime_graph_bin=\"" + runtime_graph_bin.string() + "\"");

        emit_runtime_assignment_summary(service_config, runtime_model);
    } catch (const std::exception& error) {
        try_log_runtime_message(
            service_config,
            masodik::diagnostics::LogLevel::warning,
            "genai_runtime_assignment_failed profile=" + service_config.active_profile +
                " effective_device=" + service_config.effective_device +
                " diagnostic_properties=\"" + summarize_property_names(pipeline_properties) + ", " +
                ov::enable_profiling.name() + "\"" +
                " error=\"" + std::string{error.what()} + "\"");
    }
}

void schedule_runtime_assignment_diagnostics(
    const masodik::runtime::ServiceConfig& service_config,
    const ov::AnyMap& pipeline_properties) {
    if (!runtime_assignment_diagnostics_requested(service_config)) {
        return;
    }

    const auto runtime_graph_xml = build_runtime_assignment_path(service_config, "-exec-graph.xml");
    const auto runtime_graph_bin = build_runtime_assignment_path(service_config, "-exec-graph.bin");
    try_log_runtime_message(
        service_config,
        masodik::diagnostics::LogLevel::info,
        "genai_runtime_assignment_scheduled profile=" + service_config.active_profile +
            " effective_device=" + service_config.effective_device +
            " runtime_graph_xml=\"" + runtime_graph_xml.string() + "\"" +
            " runtime_graph_bin=\"" + runtime_graph_bin.string() + "\"");

    std::thread([service_config, pipeline_properties]() {
        run_runtime_assignment_diagnostics(service_config, pipeline_properties);
    }).detach();
}

ov::log::Level openvino_log_level_from_environment_or_profile(const masodik::runtime::ServiceConfig& service_config) {
    if (const char* raw = std::getenv("MASODIK_OPENVINO_LOG_LEVEL"); raw != nullptr) {
        const auto value = lowercase_ascii(raw);
        if (value == "trace") {
            return ov::log::Level::TRACE;
        }
        if (value == "debug") {
            return ov::log::Level::DEBUG;
        }
        if (value == "info") {
            return ov::log::Level::INFO;
        }
        if (value == "warning" || value == "warn") {
            return ov::log::Level::WARNING;
        }
        if (value == "error" || value == "err") {
            return ov::log::Level::ERR;
        }
        if (value == "none" || value == "off") {
            return ov::log::Level::NO;
        }
    }

    return ov::log::Level::WARNING;
}

std::string to_string(const ov::log::Level level) {
    switch (level) {
    case ov::log::Level::NO:
        return "LOG_NONE";
    case ov::log::Level::ERR:
        return "LOG_ERROR";
    case ov::log::Level::WARNING:
        return "LOG_WARNING";
    case ov::log::Level::INFO:
        return "LOG_INFO";
    case ov::log::Level::DEBUG:
        return "LOG_DEBUG";
    case ov::log::Level::TRACE:
        return "LOG_TRACE";
    }
    return "LOG_UNKNOWN";
}
#endif

bool contains_any(const std::string_view haystack, const std::initializer_list<std::string_view> needles) {
    for (const auto needle : needles) {
        if (haystack.find(needle) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

bool is_file_intent(const std::string_view normalized_last_message) {
    return contains_any(
        normalized_last_message,
        {"agent.md",
         "workspace",
         "file",
         "fajl",
         "files",
         "mappa",
         "folder",
         "directory",
         "dir",
         "memory",
         "memories",
         "read",
         "olvasd",
         "olvasd ki",
         "check your"});
}

bool is_read_intent(const std::string_view normalized_last_message) {
    return contains_any(
        normalized_last_message,
        {"read",
         "open",
         "show",
         "view",
         "inspect",
         "follow",
         "olvasd",
         "olvasd ki",
         "mutasd",
         "nyisd meg"});
}

bool is_path_character(const char character) {
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '/' || character == '\\' ||
           character == '.' || character == '_' || character == '-' || character == ':' || character == '~';
}

std::string extract_explicit_file_path(const std::string_view original_last_message) {
    for (std::size_t index = 0; index < original_last_message.size(); ++index) {
        const char character = original_last_message[index];
        const bool unix_path = character == '/';
        const bool windows_path =
            std::isalpha(static_cast<unsigned char>(character)) != 0 && index + 2 < original_last_message.size() &&
            original_last_message[index + 1] == ':' &&
            (original_last_message[index + 2] == '\\' || original_last_message[index + 2] == '/');
        if (!unix_path && !windows_path) {
            continue;
        }

        std::size_t end = index;
        while (end < original_last_message.size() && is_path_character(original_last_message[end])) {
            ++end;
        }

        const auto candidate = std::string{original_last_message.substr(index, end - index)};
        if (candidate.find('.') != std::string::npos && candidate.find('/') != std::string::npos) {
            return candidate;
        }
        if (candidate.find('\\') != std::string::npos &&
            (candidate.find('.') != std::string::npos || candidate.find(':') != std::string::npos)) {
            return candidate;
        }
    }

    return {};
}

std::string extract_explicit_filename(const std::string_view original_last_message) {
    constexpr std::string_view extensions[] = {
        ".md", ".txt", ".json", ".yaml", ".yml", ".log", ".cfg", ".ini", ".cpp", ".hpp", ".h"};

    std::size_t token_start = 0;
    while (token_start < original_last_message.size()) {
        while (token_start < original_last_message.size() &&
               !std::isalnum(static_cast<unsigned char>(original_last_message[token_start]))) {
            ++token_start;
        }
        std::size_t token_end = token_start;
        while (token_end < original_last_message.size() &&
               (std::isalnum(static_cast<unsigned char>(original_last_message[token_end])) != 0 ||
                original_last_message[token_end] == '.' || original_last_message[token_end] == '_' ||
                original_last_message[token_end] == '-')) {
            ++token_end;
        }

        if (token_end > token_start) {
            const auto candidate = std::string{original_last_message.substr(token_start, token_end - token_start)};
            const auto normalized_candidate = lowercase_ascii(candidate);
            if (std::ranges::any_of(extensions, [&](const auto extension) {
                    return normalized_candidate.size() > extension.size() &&
                           normalized_candidate.ends_with(extension);
                })) {
                return candidate;
            }
        }

        token_start = token_end + 1;
    }

    return {};
}

bool is_shell_intent(const std::string_view normalized_last_message) {
    return contains_any(
        normalized_last_message,
        {"ssh",
         "shell",
         "terminal",
         "parancs",
         "command",
         "exec",
         "futtasd",
         "run",
         "open ssh"});
}

bool is_web_search_intent(const std::string_view normalized_last_message) {
    return contains_any(
        normalized_last_message,
        {"search on the web",
         "search the web",
         "web search",
         "internet search",
         "search online",
         "look up online",
         "look it up",
         "search for",
         "on the internet",
         "on the web",
         "interneten",
         "weben",
         "keress ra",
         "keress rá",
         "online"});
}

bool tool_matches_intent(
    const masodik::api::ToolDefinition& tool_definition,
    const std::string_view normalized_last_message,
    const std::string_view original_last_message) {
    const auto normalized_name = lowercase_ascii(tool_definition.function.name);
    const auto normalized_description = lowercase_ascii(tool_definition.function.description);
    const auto combined = normalized_name + " " + normalized_description + " " + tool_definition.function.parameters_json;
    const bool has_path_field = tool_definition.function.parameters_json.find("\"path\"") != std::string::npos ||
                                tool_definition.function.parameters_json.find("\"file_path\"") != std::string::npos;
    const bool has_command_field = tool_definition.function.parameters_json.find("\"command\"") != std::string::npos ||
                                   tool_definition.function.name == "exec";
    const bool explicit_file_reference =
        !extract_explicit_file_path(original_last_message).empty() || !extract_explicit_filename(original_last_message).empty();

    if (contains_any(normalized_last_message, {"pr", "pull request", "repo", "status", "statusz", "branch", "commit"})) {
        return contains_any(combined, {"exec", "shell", "command", "terminal", "git", "gh"});
    }

    if (is_web_search_intent(normalized_last_message)) {
        const bool query_based = tool_definition.function.parameters_json.find("\"query\"") != std::string::npos;
        const bool explicit_web_tool =
            contains_any(combined, {"web_search", "web search", "internet", "online", "website", "web search to"});
        const bool github_scoped =
            contains_any(combined, {"github", "git", "repo", "pull request", "issue", "code search", "commit", "branch"});
        return query_based && explicit_web_tool && !github_scoped;
    }

    if (explicit_file_reference && is_read_intent(normalized_last_message)) {
        if (has_path_field) {
            return true;
        }
        if (has_command_field) {
            return true;
        }
        return false;
    }

    if (is_file_intent(normalized_last_message)) {
        if (has_path_field) {
            return true;
        }
        return contains_any(combined, {"read", "file", "path", "workspace", "glob", "list", "dir", "ls", "cat"});
    }

    if (is_shell_intent(normalized_last_message)) {
        return contains_any(combined, {"ssh", "exec", "shell", "command", "terminal", "host"});
    }

    return false;
}

std::string extract_ipv4_address(const std::string_view text) {
    std::string candidate;
    int dot_count = 0;

    for (const char character : text) {
        if (std::isdigit(static_cast<unsigned char>(character)) || character == '.') {
            candidate.push_back(character);
            if (character == '.') {
                ++dot_count;
            }
            continue;
        }

        if (dot_count == 3 && !candidate.empty()) {
            return candidate;
        }

        candidate.clear();
        dot_count = 0;
    }

    if (dot_count == 3 && !candidate.empty()) {
        return candidate;
    }

    return {};
}

std::string derive_file_path(const std::string_view normalized_last_message, const std::string_view original_last_message) {
    if (const auto explicit_path = extract_explicit_file_path(original_last_message); !explicit_path.empty()) {
        return explicit_path;
    }
    if (const auto explicit_filename = extract_explicit_filename(original_last_message); !explicit_filename.empty()) {
        return explicit_filename;
    }
    if (contains_any(normalized_last_message, {"agent.md"})) {
        return "AGENT.md";
    }
    if (contains_any(normalized_last_message, {"workspace", "memory", "memories"})) {
        return ".";
    }
    return ".";
}

const masodik::api::ToolDefinition* select_tool_definition(const masodik::api::ChatCompletionRequest& request) {
    if (request.tools.empty()) {
        return nullptr;
    }

    const auto last_message = find_last_user_message(request);
    const auto normalized_last_message = lowercase_ascii(last_message);
    const bool explicit_file_reference =
        !extract_explicit_file_path(last_message).empty() || !extract_explicit_filename(last_message).empty();

    if (request.tool_choice.kind == masodik::api::ToolChoiceKind::named_function) {
        const auto selected = std::find_if(
            request.tools.begin(),
            request.tools.end(),
            [&](const masodik::api::ToolDefinition& tool) {
                return tool.function.name == request.tool_choice.function_name;
            });
        return selected == request.tools.end() ? nullptr : &(*selected);
    }

    if (explicit_file_reference && is_read_intent(normalized_last_message)) {
        const auto path_selected = std::find_if(
            request.tools.begin(),
            request.tools.end(),
            [](const masodik::api::ToolDefinition& tool) {
                return tool.function.parameters_json.find("\"path\"") != std::string::npos ||
                       tool.function.parameters_json.find("\"file_path\"") != std::string::npos;
            });
        if (path_selected != request.tools.end()) {
            return &(*path_selected);
        }
    }

    const auto selected = std::find_if(
        request.tools.begin(),
        request.tools.end(),
        [&](const masodik::api::ToolDefinition& tool) {
            return tool_matches_intent(tool, normalized_last_message, last_message);
        });
    if (selected != request.tools.end()) {
        return &(*selected);
    }

    return &request.tools.front();
}

bool should_emit_tool_call(
    const masodik::api::ChatCompletionRequest& request,
    const std::string_view normalized_last_message) {
    if (request.messages.empty() || request.messages.back().role != "user") {
        return false;
    }
    return classify_request_decision_path(request) != masodik::api::RequestDecisionPath::content_only;
}

std::string derive_exec_command(
    const std::string_view normalized_last_message,
    const std::string_view original_last_message) {
    if (is_read_intent(normalized_last_message)) {
        const auto explicit_path = derive_file_path(normalized_last_message, original_last_message);
        if (!explicit_path.empty() && explicit_path != ".") {
            const auto shell_path = escape_shell_single_quoted(explicit_path);
            return "test -f '" + shell_path + "' && sed -n '1,240p' '" + shell_path + "'";
        }
    }
    if (contains_any(normalized_last_message, {"agent.md"})) {
        return "find . -iname 'AGENT.md' -o -iname 'agent.md' | head -n 5";
    }
    if (contains_any(normalized_last_message, {"workspace", "files", "file", "directory", "folder", "mappa"})) {
        return "pwd && find . -maxdepth 2 -type f | sed -n '1,80p'";
    }
    if (contains_any(normalized_last_message, {"ssh", "open ssh"})) {
        const auto host = extract_ipv4_address(normalized_last_message);
        if (!host.empty()) {
            return "ssh " + host;
        }
        return "ssh";
    }
    if (contains_any(normalized_last_message, {"pr", "pull request"})) {
        return "gh pr list --json number,title,state";
    }
    if (contains_any(normalized_last_message, {"status", "statusz"})) {
        return "git status --short";
    }
    if (contains_any(normalized_last_message, {"branch", "ag"})) {
        return "git branch --show-current";
    }
    if (contains_any(normalized_last_message, {"commit", "log"})) {
        return "git log --oneline -5";
    }
    return "git status --short";
}

std::string synthesize_tool_arguments(
    const masodik::api::ToolDefinition& tool_definition,
    const std::string_view normalized_last_message,
    const std::string_view original_last_message) {
    const auto& parameters_json = tool_definition.function.parameters_json;
    const auto uses_command_field = parameters_json.find("\"command\"") != std::string::npos || tool_definition.function.name == "exec";
    if (uses_command_field) {
        return std::string{"{\"command\":\""} +
               escape_json_string(derive_exec_command(normalized_last_message, original_last_message)) + "\"}";
    }
    if (parameters_json.find("\"path\"") != std::string::npos || parameters_json.find("\"file_path\"") != std::string::npos) {
        const auto path = derive_file_path(normalized_last_message, original_last_message);
        if (parameters_json.find("\"file_path\"") != std::string::npos) {
            return std::string{"{\"file_path\":\""} + escape_json_string(path) + "\"}";
        }
        return std::string{"{\"path\":\""} + escape_json_string(path) + "\"}";
    }
    if (parameters_json.find("\"host\"") != std::string::npos) {
        const auto host = extract_ipv4_address(normalized_last_message);
        if (parameters_json.find("\"command\"") != std::string::npos) {
            return std::string{"{\"host\":\""} + escape_json_string(host) + "\",\"command\":\"pwd\"}";
        }
        return std::string{"{\"host\":\""} + escape_json_string(host) + "\"}";
    }
    if (parameters_json.find("\"query\"") != std::string::npos) {
        return std::string{"{\"query\":\""} + escape_json_string(original_last_message) + "\"}";
    }
    return std::string{"{\"input\":\""} + escape_json_string(original_last_message) + "\"}";
}

ChatGenerationResult build_tool_call_result(const masodik::api::ChatCompletionRequest& request) {
    const auto last_message = find_last_user_message(request);
    const auto normalized_last_message = lowercase_ascii(last_message);
    if (!should_emit_tool_call(request, normalized_last_message)) {
        return {};
    }

    const auto* tool_definition = select_tool_definition(request);
    if (tool_definition == nullptr) {
        throw std::invalid_argument("A kivalasztott tool definicio nem talalhato.");
    }

    masodik::api::ToolCall tool_call;
    tool_call.id = "call_" + tool_definition->function.name + "_0";
    tool_call.type = tool_definition->type;
    tool_call.index = 0;
    tool_call.function.name = tool_definition->function.name;
    tool_call.function.arguments = synthesize_tool_arguments(*tool_definition, normalized_last_message, last_message);

    ChatGenerationResult result;
    result.tool_calls.push_back(std::move(tool_call));
    result.finish_reason = "tool_calls";
    return result;
}

std::vector<StreamingChunk> build_tool_call_stream_chunks(const std::vector<masodik::api::ToolCall>& tool_calls) {
    std::vector<StreamingChunk> chunks;
    for (const auto& tool_call : tool_calls) {
        chunks.push_back(
            {.tool_call_deltas = {{.index = tool_call.index,
                                   .id = tool_call.id,
                                   .type = tool_call.type,
                                   .function = {.name = tool_call.function.name}}},
             .terminal = false});

        for (const auto& arguments_chunk : split_stream_chunks(tool_call.function.arguments)) {
            chunks.push_back(
                {.tool_call_deltas = {{.index = tool_call.index, .function = {.arguments = arguments_chunk}}},
                 .terminal = false});
        }
    }
    return chunks;
}

}  // namespace

masodik::api::RequestDecisionPath classify_request_decision_path(const masodik::api::ChatCompletionRequest& request) {
    if (request.tools.empty()) {
        return masodik::api::RequestDecisionPath::content_only;
    }

    switch (request.tool_choice.kind) {
    case masodik::api::ToolChoiceKind::none:
        return masodik::api::RequestDecisionPath::content_only;
    case masodik::api::ToolChoiceKind::required:
        return masodik::api::RequestDecisionPath::tool_call_required;
    case masodik::api::ToolChoiceKind::named_function:
        return masodik::api::RequestDecisionPath::tool_call_named;
    case masodik::api::ToolChoiceKind::auto_select: {
        const auto normalized_last_message = lowercase_ascii(find_last_user_message(request));
        if (contains_any(
                normalized_last_message,
                {"tool",
                 "parancs",
                 "pr",
                 "status",
                 "statusz",
                 "branch",
                 "commit",
                 "repo",
                 "workspace",
                 "agent.md",
                 "file",
                 "files",
                 "directory",
                 "folder",
                 "memory",
                 "ssh",
                 "shell",
                 "terminal",
                 "read",
                 "check",
                 "futtasd",
                 "deritsd",
                 "sorold",
                 "web",
                 "internet",
                 "online",
                 "search"})) {
            return masodik::api::RequestDecisionPath::tool_call_auto;
        }
        return masodik::api::RequestDecisionPath::content_only;
    }
    }

    return masodik::api::RequestDecisionPath::unknown;
}

struct LlmPipelineService::Impl {
#if MASODIK_HAS_OPENVINO_GENAI
    struct SplitCompiledStage {
        std::filesystem::path model_xml_path;
        std::unique_ptr<ov::CompiledModel> compiled_model;
        std::vector<std::string> execution_devices;
    };

    std::unique_ptr<ov::Core> core;
    std::unique_ptr<ov::genai::LLMPipeline> pipeline;
    std::unique_ptr<ov::genai::Tokenizer> split_tokenizer;
    SplitCompiledStage split_stage_1;
    SplitCompiledStage split_stage_2;
#endif
    masodik::diagnostics::QueueMetrics split_queue_metrics;
    std::unique_ptr<masodik::runtime::RequestScheduler> split_handoff_scheduler;
};

LlmPipelineService::LlmPipelineService(masodik::runtime::ServiceConfig service_config)
    : service_config_(std::move(service_config)), impl_(std::make_unique<Impl>()) {
    if (service_config_.split_mode) {
        split_stage_1_.stage_id = SplitExecutionStageId::stage_1;
        split_stage_1_.device_id = service_config_.generation_config.stage_1_device;
        split_stage_1_.memory_budget_gb = service_config_.generation_config.stage_1_memory_budget_gb;
        split_stage_1_.load_state = SplitStageState::loading;
        split_stage_2_.stage_id = SplitExecutionStageId::stage_2;
        split_stage_2_.device_id = service_config_.generation_config.stage_2_device;
        split_stage_2_.memory_budget_gb = service_config_.generation_config.stage_2_memory_budget_gb;
        split_stage_2_.load_state = SplitStageState::loading;
        split_memory_summary_.stage_1 =
            "budget:" + std::to_string(service_config_.generation_config.stage_1_memory_budget_gb) + "GB";
        split_memory_summary_.stage_2 =
            "budget:" + std::to_string(service_config_.generation_config.stage_2_memory_budget_gb) + "GB";
        split_memory_summary_.system = "bounded";
        impl_->split_handoff_scheduler = std::make_unique<masodik::runtime::RequestScheduler>(
            service_config_.serving_profile.queue_policy,
            impl_->split_queue_metrics,
            1U);
    }
}

LlmPipelineService::~LlmPipelineService() = default;

LlmPipelineService::SplitStartupPlan LlmPipelineService::build_split_startup_plan() const {
    const auto& manifest = service_config_.model_repository.split_artifact_manifest;
    return {
        .stage_1 =
            {.stage_id = SplitExecutionStageId::stage_1,
             .device_id = service_config_.generation_config.stage_1_device,
             .artifact_path = resolve_split_path(service_config_.model_repository.split_root_path, manifest.stage_1.artifact_path),
             .memory_budget_gb = service_config_.generation_config.stage_1_memory_budget_gb},
        .stage_2 =
            {.stage_id = SplitExecutionStageId::stage_2,
             .device_id = service_config_.generation_config.stage_2_device,
             .artifact_path = resolve_split_path(service_config_.model_repository.split_root_path, manifest.stage_2.artifact_path),
             .memory_budget_gb = service_config_.generation_config.stage_2_memory_budget_gb},
    };
}

LlmPipelineService::SplitStageLoadResult LlmPipelineService::initialize_split_stage(const SplitStageLoadPlan& load_plan) {
    SplitStageLoadResult result{
        .load_state = SplitStageState::loading,
        .warm_state = SplitStageState::loading,
    };

    const auto stage_name = std::string{split_stage_label(load_plan.stage_id)};
    if (genai_disabled_by_environment()) {
        result.load_state = SplitStageState::ready;
        result.warm_state = SplitStageState::ready;
        result.placeholder_mode = true;
        result.status_message =
            stage_name + " placeholder modban maradt, mert a MASODIK_DISABLE_OPENVINO_GENAI=1 kornyezeti flag aktiv.";
        try_log_runtime_message(
            service_config_,
            masodik::diagnostics::LogLevel::info,
            "manual_split_stage_placeholder profile=" + service_config_.active_profile +
                " stage=" + stage_name +
                " device=" + load_plan.device_id +
                " artifact=\"" + load_plan.artifact_path.string() + "\"");
        return result;
    }

#if MASODIK_HAS_OPENVINO_GENAI
    if (impl_->core == nullptr) {
        impl_->core = std::make_unique<ov::Core>();
    }

    const auto model_xml_path = resolve_split_model_xml_path(load_plan.artifact_path);
    const auto stage_cache_directory = service_config_.generation_config.cache_directory / stage_name;
    std::filesystem::create_directories(stage_cache_directory);

    ov::AnyMap properties = {
        {ov::cache_dir.name(), stage_cache_directory.string()}
    };
    // A KV_CACHE_PRECISION property-t csak stage-1 (dGPU) kapja meg.
    // Az iGPU (stage-2) az alapertelmezett precision-on fut, mert az Intel Arc iGPU
    // a u8 precision-nal nagyon lassu JIT code path-ra kerulhet, ami startup stall-t okoz.
    if (load_plan.stage_id == SplitExecutionStageId::stage_1) {
        apply_kv_cache_precision_property(service_config_, properties);
    }

    try_log_runtime_message(
        service_config_,
        masodik::diagnostics::LogLevel::info,
        "manual_split_stage_compile_begin profile=" + service_config_.active_profile +
            " stage=" + stage_name +
            " device=" + load_plan.device_id +
            " model_xml=\"" + model_xml_path.string() + "\"" +
            " cache_dir=\"" + stage_cache_directory.string() + "\"" +
            " properties=\"" + summarize_property_names(properties) + "\"");
    auto compiled_model = std::make_unique<ov::CompiledModel>(
        impl_->core->compile_model(model_xml_path, load_plan.device_id, properties));
    auto execution_devices = compiled_model->get_property(ov::execution_devices);
    const auto execution_device_summary = join_strings(execution_devices);
    try_log_runtime_message(
        service_config_,
        masodik::diagnostics::LogLevel::info,
        "manual_split_stage_compile_ready profile=" + service_config_.active_profile +
            " stage=" + stage_name +
            " device=" + load_plan.device_id +
            " execution_devices=\"" + execution_device_summary + "\"");
    const auto expected_device_found =
        std::find(execution_devices.begin(), execution_devices.end(), load_plan.device_id) != execution_devices.end();
    const auto cpu_fallback_found = std::any_of(execution_devices.begin(), execution_devices.end(), [](const std::string& device) {
        return is_cpu_device(device);
    });
    if (!expected_device_found || cpu_fallback_found) {
        split_fallback_detected_ = true;
        split_last_failure_scope_ = load_plan.stage_id == SplitExecutionStageId::stage_1
            ? masodik::runtime::RuntimeFailureScope::stage_1
            : masodik::runtime::RuntimeFailureScope::stage_2;
        throw std::runtime_error(
            "A manual split " + stage_name + " compiled device mismatch: requested=" + load_plan.device_id +
            " assigned=" + execution_device_summary);
    }
    auto& compiled_stage =
        load_plan.stage_id == SplitExecutionStageId::stage_1 ? impl_->split_stage_1 : impl_->split_stage_2;
    compiled_stage.model_xml_path = model_xml_path;
    compiled_stage.compiled_model = std::move(compiled_model);
    compiled_stage.execution_devices = std::move(execution_devices);
    if (load_plan.stage_id == SplitExecutionStageId::stage_1) {
        split_stage_1_execution_devices_summary_ = execution_device_summary;
    } else {
        split_stage_2_execution_devices_summary_ = execution_device_summary;
    }

    result.load_state = SplitStageState::ready;
    result.warm_state = SplitStageState::degraded;
    result.status_message = stage_name + " compiled-model inicializacio sikeres; a warmup kesleltetve, elso kereskor tortenik.";
    try_log_runtime_message(
        service_config_,
        masodik::diagnostics::LogLevel::info,
        "manual_split_stage_ready profile=" + service_config_.active_profile +
            " stage=" + stage_name +
            " device=" + load_plan.device_id +
            " artifact=\"" + load_plan.artifact_path.string() + "\"" +
            " model_xml=\"" + model_xml_path.string() + "\"" +
            " cache_dir=\"" + stage_cache_directory.string() + "\"" +
            " warmup_infer=\"deferred\"");
    return result;
#else
    throw std::runtime_error("A build nem tartalmazza az OpenVINO GenAI runtime-ot a manual split stage-loaderhez.");
#endif
}

void LlmPipelineService::initialize() {
    initialized_ = true;
    device_profile_evaluation_ = evaluate_device_profile(service_config_);

    try {
        service_config_.validate();
        startup_state_machine_.transition_to(
            masodik::runtime::RuntimeState::configured,
            "A szolgaltatas konfiguracioja ervenyes, a startup validacio indul.");
        startup_state_machine_.transition_to(
            masodik::runtime::RuntimeState::starting,
            "A dependency validacio es a pipeline inicializacio folyamatban van.");
    } catch (const std::exception& error) {
        ready_ = false;
        status_message_ = error.what();
        startup_state_machine_.transition_to(
            masodik::runtime::RuntimeState::failed,
            status_message_,
            status_message_.find("modell") != std::string::npos ? masodik::runtime::RuntimeError::missing_model
                                                                : masodik::runtime::RuntimeError::invalid_configuration);
        state_ = std::string{masodik::runtime::to_string(startup_state_machine_.current_state())};
        return;
    }

    const auto dependency_validation = dependency_validator_.validate(service_config_);
    if (!dependency_validation.valid) {
        ready_ = false;
        status_message_ = join_notes(dependency_validation.notes);
        split_last_failure_scope_ = dependency_validation.failure_scope;
        if (service_config_.split_mode) {
            if (split_last_failure_scope_ == masodik::runtime::RuntimeFailureScope::stage_1) {
                split_stage_1_.load_state = SplitStageState::failed;
            } else if (split_last_failure_scope_ == masodik::runtime::RuntimeFailureScope::stage_2) {
                split_stage_2_.load_state = SplitStageState::failed;
            }
        }
        startup_state_machine_.transition_to(
            masodik::runtime::RuntimeState::failed,
            status_message_,
            dependency_validation.error);
        state_ = std::string{masodik::runtime::to_string(startup_state_machine_.current_state())};
        return;
    }

    if (service_config_.split_mode) {
        const auto split_startup_plan = build_split_startup_plan();
        split_stage_1_.artifact_path = split_startup_plan.stage_1.artifact_path;
        split_stage_2_.artifact_path = split_startup_plan.stage_2.artifact_path;

        if (service_config_.generation_config.stage_1_device == service_config_.generation_config.stage_2_device ||
            is_cpu_device(service_config_.generation_config.stage_1_device) ||
            is_cpu_device(service_config_.generation_config.stage_2_device)) {
            ready_ = false;
            split_fallback_detected_ = true;
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::startup;
            split_stage_1_.load_state = SplitStageState::failed;
            split_stage_2_.load_state = SplitStageState::failed;
            status_message_ = "A manual split startup ket kulon GPU stage-et igenyel, CPU vagy azonos device nem megengedett.";
            startup_state_machine_.transition_to(
                masodik::runtime::RuntimeState::failed,
                status_message_,
                masodik::runtime::RuntimeError::split_fallback_detected);
            state_ = std::string{masodik::runtime::to_string(startup_state_machine_.current_state())};
            return;
        }

        try {
            const auto stage_1_result = initialize_split_stage(split_startup_plan.stage_1);
            split_stage_1_.load_state = stage_1_result.load_state;
            split_stage_1_.warm_state = stage_1_result.warm_state;
            split_stage_1_.placeholder_mode = stage_1_result.placeholder_mode;
            split_stage_1_.last_error_message = stage_1_result.placeholder_mode ? stage_1_result.status_message : std::string{};

            const auto stage_2_result = initialize_split_stage(split_startup_plan.stage_2);
            split_stage_2_.load_state = stage_2_result.load_state;
            split_stage_2_.warm_state = stage_2_result.warm_state;
            split_stage_2_.placeholder_mode = stage_2_result.placeholder_mode;
            split_stage_2_.last_error_message = stage_2_result.placeholder_mode ? stage_2_result.status_message : std::string{};

            split_handoff_.state = SplitHandoffState::transferred;
            split_memory_summary_.system =
                (stage_1_result.placeholder_mode || stage_2_result.placeholder_mode) ? "validated;placeholder" : "bounded;compiled";

#if MASODIK_HAS_OPENVINO_GENAI
            impl_->split_tokenizer.reset();
            if (!stage_1_result.placeholder_mode && !stage_2_result.placeholder_mode) {
                const auto& split_root = service_config_.model_repository.split_root_path;
                if (split_tokenizer_models_partially_present(split_root)) {
                    throw std::runtime_error(
                        "A split artifact tokenizer modellkeszlete serult: az openvino_tokenizer.xml es openvino_detokenizer.xml csak parban ervenyes.");
                }
                if (split_tokenizer_models_present(split_root)) {
                    impl_->split_tokenizer = std::make_unique<ov::genai::Tokenizer>(split_root);
                    try_log_runtime_message(
                        service_config_,
                        masodik::diagnostics::LogLevel::info,
                        "manual_split_tokenizer_ready profile=" + service_config_.active_profile +
                            " split_root=\"" + split_root.string() + "\"");
                } else {
                    try_log_runtime_message(
                        service_config_,
                        masodik::diagnostics::LogLevel::warning,
                        "manual_split_tokenizer_missing profile=" + service_config_.active_profile +
                            " split_root=\"" + split_root.string() +
                            "\" reason=\"Nincs openvino_tokenizer/openvino_detokenizer par, a runtime technikai fallback valaszt adhat a tesztfixture-oknal.\"");
                }
            }
#endif

            ready_ = true;
            device_profile_evaluation_.gpu_offload_active =
                !stage_1_result.placeholder_mode && !stage_2_result.placeholder_mode;
            status_message_ = "A manual split startup ervenyes.";
            if (stage_1_result.placeholder_mode || stage_2_result.placeholder_mode) {
                status_message_ +=
                    " A stage-szerzodes, device-hozzarendeles es loader-vaz ervenyes, de a tenyleges OpenVINO stage-loader placeholder modban maradt.";
            } else {
                status_message_ +=
                    " A stage-1 es stage-2 compiled modelje sikeresen inicializalodott; a split infer-request letrehozas es warmup az elso keresre van halasztva.";
#if MASODIK_HAS_OPENVINO_GENAI
                if (impl_->split_tokenizer != nullptr) {
                    status_message_ += " A split tokenizer/detokenizer is elerheto a valodi szoveges kimenethez.";
                }
#endif
            }
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                "manual_split_startup_ready profile=" + service_config_.active_profile +
                    " devices=" + split_device_summary(service_config_) +
                    " stage_1_artifact=\"" + split_stage_1_.artifact_path.string() + "\"" +
                    " stage_2_artifact=\"" + split_stage_2_.artifact_path.string() + "\"" +
                    " gpu_offload_active=" + std::string{device_profile_evaluation_.gpu_offload_active ? "true" : "false"} +
                    " memory_summary=\"" + split_memory_summary_.system + "\"");
            startup_state_machine_.transition_to(masodik::runtime::RuntimeState::ready, status_message_);
            state_ = std::string{masodik::runtime::to_string(startup_state_machine_.current_state())};
            return;
        } catch (const std::exception& error) {
            ready_ = false;
            split_last_failure_scope_ =
                split_stage_1_.load_state != SplitStageState::ready ? masodik::runtime::RuntimeFailureScope::stage_1
                                                                    : masodik::runtime::RuntimeFailureScope::stage_2;
            if (split_last_failure_scope_ == masodik::runtime::RuntimeFailureScope::stage_1) {
                split_stage_1_.load_state = SplitStageState::failed;
                split_stage_1_.warm_state = SplitStageState::failed;
                split_stage_1_.last_error_message = error.what();
            } else {
                split_stage_2_.load_state = SplitStageState::failed;
                split_stage_2_.warm_state = SplitStageState::failed;
                split_stage_2_.last_error_message = error.what();
            }
            split_handoff_.state = SplitHandoffState::failed;
            status_message_ = "A manual split stage inicializacio sikertelen: " + std::string{error.what()};
            startup_state_machine_.transition_to(
                masodik::runtime::RuntimeState::failed,
                status_message_,
                masodik::runtime::RuntimeError::split_stage_configuration);
            state_ = std::string{masodik::runtime::to_string(startup_state_machine_.current_state())};
            return;
        }
    }

#if MASODIK_HAS_OPENVINO_GENAI
    if (!genai_disabled_by_environment()) {
        try {
            std::filesystem::create_directories(service_config_.generation_config.cache_directory);
            const auto openvino_log_level = openvino_log_level_from_environment_or_profile(service_config_);

            ov::AnyMap properties = {
                {ov::cache_dir.name(), service_config_.generation_config.cache_directory.string()}
            };
            apply_kv_cache_precision_property(service_config_, properties);

            if (!is_hetero_device(service_config_.effective_device)) {
                ov::genai::SchedulerConfig scheduler_config;
                scheduler_config.max_num_seqs =
                    static_cast<std::size_t>(std::max(1, service_config_.serving_profile.max_num_seqs));
                scheduler_config.max_num_batched_tokens = static_cast<std::size_t>(
                    std::max(1, service_config_.generation_config.desired_context_tokens));
                scheduler_config.enable_prefix_caching = service_config_.serving_profile.prefix_caching_enabled;
                properties[ov::genai::scheduler_config.name()] = scheduler_config;
            } else {
                // HETERO eszkozokon a ContinuousBatchingAdapter (PagedAttention) nem tamogatott,
                // es scheduler_config nelkul a GenAI alapertelmezetten KORLATLAN KV-cache-t
                // allokalna (max_num_batched_tokens = SIZE_MAX). Az SDPA backend kikenyszeritese
                // stateful pipeline-t hasznal, ami kezelhetobb memoriafogyasztast eredmenyez.
                properties["ATTENTION_BACKEND"] = std::string{"SDPA"};
                try_log_runtime_message(
                    service_config_,
                    masodik::diagnostics::LogLevel::info,
                    "genai_hetero_sdpa_forced profile=" + service_config_.active_profile +
                        " effective_device=" + service_config_.effective_device +
                        " reason=\"HETERO nem tamogatja a ContinuousBatchingAdapter-t, SDPA stateful pipeline-t hasznalunk a korlatlan KV-cache allokacio elkerulesehez\"");
            }

            // PIPELINE_PARALLEL elosztasi politika bekotese, ha a profil kerelmezi (pl. igpu-parallel vagy cpu-hetero)
            if (service_config_.serving_profile.model_distribution_policy == "PIPELINE_PARALLEL") {
                properties[ov::hint::model_distribution_policy.name()] =
                    std::set<ov::hint::ModelDistributionPolicy>{ov::hint::ModelDistributionPolicy::PIPELINE_PARALLEL};
            }

            impl_->pipeline = std::make_unique<ov::genai::LLMPipeline>(
                service_config_.model_repository.root_path,
                service_config_.effective_device,
                properties);
            schedule_runtime_assignment_diagnostics(service_config_, properties);

            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                "genai_pipeline_init profile=" + service_config_.active_profile +
                    " effective_device=" + service_config_.effective_device +
                    " desired_context_tokens=" + std::to_string(service_config_.generation_config.desired_context_tokens) +
                    " max_num_seqs=" + std::to_string(service_config_.serving_profile.max_num_seqs) +
                    " prefix_caching=" + std::string{service_config_.serving_profile.prefix_caching_enabled ? "true" : "false"} +
                    " attention_backend=" +
                    std::string{is_hetero_device(service_config_.effective_device) ? "SDPA" : "PA"} +
                    " scheduler_config_enabled=" +
                    std::string{is_hetero_device(service_config_.effective_device) ? "false" : "true"} +
                    " assignment_diagnostics_enabled=" +
                    std::string{runtime_assignment_diagnostics_requested(service_config_) ? "true" : "false"} +
                    " pipeline_parallel_requested=" +
                    std::string{pipeline_parallel_requested(service_config_) ? "true" : "false"} +
                    " pipeline_properties=\"" + summarize_property_names(properties) + "\"" +
                    " openvino_log_level=" +
#if MASODIK_HAS_OPENVINO_GENAI
                    to_string(openvino_log_level)
#else
                    std::string{"disabled"}
#endif
            );

            ready_ = true;
            status_message_ = "A valodi OpenVINO GenAI pipeline sikeresen inicializalodott.";
            device_profile_evaluation_.gpu_offload_active = device_profile_evaluation_.gpu_offload_expected;
            startup_state_machine_.transition_to(masodik::runtime::RuntimeState::ready, status_message_);
            state_ = std::string{masodik::runtime::to_string(startup_state_machine_.current_state())};
            return;
        } catch (const std::exception& error) {
            ready_ = false;
            status_message_ = std::string{"Az OpenVINO GenAI pipeline inicializalasa sikertelen: "} + error.what();
            device_profile_evaluation_.gpu_offload_active = false;
            startup_state_machine_.transition_to(
                masodik::runtime::RuntimeState::failed,
                status_message_,
                masodik::runtime::RuntimeError::unsupported_operation);
            state_ = std::string{masodik::runtime::to_string(startup_state_machine_.current_state())};
            return;
        }
    }
#endif

    ready_ = true;
    device_profile_evaluation_.gpu_offload_active = false;
    status_message_ =
        "A HTTP es OpenAI-kompatibilis kiszolgalo reteg aktiv, de a tenyleges OpenVINO GenAI modelhivas "
        "kovetkezo korben kotodik be.";
    if (genai_disabled_by_environment()) {
        status_message_ += " A futas kornyezeti flag miatt placeholder modban maradt.";
    } else if (!service_config_.runtime_dependencies.openvino_genai_import_lib_exists) {
        status_message_ +=
            " A helyi bundle-ben az openvino_genai.lib tovabbra sem talalhato, ezert placeholder pipeline fut.";
    }
    startup_state_machine_.transition_to(masodik::runtime::RuntimeState::degraded, status_message_);
    state_ = std::string{masodik::runtime::to_string(startup_state_machine_.current_state())};
}

bool LlmPipelineService::ready() const noexcept { return ready_; }

const std::string& LlmPipelineService::state() const noexcept { return state_; }

const std::string& LlmPipelineService::status_message() const noexcept { return effective_status_message(); }

const masodik::runtime::ServiceConfig& LlmPipelineService::service_config() const noexcept { return service_config_; }

masodik::runtime::RuntimeError LlmPipelineService::last_error() const noexcept {
    return runtime_failure_detected_ ? runtime_failure_error_ : startup_state_machine_.last_error();
}

void LlmPipelineService::note_runtime_failure(const std::exception& error) const {
    runtime_failure_detected_ = true;
    runtime_failure_error_ = masodik::runtime::RuntimeError::unsupported_operation;
    runtime_failure_state_ = masodik::runtime::RuntimeState::failed;

    runtime_failure_message_ = std::string{"Runtime hiba miatt a pipeline unhealthy allapotba kerult: "} + error.what();
    ready_ = false;
    state_ = std::string{masodik::runtime::to_string(runtime_failure_state_)};
    status_message_ = runtime_failure_message_;
    device_profile_evaluation_.gpu_offload_active = false;

    // T053: split modban a runtime hiba scope-jat a kontextusbol becsulni kell
    if (service_config_.split_mode && split_last_failure_scope_ == masodik::runtime::RuntimeFailureScope::none) {
        const std::string what = error.what();
        if (what.find("stage-1") != std::string::npos || what.find("Stage-1") != std::string::npos) {
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::stage_1;
            split_stage_1_.load_state = SplitStageState::failed;
            split_stage_1_.last_error_message = what;
        } else if (what.find("stage-2") != std::string::npos || what.find("Stage-2") != std::string::npos) {
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::stage_2;
            split_stage_2_.load_state = SplitStageState::failed;
            split_stage_2_.last_error_message = what;
        } else if (what.find("handoff") != std::string::npos || what.find("Handoff") != std::string::npos) {
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::handoff;
            split_handoff_.state = SplitHandoffState::failed;
        }
    }

    if (service_config_.split_mode) {
        if (split_fallback_detected_) {
            runtime_failure_error_ = masodik::runtime::RuntimeError::split_fallback_detected;
        } else if (split_last_failure_scope_ == masodik::runtime::RuntimeFailureScope::handoff) {
            runtime_failure_error_ = masodik::runtime::RuntimeError::split_handoff_failed;
        } else if (std::string_view{error.what()}.find("memory budget") != std::string_view::npos) {
            runtime_failure_error_ = masodik::runtime::RuntimeError::split_memory_budget_exceeded;
        }
    }

    // T054: strukturalt split stage log sor a runtime hibarol
    if (service_config_.split_mode) {
        try_log_runtime_message(
            service_config_,
            masodik::diagnostics::LogLevel::error,
            "manual_split_runtime_failure scope=" +
                std::string{masodik::runtime::to_string(split_last_failure_scope_)} +
                " error=\"" + error.what() + "\"");
    }
}

masodik::runtime::RuntimeState LlmPipelineService::effective_runtime_state() const noexcept {
    return runtime_failure_detected_ ? runtime_failure_state_ : startup_state_machine_.current_state();
}

bool LlmPipelineService::effective_ready() const noexcept {
    return ready_ && !runtime_failure_detected_;
}

const std::string& LlmPipelineService::effective_status_message() const noexcept {
    return runtime_failure_detected_ ? runtime_failure_message_ : status_message_;
}

masodik::api::HealthResponse LlmPipelineService::make_health_response() const {
    auto health_response = readiness_reporter_.make_health_response(service_config_, effective_runtime_state(), effective_ready());
    health_response.effective_device = device_profile_evaluation_.effective_device;
    health_response.gpu_offload_active = device_profile_evaluation_.gpu_offload_active;
    health_response.status_message = effective_status_message();
    if (service_config_.split_mode) {
        health_response.split_mode = true;
        health_response.split_profile = service_config_.split_profile;
        health_response.stage_1_state = std::string{to_string(split_stage_1_.load_state)};
        health_response.stage_2_state = std::string{to_string(split_stage_2_.load_state)};
        health_response.handoff_state = std::string{to_string(split_handoff_.state)};
        health_response.request_path_mode = std::string{masodik::split::to_string(
            masodik::split::infer_split_request_path_mode(
                true,
                split_compiled_request_path_active_,
                split_stage_1_.placeholder_mode,
                split_stage_2_.placeholder_mode,
                runtime_failure_detected_))};
        health_response.stage_1_device = split_stage_1_.device_id;
        health_response.stage_2_device = split_stage_2_.device_id;
        health_response.stage_1_execution_devices = split_stage_1_execution_devices_summary_;
        health_response.stage_2_execution_devices = split_stage_2_execution_devices_summary_;
        health_response.split_artifact_version = service_config_.model_repository.split_artifact_manifest.artifact_version;
        health_response.validated_context_tokens = service_config_.model_repository.split_artifact_manifest.validated_context_tokens;
        health_response.last_split_request_id = split_handoff_.request_id;
        health_response.last_split_sequence_id = split_handoff_.sequence_id;
        health_response.last_failure_scope = std::string{masodik::runtime::to_string(split_last_failure_scope_)};
        health_response.fallback_detected = split_fallback_detected_;
        health_response.memory_summary = {.stage_1 = split_memory_summary_.stage_1,
                                          .stage_2 = split_memory_summary_.stage_2,
                                          .system = split_memory_summary_.system};

        // T051/T054: last-failure summary osszefoglalas a diagnosztikai payloadban
        if (split_last_failure_scope_ != masodik::runtime::RuntimeFailureScope::none) {
            std::string summary = "scope=" + std::string{masodik::runtime::to_string(split_last_failure_scope_)};
            if (!split_stage_1_.last_error_message.empty()) {
                summary += " stage_1_error=\"" + split_stage_1_.last_error_message + "\"";
            }
            if (!split_stage_2_.last_error_message.empty()) {
                summary += " stage_2_error=\"" + split_stage_2_.last_error_message + "\"";
            }
            if (split_fallback_detected_) {
                summary += " fallback=true";
            }
            health_response.last_failure_summary = std::move(summary);
        }

        // T055: split runtime hiba utan a ready kenyszeritetten false
        if (split_stage_1_.load_state == SplitStageState::failed ||
            split_stage_2_.load_state == SplitStageState::failed ||
            split_fallback_detected_) {
            health_response.ready = false;
            if (health_response.state != "failed") {
                health_response.state = "failed";
            }
        }
    }
    return health_response;
}

std::vector<masodik::api::ModelCard> LlmPipelineService::list_models() const {
    return {{.id = service_config_.active_model,
             .object = "model",
             .owned_by = "local",
             .split_mode = service_config_.split_mode,
             .split_profile = service_config_.split_profile}};
}

masodik::api::ChatCompletionResponse LlmPipelineService::generate_chat_completion(
    const masodik::api::ChatCompletionRequest& request) const {
    const auto result = generate_chat_result(request);

    masodik::api::ChatCompletionResponse response;
    response.id = "chatcmpl-local-genai";
    response.model = service_config_.active_model;
    response.choices.push_back(
        {.index = 0,
         .message = {.role = "assistant", .content = result.content, .tool_calls = result.tool_calls},
         .finish_reason = result.finish_reason});
    return response;
}

ChatGenerationResult LlmPipelineService::generate_chat_result(const masodik::api::ChatCompletionRequest& request) const {
    return generate_chat_result_impl(request, nullptr, nullptr);
}

ChatGenerationResult LlmPipelineService::generate_chat_result_impl(
    const masodik::api::ChatCompletionRequest& request,
    StreamingChunkCallback on_chunk,
    StreamingCancellationCheck should_cancel) const {
    if (!initialized_) {
        throw std::logic_error("A pipeline nincs inicializalva.");
    }
    if (!effective_ready()) {
        throw std::runtime_error(effective_status_message());
    }
    if (request.model != service_config_.active_model) {
        throw std::invalid_argument("Az aktiv helyi modelltol eltero modelazonosito erkezett.");
    }

    const auto tool_call_result = build_tool_call_result(request);
    if (!tool_call_result.tool_calls.empty()) {
        return tool_call_result;
    }

    if (generate_failure_mode_from_environment() == "runtime_error") {
        const std::runtime_error forced_error{"Tesztcelu runtime hiba a generalasi utvonalon."};
        note_runtime_failure(forced_error);
        throw forced_error;
    }

    auto prompt_tokens_estimate = estimate_request_prompt_tokens(request);
    std::optional<ChatPromptBuildResult> split_prompt_build;
    std::optional<masodik::runtime::RequestScheduler::HandoffLease> split_handoff_lease;

    if (service_config_.split_mode && impl_->split_tokenizer != nullptr) {
        split_prompt_build.emplace(build_chat_prompt_for_split(request, *impl_->split_tokenizer));
        try {
            const auto encoded_prompt = impl_->split_tokenizer->encode(split_prompt_build->prompt);
            const auto actual_prompt_tokens = tensor_sequence_length(encoded_prompt.input_ids);
            if (actual_prompt_tokens > 0U) {
                prompt_tokens_estimate = actual_prompt_tokens;
            }
        } catch (const std::exception& error) {
            if (split_prompt_build->error.empty()) {
                split_prompt_build->error = error.what();
            }
        }
    }

    const auto total_tokens_estimate =
        prompt_tokens_estimate + static_cast<std::size_t>(std::max(1, request.max_tokens));

    if (service_config_.split_mode) {
        const auto runtime_request_id = split_runtime_request_counter_.fetch_add(1);
        const auto split_sequence_id = next_split_sequence_id();
        if (impl_->split_handoff_scheduler == nullptr) {
            split_handoff_.state = SplitHandoffState::failed;
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::handoff;
            throw std::runtime_error("A split handoff scheduler nem inicializalodott.");
        }
        try {
            split_handoff_lease.emplace(impl_->split_handoff_scheduler->acquire_handoff(runtime_request_id, split_sequence_id));
        } catch (const std::exception& error) {
            split_handoff_.state = SplitHandoffState::failed;
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::handoff;
            throw std::runtime_error(std::string{"A split handoff foglalas sikertelen: "} + error.what());
        }

        split_handoff_.request_id = split_runtime_request_id(split_handoff_lease->request_id());
        split_handoff_.sequence_id = split_handoff_lease->sequence_id();
        split_handoff_.state = SplitHandoffState::created;
        split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::none;
        const auto split_context_cap = service_config_.generation_config.split_context_token_cap(
            service_config_.model_repository.split_artifact_manifest.max_context_tokens);
        try_log_runtime_message(
            service_config_,
            masodik::diagnostics::LogLevel::info,
            "manual_split_request_begin profile=" + service_config_.active_profile +
                " request_id=" + split_handoff_.request_id +
                " sequence_id=" + split_handoff_.sequence_id +
                " prompt_tokens=" + std::to_string(prompt_tokens_estimate) +
                " total_tokens=" + std::to_string(total_tokens_estimate) +
                " context_cap=" + std::to_string(split_context_cap));

        const auto handoff_contract = masodik::split::build_split_handoff_contract(
            service_config_.model_repository.split_artifact_manifest,
            service_config_.generation_config);
        try {
            masodik::split::validate_split_handoff_contract(handoff_contract);
        } catch (const std::exception& error) {
            split_handoff_.state = SplitHandoffState::failed;
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::handoff;
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::error,
                "manual_split_handoff_contract_failed profile=" + service_config_.active_profile +
                    " request_id=" + split_handoff_.request_id +
                    " sequence_id=" + split_handoff_.sequence_id +
                    " stage_1_device=" + service_config_.generation_config.stage_1_device +
                    " stage_2_device=" + service_config_.generation_config.stage_2_device +
                    " error=\"" + std::string{error.what()} + "\"");
            throw;
        }

        if (total_tokens_estimate > split_context_cap) {
            split_handoff_.state = SplitHandoffState::failed;
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::handoff;
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::warning,
                "manual_split_guard_reject profile=" + service_config_.active_profile +
                    " request_id=" + split_handoff_.request_id +
                    " sequence_id=" + split_handoff_.sequence_id +
                    " scope=handoff reason=context_budget total_tokens=" + std::to_string(total_tokens_estimate) +
                    " cap=" + std::to_string(split_context_cap));
            throw std::runtime_error(
                "A split long-context keres meghaladja a validalt context budgetet: total_tokens=" +
                std::to_string(total_tokens_estimate) + " cap=" + std::to_string(split_context_cap));
        }

        const auto& manifest = service_config_.model_repository.split_artifact_manifest;
        const auto stage_1_budget_cap_gb = service_config_.generation_config.split_stage_budget_cap_gb(
            service_config_.generation_config.stage_1_memory_budget_gb,
            manifest.stage_1.kv_budget_gb);
        const auto stage_2_budget_cap_gb = service_config_.generation_config.split_stage_budget_cap_gb(
            service_config_.generation_config.stage_2_memory_budget_gb,
            manifest.stage_2.kv_budget_gb);
        const auto stage_1_required_gb = service_config_.generation_config.split_stage_required_budget_gb(
            total_tokens_estimate,
            split_context_cap,
            manifest.stage_1.kv_budget_gb);
        const auto stage_2_required_gb = service_config_.generation_config.split_stage_required_budget_gb(
            total_tokens_estimate,
            split_context_cap,
            manifest.stage_2.kv_budget_gb);
        split_memory_summary_.stage_1 =
            "required:" + std::to_string(stage_1_required_gb) + "GB budget:" +
            std::to_string(stage_1_budget_cap_gb) + "GB";
        split_memory_summary_.stage_2 =
            "required:" + std::to_string(stage_2_required_gb) + "GB budget:" +
            std::to_string(stage_2_budget_cap_gb) + "GB";
        split_memory_summary_.system =
            "prompt_tokens=" + std::to_string(prompt_tokens_estimate) +
            ";total_tokens=" + std::to_string(total_tokens_estimate) +
            ";handoff_sequence=" + split_handoff_.sequence_id;

        if (stage_1_required_gb > stage_1_budget_cap_gb) {
            split_handoff_.state = SplitHandoffState::failed;
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::stage_1;
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::warning,
                "manual_split_guard_reject profile=" + service_config_.active_profile +
                    " request_id=" + split_handoff_.request_id +
                    " sequence_id=" + split_handoff_.sequence_id +
                    " scope=stage-1 reason=memory_budget required_gb=" + std::to_string(stage_1_required_gb) +
                    " budget_gb=" + std::to_string(stage_1_budget_cap_gb));
            throw std::runtime_error(
                "A split stage-1 memory budget tullepve: required=" + std::to_string(stage_1_required_gb) +
                "GB budget=" + std::to_string(stage_1_budget_cap_gb) + "GB");
        }
        if (stage_2_required_gb > stage_2_budget_cap_gb) {
            split_handoff_.state = SplitHandoffState::failed;
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::stage_2;
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::warning,
                "manual_split_guard_reject profile=" + service_config_.active_profile +
                    " request_id=" + split_handoff_.request_id +
                    " sequence_id=" + split_handoff_.sequence_id +
                    " scope=stage-2 reason=memory_budget required_gb=" + std::to_string(stage_2_required_gb) +
                    " budget_gb=" + std::to_string(stage_2_budget_cap_gb));
            throw std::runtime_error(
                "A split stage-2 memory budget tullepve: required=" + std::to_string(stage_2_required_gb) +
                "GB budget=" + std::to_string(stage_2_budget_cap_gb) + "GB");
        }

        try_log_runtime_message(
            service_config_,
            masodik::diagnostics::LogLevel::info,
            "manual_split_stage_transition profile=" + service_config_.active_profile +
                " request_id=" + split_handoff_.request_id +
                " sequence_id=" + split_handoff_.sequence_id +
                " stage=stage-1 phase=produce_hidden_state device=" + split_stage_1_.device_id +
                " model_xml=\"" + impl_->split_stage_1.model_xml_path.string() + "\"" +
                " execution_devices=\"" + join_strings(impl_->split_stage_1.execution_devices) + "\"" +
                " hidden_state_contract=\"" + manifest.hidden_state_contract + "\"" +
                " kv_contract=\"" + manifest.kv_contract + "\"" +
                " required_gb=" + std::to_string(stage_1_required_gb) +
                " budget_gb=" + std::to_string(stage_1_budget_cap_gb));
        split_handoff_.state = SplitHandoffState::transferred;
        try_log_runtime_message(
            service_config_,
            masodik::diagnostics::LogLevel::info,
            "manual_split_handoff_transfer profile=" + service_config_.active_profile +
                " request_id=" + split_handoff_.request_id +
                " sequence_id=" + split_handoff_.sequence_id +
                " producer=stage-1 consumer=stage-2 state=ready hidden_state_contract=\"" +
                manifest.hidden_state_contract + "\" kv_contract=\"" + manifest.kv_contract + "\"");
        try_log_runtime_message(
            service_config_,
            masodik::diagnostics::LogLevel::info,
            "manual_split_stage_transition profile=" + service_config_.active_profile +
                " request_id=" + split_handoff_.request_id +
                " sequence_id=" + split_handoff_.sequence_id +
                " stage=stage-2 phase=consume_hidden_state device=" + split_stage_2_.device_id +
                " model_xml=\"" + impl_->split_stage_2.model_xml_path.string() + "\"" +
                " execution_devices=\"" + join_strings(impl_->split_stage_2.execution_devices) + "\"" +
                " hidden_state_contract=\"" + manifest.hidden_state_contract + "\"" +
                " kv_contract=\"" + manifest.kv_contract + "\"" +
                " required_gb=" + std::to_string(stage_2_required_gb) +
                " budget_gb=" + std::to_string(stage_2_budget_cap_gb));
    }

#if MASODIK_HAS_OPENVINO_GENAI
    if (service_config_.split_mode &&
        !split_stage_1_.placeholder_mode &&
        !split_stage_2_.placeholder_mode) {
        const auto last_message = find_last_user_message(request);
        const auto& manifest = service_config_.model_repository.split_artifact_manifest;
        try {
            if (impl_->split_stage_1.compiled_model == nullptr || impl_->split_stage_2.compiled_model == nullptr) {
                split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::handoff;
                throw std::runtime_error("A compiled split request-pathhoz mindket stage compiled_model kotelezo.");
            }

            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                "manual_split_request_infer_request_begin profile=" + service_config_.active_profile +
                    " request_id=" + split_handoff_.request_id +
                    " sequence_id=" + split_handoff_.sequence_id +
                    " stage=stage-1" +
                    " device=\"" + split_stage_1_.device_id + "\"");
            auto stage_1_request = impl_->split_stage_1.compiled_model->create_infer_request();
            stage_1_request.reset_state();
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                "manual_split_request_infer_request_ready profile=" + service_config_.active_profile +
                    " request_id=" + split_handoff_.request_id +
                    " sequence_id=" + split_handoff_.sequence_id +
                    " stage=stage-1" +
                    " device=\"" + split_stage_1_.device_id + "\"");
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                "manual_split_request_infer_request_begin profile=" + service_config_.active_profile +
                    " request_id=" + split_handoff_.request_id +
                    " sequence_id=" + split_handoff_.sequence_id +
                    " stage=stage-2" +
                    " device=\"" + split_stage_2_.device_id + "\"");
            auto stage_2_request = impl_->split_stage_2.compiled_model->create_infer_request();
            stage_2_request.reset_state();
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                "manual_split_request_infer_request_ready profile=" + service_config_.active_profile +
                    " request_id=" + split_handoff_.request_id +
                    " sequence_id=" + split_handoff_.sequence_id +
                    " stage=stage-2" +
                    " device=\"" + split_stage_2_.device_id + "\"");

            auto stage_1_output_port =
                find_compiled_output_port(*impl_->split_stage_1.compiled_model, manifest.hidden_state_contract);
            if (!stage_1_output_port.has_value() && !impl_->split_stage_1.compiled_model->outputs().empty()) {
                stage_1_output_port = impl_->split_stage_1.compiled_model->outputs().front();
            }
            if (!stage_1_output_port.has_value()) {
                split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::stage_1;
                throw std::runtime_error("A Stage-1 compiled modellnek nincs olvashato output tensorja.");
            }

            const auto stage_1_output_name = compiled_port_name(*stage_1_output_port);
            if (stage_1_output_name != manifest.hidden_state_contract) {
                try_log_runtime_message(
                    service_config_,
                    masodik::diagnostics::LogLevel::warning,
                    "manual_split_contract_mismatch profile=" + service_config_.active_profile +
                        " stage=stage-1 manifest_hidden_state_contract=\"" + manifest.hidden_state_contract +
                        "\" actual_output=\"" + stage_1_output_name + "\"");
            }
            auto stage_2_handoff_input =
                find_compiled_input_port(*impl_->split_stage_2.compiled_model, "hidden_states_from_stage_1");
            if (!stage_2_handoff_input.has_value()) {
                stage_2_handoff_input =
                    find_compiled_input_port(*impl_->split_stage_2.compiled_model, manifest.stage_2.expected_input);
            }
            if (!stage_2_handoff_input.has_value() && !impl_->split_stage_2.compiled_model->inputs().empty()) {
                stage_2_handoff_input = impl_->split_stage_2.compiled_model->inputs().front();
            }
            if (!stage_2_handoff_input.has_value()) {
                split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::handoff;
                throw std::runtime_error("A Stage-2 compiled modellben nem talalhato handoff input tensor.");
            }

            auto stage_2_output_port = find_compiled_output_port(*impl_->split_stage_2.compiled_model, "logits");
            if (!stage_2_output_port.has_value() && !impl_->split_stage_2.compiled_model->outputs().empty()) {
                stage_2_output_port = impl_->split_stage_2.compiled_model->outputs().front();
            }
            if (!stage_2_output_port.has_value()) {
                split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::stage_2;
                throw std::runtime_error("A Stage-2 compiled modellnek nincs olvashato output tensorja.");
            }

            if (impl_->split_tokenizer != nullptr) {
                constexpr std::size_t split_prefill_chunk_token_limit = 768U;

                const auto prompt_build = split_prompt_build.has_value()
                                              ? *split_prompt_build
                                              : build_chat_prompt_for_split(request, *impl_->split_tokenizer);
                if (!prompt_build.error.empty()) {
                    try_log_runtime_message(
                        service_config_,
                        masodik::diagnostics::LogLevel::warning,
                        "manual_split_chat_template_fallback profile=" + service_config_.active_profile +
                            " request_id=" + split_handoff_.request_id +
                            " sequence_id=" + split_handoff_.sequence_id +
                            " error=\"" + prompt_build.error + "\"");
                }
                try_log_runtime_message(
                    service_config_,
                    masodik::diagnostics::LogLevel::info,
                    "manual_split_prompt_source profile=" + service_config_.active_profile +
                        " request_id=" + split_handoff_.request_id +
                        " sequence_id=" + split_handoff_.sequence_id +
                        " source=" + std::string{prompt_build.used_chat_template ? "chat_template" : "last_user_message"} +
                        " prompt_chars=" + std::to_string(prompt_build.prompt.size()) +
                        " prompt_preview=\"" + truncate_for_log(prompt_build.prompt) + "\"");
                const auto encoded_prompt = impl_->split_tokenizer->encode(prompt_build.prompt);
                const auto prompt_token_count = tensor_sequence_length(encoded_prompt.input_ids);
                if (prompt_token_count == 0U) {
                    split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::handoff;
                    throw std::runtime_error("A split tokenizer ures input_ids tenzort adott vissza.");
                }
                const auto max_context_tokens = static_cast<std::size_t>(
                    std::max(1, service_config_.model_repository.split_artifact_manifest.max_context_tokens));
                if (prompt_token_count >= max_context_tokens) {
                    split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::handoff;
                    throw std::runtime_error(
                        "A split tokenizer altal eloallitott prompt mar a generacio elott kimeriti a context budgetet: prompt_tokens=" +
                        std::to_string(prompt_token_count) + " cap=" + std::to_string(max_context_tokens));
                }

                std::vector<std::int64_t> generated_tokens;
                generated_tokens.reserve(static_cast<std::size_t>(std::max(1, request.max_tokens)));

                const ov::Tensor full_prompt_input_ids = encoded_prompt.input_ids;
                const ov::Tensor full_prompt_attention_mask =
                    tensor_sequence_length(encoded_prompt.attention_mask) > 0U ? encoded_prompt.attention_mask
                                                                              : make_attention_mask_tensor(prompt_token_count);
                const auto stage_2_output_name = compiled_port_name(*stage_2_output_port);
                ov::Tensor current_input_ids = full_prompt_input_ids;
                ov::Tensor current_attention_mask = full_prompt_attention_mask;
                ov::Tensor current_position_ids = make_position_ids_tensor(0U, prompt_token_count);
                auto current_total_tokens = prompt_token_count;
                bool reached_eos = false;
                // A Stage-2 sajat KV cache szamlaloja — a deep split-nel a Stage-2
                // attention retegeket tartalmaz, ezert sajat kumulativ poziciot kell
                // vezetni a helyes attention mask es position_ids eloallitasahoz.
                std::size_t stage_2_kv_processed = 0U;

                auto run_split_step = [&](const ov::Tensor& stage_1_input_ids,
                                          const ov::Tensor& stage_2_input_ids,
                                          const ov::Tensor& step_attention_mask,
                                          const ov::Tensor& step_position_ids) {
                    bind_split_generation_inputs(
                        stage_1_request,
                        *impl_->split_stage_1.compiled_model,
                        stage_1_input_ids,
                        step_attention_mask,
                        step_position_ids);
                    try_log_runtime_message(
                        service_config_,
                        masodik::diagnostics::LogLevel::info,
                        "manual_split_stage1_step_inputs profile=" + service_config_.active_profile +
                            " request_id=" + split_handoff_.request_id +
                            " sequence_id=" + split_handoff_.sequence_id +
                            " input_ids_shape=\"" + masodik::split::describe_tensor_shape(stage_1_input_ids) +
                            "\" attention_mask_shape=\"" + masodik::split::describe_tensor_shape(step_attention_mask) +
                            "\" position_ids_shape=\"" + masodik::split::describe_tensor_shape(step_position_ids) +
                            "\" state_before=\"" + summarize_variable_states(stage_1_request) + "\"");
                    stage_1_request.infer();
                    try_log_runtime_message(
                        service_config_,
                        masodik::diagnostics::LogLevel::info,
                        "manual_split_stage1_step_complete profile=" + service_config_.active_profile +
                            " request_id=" + split_handoff_.request_id +
                            " sequence_id=" + split_handoff_.sequence_id +
                            " state_after=\"" + summarize_variable_states(stage_1_request) + "\"");

                    const auto stage_1_hidden_state = stage_1_request.get_tensor(*stage_1_output_port);
                    auto handoff_buffer = masodik::split::make_ram_backed_handoff_buffer(
                        stage_1_hidden_state,
                        manifest.hidden_state_contract,
                        compiled_port_name(*stage_2_handoff_input));
                    // A deep split-ben a Stage-2 sajat KV cache-t epit (retegenkent fuggetlen).
                    // A Stage-2 attention mask-nak figyelembe kell vennie a Stage-2 KV cache
                    // eddigi merteket + az aktualis chunk meretet. A position_ids az aktualis
                    // chunk poziciojat jelzi a Stage-2 sajat szekvenciajaban.
                    const auto stage_2_chunk_len = tensor_sequence_length(stage_2_input_ids);
                    const auto stage_2_attn_total = stage_2_kv_processed + stage_2_chunk_len;
                    const auto stage_2_attention_mask = make_attention_mask_tensor(stage_2_attn_total);
                    const auto stage_2_position_ids = make_position_ids_tensor(stage_2_kv_processed, stage_2_chunk_len);
                    bind_split_generation_inputs(
                        stage_2_request,
                        *impl_->split_stage_2.compiled_model,
                        stage_2_input_ids,
                        stage_2_attention_mask,
                        stage_2_position_ids);
                    stage_2_request.set_tensor(*stage_2_handoff_input, handoff_buffer.tensor);
                    split_handoff_.state = SplitHandoffState::transferred;
                    try_log_runtime_message(
                        service_config_,
                        masodik::diagnostics::LogLevel::info,
                        "manual_split_stage2_step_inputs profile=" + service_config_.active_profile +
                            " request_id=" + split_handoff_.request_id +
                            " sequence_id=" + split_handoff_.sequence_id +
                            " stage1_input_ids_shape=\"" + masodik::split::describe_tensor_shape(stage_1_input_ids) +
                            "\" stage2_input_ids_shape=\"" + masodik::split::describe_tensor_shape(stage_2_input_ids) +
                            "\" attention_mask_shape=\"" + masodik::split::describe_tensor_shape(step_attention_mask) +
                            "\" position_ids_shape=\"" + masodik::split::describe_tensor_shape(step_position_ids) +
                            "\" hidden_shape=\"" + masodik::split::describe_tensor_shape(handoff_buffer.tensor) +
                            "\" state_before=\"" + summarize_variable_states(stage_2_request) + "\"");
                    try_log_runtime_message(
                        service_config_,
                        masodik::diagnostics::LogLevel::info,
                        "manual_split_handoff_buffer_ready profile=" + service_config_.active_profile +
                            " request_id=" + split_handoff_.request_id +
                            " sequence_id=" + split_handoff_.sequence_id +
                            " source_contract=\"" + handoff_buffer.source_contract + "\"" +
                            " target_tensor=\"" + handoff_buffer.target_tensor_name + "\"" +
                            " shape=\"" + masodik::split::describe_tensor_shape(handoff_buffer.tensor) + "\"" +
                            " bytes=" + std::to_string(handoff_buffer.byte_size) +
                            " fingerprint=" + format_uint64_hex(handoff_buffer.fingerprint));
                    stage_2_request.infer();
                    stage_2_kv_processed += stage_2_chunk_len;
                    try_log_runtime_message(
                        service_config_,
                        masodik::diagnostics::LogLevel::info,
                        "manual_split_stage2_step_complete profile=" + service_config_.active_profile +
                            " request_id=" + split_handoff_.request_id +
                            " sequence_id=" + split_handoff_.sequence_id +
                            " stage_2_kv_processed=" + std::to_string(stage_2_kv_processed) +
                            " state_after=\"" + summarize_variable_states(stage_2_request) + "\"");
                    const auto stage_2_output_tensor = stage_2_request.get_tensor(*stage_2_output_port);
                    const auto stage_2_output_shape = stage_2_output_tensor.get_shape();
                    const auto logits_sequence_index =
                        stage_2_output_shape.size() > 1U && stage_2_output_shape[1] > 0U ? stage_2_output_shape[1] - 1U : 0U;

                    struct StepResult {
                        std::int64_t next_token_id;
                        std::size_t handoff_byte_size;
                    };

                    return StepResult{
                        .next_token_id = select_next_token_id_from_logits(stage_2_output_tensor, logits_sequence_index,
                            {.temperature = service_config_.generation_config.temperature,
                             .repetition_penalty = service_config_.generation_config.repetition_penalty,
                             .top_p = service_config_.generation_config.top_p,
                             .top_k = service_config_.generation_config.top_k,
                             .generated_tokens = &generated_tokens}),
                        .handoff_byte_size = handoff_buffer.byte_size,
                    };
                };

                // Inkrementalis streaming allapot: a valodi tokenenkenti SSE emisszihoz
                // minden token generalasa utan dekodolunk es a deltat kuldjuk a kliensnek.
                std::size_t stream_emitted_chars = 0;
                bool stream_prefix_cleared = false;
                bool stream_think_opened = false;
                bool stream_cancelled = false;

                for (int generated_index = 0; generated_index < std::max(1, request.max_tokens); ++generated_index) {
                    decltype(run_split_step(current_input_ids, current_input_ids, current_attention_mask, current_position_ids))
                        step_result{};
                    if (generated_index == 0 && prompt_token_count > split_prefill_chunk_token_limit) {
                        // Prefill chunking: a Stage-1 chunkokban dolgozza fel a promptot
                        // (KV cache epitese), majd a Stage-2-t egyszer futtatjuk a teljes
                        // prompt-tal. Ez azert szukseges, mert a Stage-2 attention retegei
                        // (deep split) nem kezelik a kulonbozo meretu input_ids/hidden_state
                        // tensorokat chunk-onkent. A Stage-1 chunkolas viszont gyorsitja a
                        // prefill-t mert nem kell egyszerre 41k+ tokent a VRAM-ba tolteni.
                        std::size_t processed_prompt_tokens = 0U;
                        std::size_t chunk_index = 0U;

                        // Optimalizalt prefill: a Stage-1 chunkokban epiti a KV cache-t,
                        // az UTOLSO chunk-ot a run_split_step-pel futtatjuk ami a
                        // Stage-2-t is meghivja. Igy nincs felesleges teljes ujrafuttatas.
                        // A Stage-2 (tail-split eseten) nem tartalmaz attention retegeket,
                        // tehat eleg neki az utolso chunk hidden state-je.
                        const auto total_chunks =
                            (prompt_token_count + split_prefill_chunk_token_limit - 1U) / split_prefill_chunk_token_limit;

                        while (processed_prompt_tokens < prompt_token_count) {
                            const auto chunk_token_count =
                                std::min(split_prefill_chunk_token_limit, prompt_token_count - processed_prompt_tokens);
                            current_input_ids = slice_sequence_tensor(full_prompt_input_ids, processed_prompt_tokens, chunk_token_count);
                            current_attention_mask =
                                slice_sequence_tensor(full_prompt_attention_mask, 0U, processed_prompt_tokens + chunk_token_count);
                            current_position_ids = make_position_ids_tensor(processed_prompt_tokens, chunk_token_count);

                            const bool is_last_chunk = (processed_prompt_tokens + chunk_token_count >= prompt_token_count);

                            try_log_runtime_message(
                                service_config_,
                                masodik::diagnostics::LogLevel::info,
                                "manual_split_prefill_chunk profile=" + service_config_.active_profile +
                                    " request_id=" + split_handoff_.request_id +
                                    " sequence_id=" + split_handoff_.sequence_id +
                                    " chunk_index=" + std::to_string(chunk_index + 1U) +
                                    " chunk_tokens=" + std::to_string(chunk_token_count) +
                                    " attention_tokens=" + std::to_string(processed_prompt_tokens + chunk_token_count) +
                                    " prompt_tokens=" + std::to_string(prompt_token_count) +
                                    " last=" + (is_last_chunk ? "true" : "false"));

                            if (is_last_chunk) {
                                // Utolso chunk: run_split_step-pel futtatjuk (Stage-1 + Stage-2)
                                // A Stage-2 az utolso chunk hidden state-jet kapja meg.
                                step_result = run_split_step(
                                    current_input_ids,
                                    current_input_ids,
                                    current_attention_mask,
                                    current_position_ids);
                                stage_2_kv_processed = prompt_token_count;
                            } else {
                                // Koztes chunk: csak Stage-1 (KV cache epitese)
                                bind_split_generation_inputs(
                                    stage_1_request,
                                    *impl_->split_stage_1.compiled_model,
                                    current_input_ids,
                                    current_attention_mask,
                                    current_position_ids);
                                stage_1_request.infer();
                            }

                            processed_prompt_tokens += chunk_token_count;
                            ++chunk_index;
                        }
                    } else {
                        step_result = run_split_step(
                            current_input_ids,
                            current_input_ids,
                            current_attention_mask,
                            current_position_ids);
                    }

                    const auto next_token_id = step_result.next_token_id;
                    generated_tokens.push_back(next_token_id);

                    const auto eos_token_id = impl_->split_tokenizer->get_eos_token_id();
                    split_memory_summary_.system =
                        "prompt_tokens=" + std::to_string(prompt_token_count) +
                        ";total_tokens=" + std::to_string(current_total_tokens + generated_tokens.size()) +
                        ";generated_tokens=" + std::to_string(generated_tokens.size()) +
                        ";handoff_sequence=" + split_handoff_.sequence_id +
                        ";mode=compiled;handoff_bytes=" + std::to_string(step_result.handoff_byte_size);
                    try_log_runtime_message(
                        service_config_,
                        masodik::diagnostics::LogLevel::info,
                        "manual_split_decode_step profile=" + service_config_.active_profile +
                            " request_id=" + split_handoff_.request_id +
                            " sequence_id=" + split_handoff_.sequence_id +
                            " step=" + std::to_string(generated_index + 1) +
                            " next_token_id=" + std::to_string(next_token_id) +
                            " output_tensor=\"" + stage_2_output_name + "\"");

                    // Valodi tokenenkenti streaming <think> tag tamogatassal.
                    // A modell "analysis...assistantfinal..." formatumban general.
                    // Az "analysis" reszt <think>...</think> tagbe csomagoljuk, hogy az
                    // Open WebUI vizualisan elkuonitse a gondolkodast a vegso valasztol.
                    if (on_chunk && !(eos_token_id >= 0 && next_token_id == eos_token_id)) {
                        auto decoded_so_far = impl_->split_tokenizer->decode(
                            generated_tokens, ov::genai::skip_special_tokens(true));
                        if (!stream_prefix_cleared) {
                            constexpr std::string_view analysis_prefix = "analysis";
                            constexpr std::string_view marker = "assistantfinal";

                            // Az elso token(ek) utan: "analysis" prefix → <think> tag nyitas
                            if (!stream_think_opened && decoded_so_far.starts_with(analysis_prefix) &&
                                decoded_so_far.size() > analysis_prefix.size()) {
                                on_chunk({.delta_content = "<think>", .finish_reason = {}, .terminal = false});
                                // Az "analysis" prefix utani szoveget emitaljuk gondolkodaskent
                                const auto thinking_content = decoded_so_far.substr(analysis_prefix.size());
                                if (!thinking_content.empty()) {
                                    on_chunk({.delta_content = thinking_content, .finish_reason = {}, .terminal = false});
                                }
                                stream_emitted_chars = decoded_so_far.size();
                                stream_think_opened = true;
                            } else if (stream_think_opened) {
                                // A <think> mar nyitva van, streameljuk a gondolkodast
                                if (decoded_so_far.size() > stream_emitted_chars) {
                                    // De elobb ellenorizzuk, hogy az "assistantfinal" marker megjelent-e
                                    const auto marker_pos = decoded_so_far.find(marker);
                                    if (marker_pos != std::string::npos) {
                                        // A marker elotti utolso gondolkodas resz
                                        const auto remaining_thinking = decoded_so_far.substr(
                                            stream_emitted_chars, marker_pos - stream_emitted_chars);
                                        if (!remaining_thinking.empty()) {
                                            on_chunk({.delta_content = remaining_thinking, .finish_reason = {}, .terminal = false});
                                        }
                                        // </think> zaro tag + sortores
                                        on_chunk({.delta_content = "</think>\n", .finish_reason = {}, .terminal = false});
                                        // A vegso valasz kezdete
                                        const auto answer_start = marker_pos + marker.size();
                                        const auto initial_answer = decoded_so_far.substr(answer_start);
                                        stream_emitted_chars = 0;
                                        stream_prefix_cleared = true;
                                        // Az eddigi vegso valasz tartalom emisszioja
                                        if (!initial_answer.empty()) {
                                            on_chunk({.delta_content = initial_answer, .finish_reason = {}, .terminal = false});
                                            stream_emitted_chars = initial_answer.size();
                                        }
                                    } else {
                                        // Meg gondolkodik — streameljuk a deltat
                                        const auto delta = decoded_so_far.substr(stream_emitted_chars);
                                        if (!delta.empty()) {
                                            on_chunk({.delta_content = delta, .finish_reason = {}, .terminal = false});
                                        }
                                        stream_emitted_chars = decoded_so_far.size();
                                    }
                                }
                            }
                        }
                        // A marker utan: sima vegso valasz streaming
                        if (stream_prefix_cleared && decoded_so_far.size() > stream_emitted_chars) {
                            // A decoded_so_far itt mar a teljes szoveg, de nekunk csak
                            // az assistantfinal utani reszre kell az emitted_chars-t alkalmazni.
                            // Ujra dekodolni nem kell — a prefix_cleared allapotban a
                            // stream_emitted_chars mar a vegso valasz poziciojara mutat.
                        }
                        if (stream_prefix_cleared) {
                            // A vegso valasz streaming: ujra dekodolunk es az elozo emisszio
                            // utan levo deltat kuldjuk
                            auto full_decoded = impl_->split_tokenizer->decode(
                                generated_tokens, ov::genai::skip_special_tokens(true));
                            constexpr std::string_view m = "assistantfinal";
                            const auto mp = full_decoded.find(m);
                            if (mp != std::string::npos) {
                                auto answer_text = full_decoded.substr(mp + m.size());
                                if (answer_text.size() > stream_emitted_chars) {
                                    const auto delta = answer_text.substr(stream_emitted_chars);
                                    if (!delta.empty()) {
                                        if (!on_chunk({.delta_content = delta, .finish_reason = {}, .terminal = false})) {
                                            stream_cancelled = true;
                                        }
                                    }
                                    stream_emitted_chars = answer_text.size();
                                }
                            }
                        }
                        if (should_cancel && should_cancel()) {
                            stream_cancelled = true;
                        }
                        if (stream_cancelled) {
                            break;
                        }
                    }

                    if (eos_token_id >= 0 && next_token_id == eos_token_id) {
                        reached_eos = true;
                        break;
                    }

                    ++current_total_tokens;
                    if (current_total_tokens >= max_context_tokens) {
                        break;
                    }

                    current_input_ids = make_single_token_tensor(next_token_id);
                    current_attention_mask = make_attention_mask_tensor(current_total_tokens);
                    current_position_ids = make_position_ids_tensor(current_total_tokens - 1U, 1U);
                }

                const auto decoded_text = decode_generated_tokens(*impl_->split_tokenizer, generated_tokens);
                const auto finish_reason = reached_eos ? "stop" : "length";
                if (decoded_text.empty()) {
                    split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::stage_2;
                    throw std::runtime_error("A split detokenizer nem adott vissza olvashato szoveget a generalt tokenekhez.");
                }

                split_compiled_request_path_active_ = true;
                split_handoff_.state = SplitHandoffState::consumed;
                split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::none;
                try_log_runtime_message(
                    service_config_,
                    masodik::diagnostics::LogLevel::info,
                        "manual_split_request_complete profile=" + service_config_.active_profile +
                            " request_id=" + split_handoff_.request_id +
                            " sequence_id=" + split_handoff_.sequence_id +
                            " handoff_state=consumed mode=compiled decoded_chars=" + std::to_string(decoded_text.size()) +
                            " generated_tokens=" + std::to_string(generated_tokens.size()));
                try_log_runtime_message(
                    service_config_,
                    masodik::diagnostics::LogLevel::info,
                    masodik::diagnostics::format_split_proof_message(
                        {.request_id = split_handoff_.request_id,
                         .sequence_id = split_handoff_.sequence_id,
                         .request_path_mode = "compiled",
                         .handoff_state = std::string{to_string(split_handoff_.state)},
                         .stage_1_device = split_stage_1_.device_id,
                         .stage_2_device = split_stage_2_.device_id,
                         .stage_1_execution_devices = split_stage_1_execution_devices_summary_,
                         .stage_2_execution_devices = split_stage_2_execution_devices_summary_,
                         .split_artifact_version = manifest.artifact_version,
                         .validated_context_tokens = manifest.validated_context_tokens,
                         .memory_summary_system = split_memory_summary_.system}));
                return {.content = decoded_text, .finish_reason = finish_reason};
            }

            prepare_runtime_stage_inputs(stage_1_request, *impl_->split_stage_1.compiled_model);
            stage_1_request.infer();
            const auto stage_1_hidden_state = stage_1_request.get_tensor(*stage_1_output_port);
            auto handoff_buffer = masodik::split::make_ram_backed_handoff_buffer(
                stage_1_hidden_state,
                manifest.hidden_state_contract,
                compiled_port_name(*stage_2_handoff_input));
            prepare_runtime_stage_inputs(stage_2_request, *impl_->split_stage_2.compiled_model, stage_2_handoff_input);
            stage_2_request.set_tensor(*stage_2_handoff_input, handoff_buffer.tensor);
            split_handoff_.state = SplitHandoffState::transferred;
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                "manual_split_handoff_buffer_ready profile=" + service_config_.active_profile +
                    " request_id=" + split_handoff_.request_id +
                    " sequence_id=" + split_handoff_.sequence_id +
                    " source_contract=\"" + handoff_buffer.source_contract + "\"" +
                    " target_tensor=\"" + handoff_buffer.target_tensor_name + "\"" +
                    " shape=\"" + masodik::split::describe_tensor_shape(handoff_buffer.tensor) + "\"" +
                    " bytes=" + std::to_string(handoff_buffer.byte_size) +
                    " fingerprint=" + format_uint64_hex(handoff_buffer.fingerprint));
            stage_2_request.infer();

            const auto stage_2_output_tensor = stage_2_request.get_tensor(*stage_2_output_port);
            const auto stage_2_output_name = compiled_port_name(*stage_2_output_port);
            split_compiled_request_path_active_ = true;
            split_handoff_.state = SplitHandoffState::consumed;
            split_last_failure_scope_ = masodik::runtime::RuntimeFailureScope::none;
            split_memory_summary_.system +=
                ";mode=compiled;handoff_bytes=" + std::to_string(handoff_buffer.byte_size);
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                    "manual_split_request_complete profile=" + service_config_.active_profile +
                        " request_id=" + split_handoff_.request_id +
                        " sequence_id=" + split_handoff_.sequence_id +
                        " handoff_state=consumed mode=compiled output_tensor=\"" + stage_2_output_name +
                        "\" output_shape=\"" + masodik::split::describe_tensor_shape(stage_2_output_tensor) + "\"");
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                masodik::diagnostics::format_split_proof_message(
                    {.request_id = split_handoff_.request_id,
                     .sequence_id = split_handoff_.sequence_id,
                     .request_path_mode = "compiled",
                     .handoff_state = std::string{to_string(split_handoff_.state)},
                     .stage_1_device = split_stage_1_.device_id,
                     .stage_2_device = split_stage_2_.device_id,
                     .stage_1_execution_devices = split_stage_1_execution_devices_summary_,
                     .stage_2_execution_devices = split_stage_2_execution_devices_summary_,
                     .split_artifact_version = manifest.artifact_version,
                     .validated_context_tokens = manifest.validated_context_tokens,
                     .memory_summary_system = split_memory_summary_.system}));
            return {
                .content = build_compiled_split_response(
                    last_message,
                    stage_1_output_name,
                    handoff_buffer,
                    stage_2_output_name,
                    stage_2_output_tensor),
                .finish_reason = "stop"};
        } catch (const std::exception& error) {
            note_runtime_failure(error);
            throw;
        }
    }

    if (impl_->pipeline) {
        const auto last_message = find_last_user_message(request);
        ov::genai::GenerationConfig generation_config;
        generation_config.max_new_tokens = request.max_tokens;
        // A max_length a tenyleges prompt + max_new_tokens meretehez igazodik,
        // sapkazva a profil desired_context_tokens ertekevel.
        // Igy nem allokaljuk a teljes context ablakot (pl. 128k) minden egyes kereshez.
        const auto context_cap = static_cast<std::size_t>(
            std::max(1, service_config_.generation_config.desired_context_tokens));
        generation_config.max_length = std::min(
            prompt_tokens_estimate + static_cast<std::size_t>(request.max_tokens) + 256,
            context_cap);
        // Mintavetelesi parameterek a profilbol — a repetitiv loop-ok elkerulesehez
        generation_config.do_sample = service_config_.generation_config.do_sample;
        generation_config.temperature = service_config_.generation_config.temperature;
        generation_config.repetition_penalty = service_config_.generation_config.repetition_penalty;
        generation_config.top_p = service_config_.generation_config.top_p;
        generation_config.top_k = static_cast<std::size_t>(std::max(1, service_config_.generation_config.top_k));

        const auto started_at = std::chrono::steady_clock::now();
        try {
            // Valodi tokenenkenti streaming: ha van on_chunk callback, az OpenVINO SDK
            // StreamerVariant-jan keresztul emittalunk minden dekodolt szovegreszt.
            std::string streamed_full_content;
            ov::genai::StreamerVariant streamer_variant = std::monostate{};
            if (on_chunk) {
                // A modell "analysis...assistantfinal..." formatumban general.
                // A gondolkodas reszt (analysis) elnyeljuk, csak az "assistantfinal"
                // utani vegleges valaszt streameljuk a kliensnek.
                // A modell "analysis...assistantfinal..." formatumban general.
                // Az "analysis" reszt <think>...</think> tagbe csomagoljuk streamelve.
                streamer_variant = std::function<ov::genai::StreamingStatus(std::string)>(
                    [&on_chunk, &should_cancel, &streamed_full_content,
                     marker_found = false, think_opened = false, buffer = std::string{}]
                    (const std::string& text) mutable -> ov::genai::StreamingStatus {
                        if (should_cancel && should_cancel()) {
                            return ov::genai::StreamingStatus::CANCEL;
                        }
                        if (text.empty()) {
                            return ov::genai::StreamingStatus::RUNNING;
                        }
                        if (!marker_found) {
                            buffer += text;
                            // "analysis" prefix → <think> tag nyitas
                            constexpr std::string_view analysis_prefix = "analysis";
                            if (!think_opened && buffer.starts_with(analysis_prefix) &&
                                buffer.size() > analysis_prefix.size()) {
                                on_chunk({.delta_content = "<think>", .finish_reason = {}, .terminal = false});
                                const auto thinking = buffer.substr(analysis_prefix.size());
                                on_chunk({.delta_content = thinking, .finish_reason = {}, .terminal = false});
                                think_opened = true;
                            } else if (think_opened) {
                                // Uj gondolkodas delta
                                on_chunk({.delta_content = text, .finish_reason = {}, .terminal = false});
                            }
                            // Keressuk az "assistantfinal" markert
                            constexpr std::string_view marker = "assistantfinal";
                            const auto pos = buffer.find(marker);
                            if (pos != std::string::npos) {
                                marker_found = true;
                                if (think_opened) {
                                    on_chunk({.delta_content = "</think>\n", .finish_reason = {}, .terminal = false});
                                }
                                const auto content_start = pos + marker.size();
                                const auto initial_content = buffer.substr(content_start);
                                buffer.clear();
                                if (!initial_content.empty()) {
                                    streamed_full_content += initial_content;
                                    if (!on_chunk({.delta_content = initial_content, .finish_reason = {}, .terminal = false})) {
                                        return ov::genai::StreamingStatus::STOP;
                                    }
                                }
                            }
                            return ov::genai::StreamingStatus::RUNNING;
                        }
                        streamed_full_content += text;
                        if (!on_chunk({.delta_content = text, .finish_reason = {}, .terminal = false})) {
                            return ov::genai::StreamingStatus::STOP;
                        }
                        return ov::genai::StreamingStatus::RUNNING;
                    });
            }

            const auto decoded_results = impl_->pipeline->generate(last_message, generation_config, streamer_variant);
            const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at)
                    .count();

            // Ha streaming volt, a content mar emittalodott; egyebkent a decoded_results-bol jont
            const auto& output_text = on_chunk ? streamed_full_content
                : (decoded_results.texts.empty() ? std::string{}  : decoded_results.texts.front());
            if (output_text.empty() && !on_chunk) {
                throw std::runtime_error("Az OpenVINO GenAI pipeline nem adott vissza dekodolt szoveget.");
            }

            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::info,
                "genai_generate_ok profile=" + service_config_.active_profile +
                    " effective_device=" + service_config_.effective_device +
                    " prompt_chars=" + std::to_string(last_message.size()) +
                    " max_new_tokens=" + std::to_string(request.max_tokens) +
                    " max_length=" + std::to_string(generation_config.max_length) +
                    " output_chars=" + std::to_string(output_text.size()) +
                    " elapsed_ms=" + std::to_string(elapsed_ms) +
                    " streaming=" + (on_chunk ? "true" : "false") +
                    " prompt_preview=\"" + truncate_for_log(last_message) + "\"");

            if (service_config_.split_mode) {
                split_handoff_.state = SplitHandoffState::consumed;
                try_log_runtime_message(
                    service_config_,
                    masodik::diagnostics::LogLevel::info,
                    "manual_split_request_complete profile=" + service_config_.active_profile +
                        " request_id=" + split_handoff_.request_id +
                        " sequence_id=" + split_handoff_.sequence_id +
                        " handoff_state=consumed mode=genai output_chars=" +
                        std::to_string(output_text.size()));
            }
            return {.content = output_text, .finish_reason = "stop"};
        } catch (const std::exception& error) {
            const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at)
                    .count();
            note_runtime_failure(error);
            try_log_runtime_message(
                service_config_,
                masodik::diagnostics::LogLevel::error,
                "genai_generate_failed profile=" + service_config_.active_profile +
                    " effective_device=" + service_config_.effective_device +
                    " prompt_chars=" + std::to_string(last_message.size()) +
                    " max_new_tokens=" + std::to_string(request.max_tokens) +
                    " max_length=" + std::to_string(generation_config.max_length) +
                    " elapsed_ms=" + std::to_string(elapsed_ms) +
                    " error=\"" + std::string{error.what()} + "\"" +
                    " prompt_preview=\"" + truncate_for_log(last_message) + "\"");
            throw;
        }
    }
#endif

    const auto last_message = find_last_user_message(request);
    if (service_config_.split_mode) {
        split_handoff_.state = SplitHandoffState::consumed;
        try_log_runtime_message(
            service_config_,
            masodik::diagnostics::LogLevel::info,
            "manual_split_request_complete profile=" + service_config_.active_profile +
                " request_id=" + split_handoff_.request_id +
                " sequence_id=" + split_handoff_.sequence_id +
                " handoff_state=consumed mode=placeholder output_chars=" +
                std::to_string(last_message.size()));
    }
    return {.content =
                "Placeholder valasz a Phase 3 vertical slice-bol. A tenyleges OpenVINO GenAI kotest a "
                "kovetkezo kor vezeti be. Aktiv eszkozprofil: " +
                service_config_.effective_device + ". Utolso user uzenet: " + last_message,
            .finish_reason = "stop"};
}

void LlmPipelineService::stream_chat_completion(
    const masodik::api::ChatCompletionRequest& request,
    StreamingChunkCallback on_chunk,
    StreamingCancellationCheck should_cancel) const {
    if (!on_chunk) {
        throw std::invalid_argument("A streaming callback nem lehet ures.");
    }

    if (should_cancel && should_cancel()) {
        throw std::runtime_error("A streaming generalas a kliens megszakitasa miatt nem indult el.");
    }

    const auto stream_failure_mode = stream_failure_mode_from_environment();
    if (stream_failure_mode == "before_first_chunk") {
        // A teszt hook-ot meg a generalas elott kell triggerelni.
        // Elobb futtassuk a teljes generalast, hogy az eredmeny meglegyen,
        // majd dobjunk hibaval a content chunkok elott.
        const auto generation_result = generate_chat_result(request);
        (void)generation_result;
        throw std::runtime_error("Tesztcelu streaming hiba a content chunkok elott.");
    }

    const auto stream_chunk_delay_millis = stream_chunk_delay_millis_from_environment();

    // Ellenorizzuk, hogy tool_call request-e. Ha igen, a tool call eredmenyek
    // szintetizaltak (nem token-generalt valasz), ezert bufferelt streaming marad.
    const auto tool_call_result = build_tool_call_result(request);
    if (!tool_call_result.tool_calls.empty()) {
        bool emitted_tool_delta = false;
        for (const auto& chunk : build_tool_call_stream_chunks(tool_call_result.tool_calls)) {
            if (stream_chunk_delay_millis > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds{stream_chunk_delay_millis});
            }
            if (should_cancel && should_cancel()) {
                throw std::runtime_error("A streaming generalas a kliens megszakitasa miatt megszakadt.");
            }
            if (!on_chunk(chunk)) {
                throw std::runtime_error("A streaming callback megszakitotta a tool-call valaszfolyamot.");
            }
            emitted_tool_delta = true;
            if (stream_failure_mode == "after_first_chunk" && emitted_tool_delta) {
                throw std::runtime_error("Tesztcelu streaming hiba az elso tool-call delta utan.");
            }
        }
        on_chunk(
            {.finish_reason = tool_call_result.finish_reason.empty() ? "tool_calls" : tool_call_result.finish_reason,
             .terminal = true});
        return;
    }

    // Content streaming: a generate_chat_result_impl() a generacio soran
    // valodi tokenenkent hivja az on_chunk callbacket. A content mar a
    // generacio kozben emittalodik, nem utolag 24-karakteres chunkokra vagva.
    bool emitted_any_content = false;
    auto streaming_on_chunk = [&](const StreamingChunk& chunk) -> bool {
        if (stream_failure_mode == "after_first_chunk" && emitted_any_content) {
            throw std::runtime_error("Tesztcelu streaming hiba az elso content chunk utan.");
        }
        if (stream_chunk_delay_millis > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds{stream_chunk_delay_millis});
        }
        emitted_any_content = true;
        return on_chunk(chunk);
    };

    const auto generation_result = generate_chat_result_impl(
        request,
        streaming_on_chunk,
        should_cancel);

    if (should_cancel && should_cancel()) {
        throw std::runtime_error("A streaming generalas a kliens megszakitasa miatt megszakadt.");
    }

    on_chunk({.delta_content = {}, .finish_reason = generation_result.finish_reason, .terminal = true});
}

}  // namespace masodik::genai
