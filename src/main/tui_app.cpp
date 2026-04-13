#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "tui_app.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include "masodik/api/openai_contract.hpp"
#include "masodik/config/default_paths.hpp"
#include "masodik/config/json_loader.hpp"
#include "masodik/diagnostics/logger.hpp"
#include "masodik/genai/llm_pipeline_service.hpp"
#include "masodik/genai/embedding_pipeline_service.hpp"
#include "masodik/runtime/service_config.hpp"
#include "masodik/tui/resource_monitor.hpp"
#include "masodik/runtime/service_lifecycle.hpp"
#include "masodik/server/openai_handlers.hpp"
#include "masodik/server/http_server.hpp"
#include "masodik/tui/chat_history.hpp"
#include "masodik/tui/chat_message.hpp"
#include "masodik/tui/chat_screen.hpp"
#include "masodik/tui/help_screen.hpp"
#include "masodik/tui/loading_screen.hpp"
#include "masodik/tui/status_bar.hpp"
#include "masodik/tui/function_keys.hpp"

namespace {

struct TuiArguments {
    std::filesystem::path profile_path;
    std::filesystem::path model_root;
    std::filesystem::path repo_root = std::filesystem::current_path();
    // Chat feature konfig mezok
    std::string user_name = "Te";
    std::string agent_name = "AI";
    std::filesystem::path agent_prompt_file;
    std::filesystem::path chat_history_file;
    // Generalasi parameterek (felulirjak a profil ertekeket ha megadottak)
    int max_tokens = 1024;
    float temperature = -1.0f;        // -1 = profil erteke marad
    float repetition_penalty = -1.0f;
    float top_p = -1.0f;
    int top_k = -1;
};

// A config/masodik.json fajlbol olvassa az alapertekeket,
// a parancssori argumentumok felulirjak oket.
TuiArguments parse_tui_arguments(const int argc, char** argv) {
    TuiArguments args;

    const auto config_path = args.repo_root / "config" / "masodik.json";
    if (std::filesystem::exists(config_path)) {
        try {
            const auto config_value = masodik::config::JsonLoader::load_file(config_path);
            if (config_value.is_object()) {
                const auto& obj = config_value.as_object();
                args.profile_path = masodik::config::get_string_or(obj, "profile", "");
                args.model_root = masodik::config::get_string_or(obj, "model_root", "");
                args.user_name = masodik::config::get_string_or(obj, "user_name", "Te");
                args.agent_name = masodik::config::get_string_or(obj, "agent_name", "AI");
                auto prompt_file = masodik::config::get_string_or(obj, "agent_prompt_file", "");
                if (!prompt_file.empty()) args.agent_prompt_file = prompt_file;
                auto history_file = masodik::config::get_string_or(obj, "chat_history_file", "");
                if (!history_file.empty()) args.chat_history_file = history_file;
                // Generalasi parameterek
                auto mt = masodik::config::get_number_or(obj, "max_tokens", -1.0);
                if (mt > 0) args.max_tokens = static_cast<int>(mt);
                auto temp = masodik::config::get_number_or(obj, "temperature", -1.0);
                if (temp >= 0) args.temperature = static_cast<float>(temp);
                auto rep = masodik::config::get_number_or(obj, "repetition_penalty", -1.0);
                if (rep > 0) args.repetition_penalty = static_cast<float>(rep);
                auto tp = masodik::config::get_number_or(obj, "top_p", -1.0);
                if (tp >= 0) args.top_p = static_cast<float>(tp);
                auto tk = masodik::config::get_number_or(obj, "top_k", -1.0);
                if (tk >= 0) args.top_k = static_cast<int>(tk);
            }
        } catch (...) {}
    }

    if (args.profile_path.empty()) {
        args.profile_path = std::filesystem::path{"config"} / "profiles" / "b580-tail-split-128k.json";
    }
    if (args.model_root.empty()) {
        args.model_root = masodik::config::default_paths::default_model_root;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string current = argv[i];
        if (current == "--serve") continue;
        if (current == "--profile" && i + 1 < argc) { args.profile_path = argv[++i]; continue; }
        if (current == "--model-root" && i + 1 < argc) { args.model_root = argv[++i]; continue; }
        if (current == "--repo-root" && i + 1 < argc) { args.repo_root = argv[++i]; continue; }
    }

    if (args.profile_path.is_relative()) {
        args.profile_path = args.repo_root / args.profile_path;
    }
    if (args.model_root.is_relative()) {
        args.model_root = args.repo_root / args.model_root;
    }
    if (!args.agent_prompt_file.empty() && args.agent_prompt_file.is_relative()) {
        args.agent_prompt_file = args.repo_root / args.agent_prompt_file;
    }
    if (!args.chat_history_file.empty() && args.chat_history_file.is_relative()) {
        args.chat_history_file = args.repo_root / args.chat_history_file;
    }
    return args;
}

}  // namespace

// Globalis Ctrl+C kezeles — a TUI-ban nem all le, hanem hint uzenetet mutat.
// A hint_message pointer-t a handler allitja, a monitor thread torolni fogja 3 sec mulva.
namespace {
    std::atomic<int> ctrl_c_count = 0;
    std::shared_ptr<masodik::tui::StatusBarInfo> g_status_info;
    ftxui::ScreenInteractive* g_screen = nullptr;

    BOOL WINAPI tui_console_ctrl_handler(DWORD signal) {
        if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
            ++ctrl_c_count;
            if (g_status_info) {
                g_status_info->hint_message = "Kilepeshez hasznald az F10 billentyut.";
            }
            if (g_screen) {
                g_screen->Post(ftxui::Event::Custom);
            }
            return TRUE;  // Kezeltuk — NEM all le a process
        }
        return FALSE;
    }
}

int run_tui_app(int argc, char** argv) {
    const auto args = parse_tui_arguments(argc, argv);

    auto service_config = masodik::runtime::build_service_config(
        args.profile_path, args.model_root, args.repo_root);

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    g_screen = &screen;

    // A Ctrl+C NEM allitja le az alkalmazast — F10 a kilepes.
    // Harom retegu vedelem:
    // 1. ForceHandleCtrlC(false) — az FTXUI event szinten ne hivjon SIGABRT-ot
    // 2. SetConsoleCtrlHandler — a Windows console signal szinten elkapjuk
    // 3. ENABLE_PROCESSED_INPUT kikapcsolasa — a Windows kernel ne kuldjoen SIGINT-et
    screen.ForceHandleCtrlC(false);
    // Az FTXUI eger kovetes BEKAPCSOLVA marad (alapertelmezett).
    // Az FTXUI belso selection API-ja kezeli a kijelolest — ez tulelei
    // az ujrarajzolasokat mert a selection_data_ adatstrukturaban taroloddik,
    // nem a terminal bufferben. A CatchEvent-ben return true-val
    // megakadalyozzuk hogy az Input komponens zavarjon.

    // A Windows console ENABLE_PROCESSED_INPUT flag kikapcsolasa megakadalyozza,
    // hogy a Ctrl+C SIGINT signal-t generaljon. Igy a Ctrl+C csak egy sima
    // billentyulenyomasdkent erkezik az FTXUI event loop-ba.
    {
        HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
        DWORD console_mode = 0;
        GetConsoleMode(stdin_handle, &console_mode);
        // ENABLE_PROCESSED_INPUT = 0x0001 — ha aktiv, a Ctrl+C SIGINT-et general
        console_mode &= ~0x0001U;
        SetConsoleMode(stdin_handle, console_mode);
    }

    // --- Allapot ---
    auto chat_state = std::make_shared<masodik::tui::ChatState>();
    std::mutex chat_mutex;

    auto status_info = std::make_shared<masodik::tui::StatusBarInfo>();
    g_status_info = status_info;

    SetConsoleCtrlHandler(tui_console_ctrl_handler, TRUE);
    status_info->model_name = service_config.active_model;
    status_info->device_profile = service_config.effective_device;
    status_info->context_total = service_config.generation_config.desired_context_tokens;
    status_info->state = "Kesz";  // Azonnal kesz — a modell majd lazy loading-gal toltodik

    if (const char* iter_env = std::getenv("MASODIK_ITER"); iter_env != nullptr) {
        try { status_info->iter = std::stoi(iter_env); } catch (...) {}
    }
    const auto app_start_time = std::chrono::steady_clock::now();

    // --- Generalasi parameterek felulirasa a masodik.json-bol ---
    // A profil ertekeket felulirjak, ha a masodik.json-ban megadottak.
    if (args.temperature >= 0) service_config.generation_config.temperature = args.temperature;
    if (args.repetition_penalty > 0) service_config.generation_config.repetition_penalty = args.repetition_penalty;
    if (args.top_p >= 0) service_config.generation_config.top_p = args.top_p;
    if (args.top_k >= 0) service_config.generation_config.top_k = args.top_k;

    // --- Chat history betoltes ---
    // Az indulaskor a korabbi uzenetek betoltodnek a JSONL fajlbol.
    if (!args.chat_history_file.empty()) {
        try {
            auto loaded = masodik::tui::chat_history::load(args.chat_history_file);
            if (!loaded.empty()) {
                chat_state->messages = std::move(loaded);
            }
        } catch (...) {
            // Serult history fajl → ures chattel indulunk
        }
    }

    // --- System prompt betoltes ---
    // Az AGENTS.md / SYSTEM_PROMPT.md fajl tartalma a chat pipeline system prompt-ja.
    std::string system_prompt_content;
    if (!args.agent_prompt_file.empty() && std::filesystem::exists(args.agent_prompt_file)) {
        std::ifstream prompt_file(args.agent_prompt_file);
        if (prompt_file.is_open()) {
            system_prompt_content.assign(
                std::istreambuf_iterator<char>(prompt_file),
                std::istreambuf_iterator<char>());
        }
    }

    // --- Pipeline (lazy loading) ---
    // A modell NEM toltodik be az indulaskor. Csak az elso uzenet kuldesekor
    // inicializalodik. Ha 10 percig nincs hasznalat, automatikusan offload-olodik.
    std::shared_ptr<masodik::genai::LlmPipelineService> pipeline;
    std::shared_ptr<masodik::genai::EmbeddingPipelineService> embedding;
    std::atomic<bool> pipeline_loaded = false;
    std::atomic<bool> loading_in_progress = false;
    std::string loading_error_message;
    // Ha az uzenet betoltes kozben erkezik, a betoltes utan automatikusan feldolgozzuk
    std::atomic<bool> pending_send = false;
    std::chrono::steady_clock::time_point last_activity_time = std::chrono::steady_clock::now();
    constexpr int offload_timeout_seconds = 600;  // 10 perc inaktivitas utan offload

    // A pipeline betoltese — hivhato az elso uzenetkor vagy F5-tel
    auto ensure_pipeline_loaded = [&]() {
        if (pipeline_loaded && pipeline && pipeline->ready()) {
            last_activity_time = std::chrono::steady_clock::now();
            return true;
        }
        if (loading_in_progress) return false;

        loading_in_progress = true;
        status_info->state = "Betoltes...";
        status_info->hint_message = "Modell betoltese folyamatban...";
        screen.Post(ftxui::Event::Custom);

        // Betoltes kulon szalon
        std::thread([&]() {
            try {
                pipeline = std::make_shared<masodik::genai::LlmPipelineService>(service_config);
                pipeline->initialize();

                if (pipeline->ready()) {
                    // Embedding betoltes (NPU)
                    if (service_config.serving_profile.embedding_enabled && !embedding) {
                        embedding = std::make_shared<masodik::genai::EmbeddingPipelineService>();
                        embedding->initialize(
                            service_config.serving_profile.embedding_model_path,
                            service_config.serving_profile.embedding_device,
                            service_config.serving_profile.embedding_model_name,
                            service_config.serving_profile.embedding_reject_cpu_fallback);
                    }
                    pipeline_loaded = true;
                    last_activity_time = std::chrono::steady_clock::now();
                    status_info->state = "Kesz";
                    status_info->hint_message.clear();
                } else {
                    loading_error_message = pipeline->status_message();
                    status_info->state = "Hiba";
                    status_info->hint_message = "Betoltesi hiba: " + loading_error_message;
                }
            } catch (const std::exception& error) {
                loading_error_message = error.what();
                status_info->state = "Hiba";
                status_info->hint_message = "Betoltesi hiba: " + loading_error_message;
            }
            loading_in_progress = false;
            screen.Post(ftxui::Event::Custom);
        }).detach();

        return false;
    };

    std::string input_content;
    std::atomic<bool> generating = false;
    std::chrono::steady_clock::time_point generation_start;
    std::atomic<int> generated_token_count = 0;

    // Uzenet navigacio allapot (US7: korabbi uzenetek szerkesztese)
    // -1 = beviteli mezo aktiv (nincs navigacio), 0+ = a felhasznaloi uzenetek indexe
    int browse_index = -1;
    // Ha korabbi uzenetet toltottunk be szerkesztesre, megjegyezzuk honnan.
    // -1 = nincs betoltott uzenet, 0+ = az uzenetek tombben az eredeti index.
    // A torles CSAK akkor tortenik ha a felhasznalo elküldi a modositott uzenetet.
    int edit_source_msg_idx = -1;
    // Megerosites parbeszed: hany uzenet vesz el
    int pending_edit_loss_count = 0;
    // Kontextus tulcsordulas: becsult / max token
    int overflow_estimated_tokens = 0;
    int overflow_max_tokens = 0;

    // TUI debug naplo — a generalasi es navigacios utak nyomonkovetese
    auto tui_log_path = args.repo_root / "data" / "tui-debug.log";
    auto tui_log = [&](const std::string& msg) {
        std::ofstream log(tui_log_path, std::ios::app);
        if (log.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time));
            log << buf << " " << msg << "\n";
        }
    };
    tui_log("=== TUI indulas ===");

    enum class ModalMode { none, help, quit_confirm, diagnostics, edit_confirm, context_overflow };
    ModalMode modal_mode = ModalMode::none;

    std::unique_ptr<masodik::server::HttpServer> web_server;
    bool web_server_running = false;

    // Kurzor villogas — stabil allapotvaltas masodpercenkent
    // A monitor thread valtogtaja, a render transform olvassa.
    std::atomic<bool> cursor_visible = true;

    // Pipeline offload — felszabaditja az eroforrasokat
    auto offload_pipeline = [&]() {
        if (!pipeline_loaded || loading_in_progress || generating) return;
        pipeline.reset();
        embedding.reset();
        pipeline_loaded = false;
        status_info->state = "Kesz";
        status_info->hint_message = "Modell offload-olva (10 perc inaktivitas).";
        screen.Post(ftxui::Event::Custom);
    };

    // A streaming generalas inditasa — forward deklaracio
    std::function<void()> start_generation;

    // --- Uzenet kuldese a pipeline-nak ---
    auto send_message = [&]() {
        tui_log("send_message: generating=" + std::to_string(generating.load()) + " input='" + input_content.substr(0, 40) + "'");
        if (generating) return;

        // Az uzenetet mindenkepp felrakjuk a chat-re (meg ha a modell nincs betoltve)
        std::string user_text;
        {
            std::lock_guard lock(chat_mutex);
            user_text = input_content;
            while (!user_text.empty() && user_text.front() == ' ') user_text.erase(0, 1);
            while (!user_text.empty() && user_text.back() == ' ') user_text.pop_back();
            if (user_text.empty()) return;
            input_content = " ";

            // Felhasznaloi uzenet hozzaadasa
            chat_state->messages.push_back({
                .role = masodik::tui::ChatMessage::Role::user,
                .name = args.user_name,
                .content = user_text,
            });
            // Asszisztensi uzenet — a varakozasi allapotot mutatja
            chat_state->messages.push_back({
                .role = masodik::tui::ChatMessage::Role::assistant,
                .name = args.agent_name,
                .is_streaming = true,
            });
            chat_state->generating = true;

            // Chat history: felhasznaloi uzenet mentese JSONL-be
            if (!args.chat_history_file.empty()) {
                masodik::tui::chat_history::append(args.chat_history_file,
                    chat_state->messages[chat_state->messages.size() - 2]);
            }
        }
        screen.Post(ftxui::Event::Custom);

        // Lazy loading: ha a pipeline meg nincs betoltve, most toltjuk be
        if (!pipeline_loaded || !pipeline || !pipeline->ready()) {
            if (!ensure_pipeline_loaded()) {
                // A betoltes elindult — az AI uzenetben jelezzuk
                {
                    std::lock_guard lock(chat_mutex);
                    auto& ai_msg = chat_state->messages.back();
                    ai_msg.content = "Varakozas: Modell betoltese...";
                }
                pending_send = true;
                screen.Post(ftxui::Event::Custom);
                return;
            }
        }
        last_activity_time = std::chrono::steady_clock::now();
        start_generation();
    };

    // A streaming generalas inditasa — a chat-ben mar van user + AI uzenet par,
    // ez a fuggveny inditja a tenyleges pipeline hivast kulon szalon.
    start_generation = [&]() {
        tui_log("start_generation: msgs=" + std::to_string(chat_state->messages.size()) + " pipeline_loaded=" + std::to_string(pipeline_loaded.load()));
        generating = true;
        generated_token_count = 0;
        generation_start = std::chrono::steady_clock::now();
        status_info->state = "General...";
        // Context szamlalo becsles: az osszes korabbi uzenet + az uj uzenet
        // Hozzavetoleges becslés: 1 token ~ 4 karakter (magyar szovegnel)
        {
            int estimated_tokens = 0;
            for (const auto& msg : chat_state->messages) {
                estimated_tokens += static_cast<int>(msg.content.size()) / 4 + 1;
            }
            status_info->context_used = estimated_tokens;
        }
        screen.Post(ftxui::Event::Custom);

        // Streaming generalas kulon szalon
        std::thread([&]() {
            try {
                // OpenAI chat request osszeallitasa
                masodik::api::ChatCompletionRequest request;
                request.model = service_config.active_model;
                request.max_tokens = args.max_tokens;

                // Az osszes korabbi uzenet belerakas a kontextusba.
                // Fontos: a <think> blokkokat es invalid UTF-8-at el kell tavolitani,
                // mert a chat template parser nem kezeli oket.
                auto clean_for_pipeline = [](const std::string& text) -> std::string {
                    std::string result = text;
                    // <think>...</think> blokk eltavolitasa
                    while (true) {
                        const auto start = result.find("<think>");
                        if (start == std::string::npos) break;
                        const auto end = result.find("</think>", start);
                        if (end != std::string::npos) {
                            result.erase(start, end - start + 8);
                            // Az utana levo sortores is
                            if (start < result.size() && result[start] == '\n') {
                                result.erase(start, 1);
                            }
                        } else {
                            // Nincs zaro tag — az egesz <think> utani reszt toroljuk
                            result.erase(start);
                            break;
                        }
                    }
                    // Invalid UTF-8 byte-ok eltavolitasa (pl. emoji tormelekek)
                    std::string clean;
                    clean.reserve(result.size());
                    for (std::size_t i = 0; i < result.size(); ++i) {
                        const auto ch = static_cast<unsigned char>(result[i]);
                        if (ch < 0x80) {
                            clean.push_back(result[i]);  // ASCII
                        } else if (ch < 0xC0) {
                            continue;  // Folytato byte maganyosan — kihagyjuk
                        } else if (ch < 0xE0) {
                            if (i + 1 < result.size()) { clean += result.substr(i, 2); i += 1; }
                        } else if (ch < 0xF0) {
                            if (i + 2 < result.size()) { clean += result.substr(i, 3); i += 2; }
                        } else if (ch < 0xF8) {
                            if (i + 3 < result.size()) { clean += result.substr(i, 4); i += 3; }
                        }
                    }
                    return clean;
                };

                // System prompt hozzaadasa a keres elejere
                if (!system_prompt_content.empty()) {
                    masodik::api::ChatMessage sys_msg;
                    sys_msg.role = "system";
                    sys_msg.content = system_prompt_content;
                    request.messages.push_back(std::move(sys_msg));
                }

                {
                    std::lock_guard lock(chat_mutex);
                    for (const auto& msg : chat_state->messages) {
                        if (msg.is_streaming) continue;
                        masodik::api::ChatMessage api_msg;
                        api_msg.role = msg.role == masodik::tui::ChatMessage::Role::user ? "user" : "assistant";
                        api_msg.content = clean_for_pipeline(msg.content);
                        if (!api_msg.content.empty()) {
                            request.messages.push_back(std::move(api_msg));
                        }
                    }
                }

                // Kozvetlen C++ streaming hivas — nincs HTTP reteg.
                // A <think>...</think> blokkot kulon kezeljuk: amig a vegso valasz
                // nem erkezik, a thinking resz halvany szinnel jelenik meg.
                // A pipeline mar <think>...</think>\n formatumban kuldi a tartalmat.
                bool in_think_block = false;
                std::string stream_buffer;  // Nyers buffer a <think> tag detektalasahoz

                pipeline->stream_chat_completion(
                    request,
                    [&](const masodik::genai::StreamingChunk& chunk) -> bool {
                        if (!chunk.delta_content.empty()) {
                            std::lock_guard lock(chat_mutex);
                            auto& last_msg = chat_state->messages.back();
                            stream_buffer += chunk.delta_content;
                            ++generated_token_count;

                            // <think> tag kezeles a nyers bufferben
                            if (!in_think_block && stream_buffer.find("<think>") != std::string::npos) {
                                in_think_block = true;
                                // A <think> tag elotti reszt eldobjuk (ures)
                                const auto pos = stream_buffer.find("<think>");
                                stream_buffer = stream_buffer.substr(pos + 7);
                                last_msg.thinking_content = stream_buffer;
                                last_msg.show_thinking = true;
                            } else if (in_think_block) {
                                const auto close_pos = stream_buffer.find("</think>");
                                if (close_pos != std::string::npos) {
                                    // A thinking blokk vege — a </think> utani resz a vegso valasz
                                    last_msg.thinking_content = stream_buffer.substr(0, close_pos);
                                    auto after_think = stream_buffer.substr(close_pos + 8);
                                    if (!after_think.empty() && after_think[0] == '\n') {
                                        after_think = after_think.substr(1);
                                    }
                                    last_msg.content = after_think;
                                    last_msg.show_thinking = false;
                                    in_think_block = false;
                                    // A stream_buffer-t reseteljuk a vegso valasz kezdetere,
                                    // hogy a kovetkezo chunk-ok ne tartalmazzak a thinking-et.
                                    stream_buffer = after_think;
                                } else {
                                    // Meg a thinking blokkban vagyunk
                                    last_msg.thinking_content = stream_buffer;
                                }
                            } else {
                                // Nincs <think> tag — sima tartalom
                                last_msg.content = stream_buffer;
                            }

                            // Tok/s szamitas + context szamlalo noveles
                            ++status_info->context_used;
                            const auto elapsed = std::chrono::steady_clock::now() - generation_start;
                            const auto elapsed_sec = std::chrono::duration<double>(elapsed).count();
                            if (elapsed_sec > 0.5) {
                                status_info->tokens_per_second =
                                    static_cast<double>(generated_token_count) / elapsed_sec;
                            }
                        }
                        screen.Post(ftxui::Event::Custom);

                        if (chunk.terminal) {
                            std::lock_guard lock(chat_mutex);
                            auto& last_msg = chat_state->messages.back();
                            // Ha az Esc mar lekezelte (is_streaming = false), ne duplikaljuk
                            bool already_finalized = !last_msg.is_streaming;
                            last_msg.is_streaming = false;
                            // Ha a gondolkodas lezaratlan maradt (nincs </think>), jelezzuk
                            if (in_think_block && last_msg.content.empty()) {
                                last_msg.show_thinking = true;  // Thinking marad lathato
                            }
                            chat_state->generating = false;
                            // Chat history: asszisztensi valasz mentese JSONL-be
                            // (csak ha az Esc meg nem mentette — dupla-append megelozes)
                            if (!already_finalized && !args.chat_history_file.empty() &&
                                (!last_msg.content.empty() || !last_msg.thinking_content.empty())) {
                                masodik::tui::chat_history::append(args.chat_history_file, last_msg);
                            }
                        }
                        return true;
                    },
                    [&]() -> bool { return !generating; });

            } catch (const std::exception& error) {
                std::lock_guard lock(chat_mutex);
                auto& last_msg = chat_state->messages.back();
                // Ha a felhasznalo szakitotta meg (Esc → generating = false),
                // az NEM hiba — nem irjuk ki hibauzenetkent.
                if (!generating) {
                    tui_log("generation MEGSZAKITVA (felhasznalo)");
                    last_msg.is_streaming = false;
                } else {
                    auto err_str = std::string{error.what()};
                    tui_log("generation HIBA: " + err_str);
                    // Baratsagos hibauzenet a kontextus tullepesre
                    if (err_str.find("context") != std::string::npos ||
                        err_str.find("budget") != std::string::npos) {
                        last_msg.content += "\nAz uzenet tul nagy a modell kontextusablakahoz. Probalj rovidebb szoveget kuldeni.";
                    } else {
                        last_msg.content += "\n[HIBA: " + err_str + "]";
                    }
                    last_msg.is_streaming = false;
                }
                chat_state->generating = false;
            }

            tui_log("generation VEGE: tokens=" + std::to_string(generated_token_count.load()));
            generating = false;
            status_info->state = "Kesz";
            status_info->tokens_per_second = 0.0;
            screen.Post(ftxui::Event::Custom);
        }).detach();
    };

    // --- FTXUI komponensek ---
    auto input_option = ftxui::InputOption::Default();
    input_option.placeholder = "";
    input_option.multiline = true;
    // A fokuszalt input ne legyen inverz (feher hatter) — normal megjelenes.
    // Normal megjelenes: nincs inverz hatter.
    input_option.transform = [](ftxui::InputState state) {
        return state.element;
    };
    input_content = " ";
    auto input_component = ftxui::Input(&input_content, input_option);

    // Fo komponens: mindig a chat kepernyo (nincs betoltesi kepernyo, lazy loading)
    auto main_renderer = ftxui::Renderer(input_component, [&] {
        // Chat mod
        ftxui::Element chat_area;
        {
            std::lock_guard lock(chat_mutex);
            chat_area = masodik::tui::render_chat_screen(*chat_state, status_info->iter, browse_index);
        }

        // Beviteli mezo: paragraph() alapu rendereles a sortordeleshez.
        // Az FTXUI Input komponens kezeli a billentyuzet inputot, de a vizualis
        // megjelenitest mi vegezzuk paragraph()-tal, mert az Input nem tordeli
        // a hosszu sorokat (horizontalisan gorget).
        // Ha a szoveg tobb mint 3 sort tartalmaz, osszevont megjelenitest mutatunk.
        auto make_input_display = [&](bool show_cursor) -> std::string {
            auto trimmed = input_content;
            while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(0, 1);

            // Sorok szamolasa
            int line_count = 1;
            for (const char ch : trimmed) {
                if (ch == '\n') ++line_count;
            }

            if (line_count > 3) {
                // Osszevont megjelentes: elso sor + sor szam
                auto first_nl = trimmed.find('\n');
                auto first_line = (first_nl != std::string::npos)
                    ? trimmed.substr(0, first_nl) : trimmed;
                if (first_line.size() > 60) first_line = first_line.substr(0, 57) + "...";
                auto chars = std::to_string(trimmed.size());
                return first_line + "  [" + std::to_string(line_count) + " sor, " + chars + " kar.]" +
                    (show_cursor ? "\u2588" : "");
            }

            return input_content + (show_cursor ? "\u2588" : "");
        };

        ftxui::Element input_area;
        if (generating || loading_in_progress) {
            input_area = ftxui::hbox({
                ftxui::text(" > ") | ftxui::dim | ftxui::color(ftxui::Color::GrayDark),
                ftxui::paragraph(make_input_display(false)) | ftxui::dim | ftxui::flex,
            });
        } else if (browse_index >= 0) {
            input_area = ftxui::hbox({
                ftxui::text(" > ") | ftxui::dim | ftxui::color(ftxui::Color::GrayDark),
                ftxui::paragraph(make_input_display(false)) | ftxui::dim | ftxui::flex,
            });
        } else {
            input_area = ftxui::hbox({
                ftxui::text(" > ") | ftxui::bold | ftxui::color(ftxui::Color::Green),
                ftxui::paragraph(make_input_display(cursor_visible.load())) | ftxui::flex,
            }) | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 8);
        }

        auto base_ui = ftxui::vbox({
            chat_area | ftxui::flex,
            ftxui::separator(),
            input_area,
            ftxui::separator(),
            masodik::tui::render_status_bar(*status_info),
            masodik::tui::render_function_key_bar(),
        });

        // Modal ablakok
        if (modal_mode == ModalMode::help) {
            return ftxui::dbox({
                base_ui | ftxui::dim,
                masodik::tui::render_help_screen() | ftxui::clear_under | ftxui::center,
            });
        }
        if (modal_mode == ModalMode::quit_confirm) {
            auto quit_dialog = ftxui::vbox({
                ftxui::text("  Biztosan ki szeretnel lepni?  ") | ftxui::bold | ftxui::center,
                ftxui::separator(),
                ftxui::hbox({
                    ftxui::text("  I") | ftxui::bold | ftxui::color(ftxui::Color::Green),
                    ftxui::text(" = Igen, kilepes   "),
                    ftxui::text("N") | ftxui::bold | ftxui::color(ftxui::Color::Red),
                    ftxui::text(" = Nem, maradok  "),
                }) | ftxui::center,
            }) | ftxui::border | ftxui::clear_under | ftxui::center;
            return ftxui::dbox({ base_ui | ftxui::dim, quit_dialog });
        }
        if (modal_mode == ModalMode::diagnostics) {
            auto health = pipeline ? pipeline->make_health_response() : masodik::api::HealthResponse{};

            // Embedding allapot kozvetlenul az EmbeddingPipelineService-bol
            std::string emb_state = "Nem konfiguralt";
            std::string emb_model;
            std::string emb_device;
            std::string emb_dimension;
            if (embedding) {
                const auto emb_info = embedding->model_info();
                emb_state = std::string{embedding->state_string()};
                emb_model = emb_info.model_name;
                emb_device = emb_info.device;
                if (emb_info.embedding_dimension > 0) {
                    emb_dimension = std::to_string(emb_info.embedding_dimension) + " dim";
                }
                if (!embedding->last_error().empty()) {
                    emb_state += " (" + embedding->last_error() + ")";
                }
            }

            auto emb_color = ftxui::Color::Yellow;
            if (emb_state == "ready") emb_color = ftxui::Color::Green;
            else if (emb_state.find("failed") != std::string::npos) emb_color = ftxui::Color::Red;

            ftxui::Elements diag_rows = {
                ftxui::text("  Diagnosztika  ") | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
                ftxui::separator(),
                ftxui::text("  Chat modell") | ftxui::bold,
                ftxui::hbox({ ftxui::text("    Profil: ") | ftxui::dim, ftxui::text(health.active_profile) }),
                ftxui::hbox({ ftxui::text("    Modell: ") | ftxui::dim, ftxui::text(health.active_model) }),
                ftxui::hbox({ ftxui::text("    Eszkoz: ") | ftxui::dim, ftxui::text(health.effective_device) }),
                ftxui::hbox({ ftxui::text("    Allapot: ") | ftxui::dim, ftxui::text(health.state) | ftxui::color(health.ready ? ftxui::Color::Green : ftxui::Color::Red) }),
                ftxui::hbox({ ftxui::text("    Stage-1: ") | ftxui::dim, ftxui::text(health.stage_1_state) }),
                ftxui::hbox({ ftxui::text("    Stage-2: ") | ftxui::dim, ftxui::text(health.stage_2_state) }),
                ftxui::hbox({ ftxui::text("    Context: ") | ftxui::dim, ftxui::text(std::to_string(health.validated_context_tokens) + " token") }),
                ftxui::separator(),
                ftxui::text("  Embedding modell") | ftxui::bold,
                ftxui::hbox({ ftxui::text("    Modell: ") | ftxui::dim, ftxui::text(emb_model.empty() ? "-" : emb_model) }),
                ftxui::hbox({ ftxui::text("    Eszkoz: ") | ftxui::dim, ftxui::text(emb_device.empty() ? "-" : emb_device) }),
                ftxui::hbox({ ftxui::text("    Dimenzio: ") | ftxui::dim, ftxui::text(emb_dimension.empty() ? "-" : emb_dimension) }),
                ftxui::hbox({ ftxui::text("    Allapot: ") | ftxui::dim, ftxui::text(emb_state) | ftxui::color(emb_color) }),
                ftxui::separator(),
                ftxui::text("  Halozat") | ftxui::bold,
                ftxui::hbox({ ftxui::text("    Web szerver: ") | ftxui::dim, ftxui::text(web_server_running ? ("Aktiv — " + service_config.bind_host + ":" + std::to_string(service_config.bind_port)) : "Kikapcsolva (F9)") }),
                ftxui::separator(),
                ftxui::text("  Nyomj Enter-t vagy Esc-et a bezarashoz.") | ftxui::dim | ftxui::center,
            };

            auto diag = ftxui::vbox(std::move(diag_rows)) | ftxui::border | ftxui::clear_under | ftxui::center;
            return ftxui::dbox({ base_ui | ftxui::dim, diag });
        }
        if (modal_mode == ModalMode::edit_confirm) {
            auto edit_dialog = ftxui::vbox({
                ftxui::text("  Korabbi uzenet ujrakuldese  ") | ftxui::bold | ftxui::center,
                ftxui::separator(),
                ftxui::text("  " + std::to_string(pending_edit_loss_count) +
                    " uzenet visszavonhatatlanul torlodni fog.") | ftxui::center,
                ftxui::text("") | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1),
                ftxui::hbox({
                    ftxui::text("  I") | ftxui::bold | ftxui::color(ftxui::Color::Green),
                    ftxui::text(" = Igen, kuldes   "),
                    ftxui::text("N") | ftxui::bold | ftxui::color(ftxui::Color::Red),
                    ftxui::text(" = Nem, megse  "),
                }) | ftxui::center,
            }) | ftxui::border | ftxui::clear_under | ftxui::center;
            return ftxui::dbox({ base_ui | ftxui::dim, edit_dialog });
        }
        if (modal_mode == ModalMode::context_overflow) {
            auto est_k = std::to_string(overflow_estimated_tokens / 1000) + "k";
            auto max_k = std::to_string(overflow_max_tokens / 1000) + "k";
            auto overflow_dialog = ftxui::vbox({
                ftxui::text("  Tul nagy uzenet  ") | ftxui::bold | ftxui::color(ftxui::Color::Yellow) | ftxui::center,
                ftxui::separator(),
                ftxui::text("  Becsult meret: ~" + est_k + " token") | ftxui::center,
                ftxui::text("  Kontextus limit: " + max_k + " token") | ftxui::center,
                ftxui::text("") | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1),
                ftxui::text("  Az uzenet nem fer a modell kontextusablakaba.") | ftxui::dim | ftxui::center,
                ftxui::text("  Csonkold le a szoveget vagy torolj (Ctrl+U).") | ftxui::dim | ftxui::center,
                ftxui::text("") | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1),
                ftxui::hbox({
                    ftxui::text("  I") | ftxui::bold | ftxui::color(ftxui::Color::Green),
                    ftxui::text(" = Megis kuldom   "),
                    ftxui::text("N") | ftxui::bold | ftxui::color(ftxui::Color::Red),
                    ftxui::text(" = Megse  "),
                }) | ftxui::center,
            }) | ftxui::border | ftxui::clear_under | ftxui::center;
            return ftxui::dbox({ base_ui | ftxui::dim, overflow_dialog });
        }

        return base_ui;
    });

    // Billentyu kezeles
    auto event_handler = ftxui::CatchEvent(main_renderer, [&](ftxui::Event event) -> bool {
        // F10 mindig a kilepes — modalban is, barhol is
        if (event == ftxui::Event::F10) {
            if (modal_mode == ModalMode::quit_confirm) {
                // Mar megerositesben vagyunk — kilep
                generating = false;
                screen.Exit();
                return true;
            }
            modal_mode = ModalMode::quit_confirm;
            return true;
        }

        // Modal ablak bezarasa Enter-rel vagy Esc-kel (help, diagnostics)
        if (modal_mode == ModalMode::help || modal_mode == ModalMode::diagnostics) {
            if (event == ftxui::Event::Return || event == ftxui::Event::Escape) {
                modal_mode = ModalMode::none;
                return true;
            }
            return true;
        }

        // Szerkesztes megerosites (korabbi uzenet ujrakuldese)
        if (modal_mode == ModalMode::edit_confirm) {
            if (event.is_character()) {
                const auto ch = event.character();
                if (ch == "i" || ch == "I") {
                    // Megerositett: torlunk es kuldunk
                    {
                        std::lock_guard lock(chat_mutex);
                        if (edit_source_msg_idx >= 0 &&
                            edit_source_msg_idx < static_cast<int>(chat_state->messages.size())) {
                            chat_state->messages.erase(
                                chat_state->messages.begin() + edit_source_msg_idx,
                                chat_state->messages.end());
                            if (!args.chat_history_file.empty()) {
                                masodik::tui::chat_history::rewrite(
                                    args.chat_history_file, chat_state->messages);
                            }
                        }
                        tui_log("Edit confirm: torolve " + std::to_string(pending_edit_loss_count) + " uzenet, msgs_left=" + std::to_string(chat_state->messages.size()));
                    }
                    edit_source_msg_idx = -1;
                    modal_mode = ModalMode::none;
                    send_message();
                    return true;
                }
                // Barmi mas → megse, visszateres szerkesztesbe
                modal_mode = ModalMode::none;
                return true;
            }
            if (event == ftxui::Event::Escape) {
                modal_mode = ModalMode::none;
                return true;
            }
            return true;
        }

        // Kontextus tulcsordulas kezeles
        if (modal_mode == ModalMode::context_overflow) {
            if (event.is_character()) {
                const auto ch = event.character();
                if (ch == "i" || ch == "I") {
                    // Megis kuldi — a pipeline hibat fog dobni, de a felhasznalo tudja
                    modal_mode = ModalMode::none;
                    // Az edit_confirm-ot is le kell kezelni ha kell
                    if (edit_source_msg_idx >= 0) {
                        std::lock_guard lock(chat_mutex);
                        int msgs_after = static_cast<int>(chat_state->messages.size()) - edit_source_msg_idx;
                        if (msgs_after > 0) {
                            pending_edit_loss_count = msgs_after;
                            modal_mode = ModalMode::edit_confirm;
                            return true;
                        }
                        edit_source_msg_idx = -1;
                    }
                    send_message();
                    return true;
                }
                // Barmi mas → megse, visszateres a szerkesztesbe
                modal_mode = ModalMode::none;
                return true;
            }
            if (event == ftxui::Event::Escape) {
                modal_mode = ModalMode::none;
                return true;
            }
            return true;
        }

        // Kilepes megerosites kezeles (F10-bol erkeztunk ide)
        if (modal_mode == ModalMode::quit_confirm) {
            if (event.is_character()) {
                const auto ch = event.character();
                if (ch == "i" || ch == "I") {
                    generating = false;
                    screen.Exit();
                    return true;
                }
                // Barmi mas → maradunk
                modal_mode = ModalMode::none;
                return true;
            }
            if (event == ftxui::Event::Escape || event == ftxui::Event::Return) {
                modal_mode = ModalMode::none;
                return true;
            }
            return true;
        }

        // Ctrl+C → FTXUI kijeloles masolasa a Windows vagolpara.
        // Ha nincs kijeloles, csendben elnyeljuk.
        if (event == ftxui::Event::CtrlC) {
            auto selected = screen.GetSelection();
            if (!selected.empty()) {
                // UTF-8 → UTF-16 konverzio es vagolpara helyezes
                int wide_len = MultiByteToWideChar(CP_UTF8, 0, selected.c_str(),
                    static_cast<int>(selected.size()), nullptr, 0);
                if (wide_len > 0 && OpenClipboard(nullptr)) {
                    EmptyClipboard();
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wide_len + 1) * sizeof(wchar_t));
                    if (hMem) {
                        auto* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
                        MultiByteToWideChar(CP_UTF8, 0, selected.c_str(),
                            static_cast<int>(selected.size()), pMem, wide_len);
                        pMem[wide_len] = L'\0';
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                    CloseClipboard();

                    int lines = 1;
                    for (const char ch : selected) { if (ch == '\n') ++lines; }
                    status_info->hint_message = std::to_string(lines) + " sor, " +
                        std::to_string(selected.size()) + " kar. a vagolpara kerult.";
                }
            }
            return true;
        }

        // Esc → generalas megszakitasa (US6) / navigacio megszakitasa (US7) / modal
        if (event == ftxui::Event::Escape) {
            if (generating) {
                // Generalas megszakitasa — a reszleges valasz megmarad
                generating = false;
                {
                    std::lock_guard lock(chat_mutex);
                    if (!chat_state->messages.empty()) {
                        auto& last_msg = chat_state->messages.back();
                        last_msg.is_streaming = false;
                        // Reszleges valasz mentese JSONL-be
                        if (!args.chat_history_file.empty() && !last_msg.content.empty()) {
                            masodik::tui::chat_history::append(args.chat_history_file, last_msg);
                        }
                    }
                    chat_state->generating = false;
                }
                status_info->state = "Kesz";
                status_info->tokens_per_second = 0.0;
                screen.Post(ftxui::Event::Custom);
            } else if (browse_index >= 0) {
                // Navigacio megszakitasa — visszateres a beviteli mezore
                browse_index = -1;
                status_info->hint_message.clear();
            }
            return true;
        }

        // Ctrl+U → beviteli mezo torlese
        if (event == ftxui::Event::CtrlU) {
            input_content = " ";  // Space workaround
            edit_source_msg_idx = -1;
            return true;
        }

        // --- Beillesztes (paste) es Enter kezeles ---
        // Windows ConPTY-n a beillesztett szoveg karakterenkent erkezik.
        // Idozites-alapu detektalas: ha az Enter 20ms-on belul erkezik
        // az elozo karakter utan, az beillesztes resze (nem kuldes).
        // A last_input_time-ot minden karakter eventnel frissitjuk.
        {
            static auto last_input_time = std::chrono::steady_clock::time_point{};

            // Bracketed paste (ha az FTXUI tamogatja): tobb karakter egyetlen eventben
            const auto& inp = event.input();
            if (inp.size() > 1 && !inp.empty() && inp[0] != '\x1b' &&
                !generating && !loading_in_progress && browse_index < 0) {
                for (const char ch : inp) {
                    if (ch == '\r') continue;
                    input_content += ch;
                }
                last_input_time = std::chrono::steady_clock::now();
                return true;
            }

            // Karakter event → frissitjuk az idopecsetet
            if (event.is_character()) {
                last_input_time = std::chrono::steady_clock::now();
            }

            // Enter: ha a korabbi karakter 20ms-on belul erkezett → beillesztes resze
            if (event == ftxui::Event::Return &&
                !generating && !loading_in_progress && browse_index < 0) {
                auto now = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_input_time).count();
                if (ms < 20 && last_input_time.time_since_epoch().count() > 0) {
                    // Paste kozben: Enter → ujsor a beviteli mezoben
                    input_content += "\n";
                    last_input_time = now;
                    return true;
                }
            }
        }

        // Shift+Enter / Alt+Enter → uj sor beszurasa a beviteli mezobe.
        {
            const auto& inp = event.input();
            bool is_alt_enter = (inp == "\x1b\r" || inp == "\x1b\n");
            bool is_shift_enter = (inp == "\x1b[13;2u");
            if (is_alt_enter || is_shift_enter) {
                input_content += "\n";
                return true;
            }
        }

        // F1 → surgo
        if (event == ftxui::Event::F1) {
            modal_mode = ModalMode::help;
            return true;
        }

        // F2 → uj chat
        if (event == ftxui::Event::F2 && !generating) {
            std::lock_guard lock(chat_mutex);
            chat_state->messages.clear();
            input_content = " ";  // Space workaround a kurzor lathatosagaert
            status_info->context_used = 0;
            edit_source_msg_idx = -1;
            browse_index = -1;
            return true;
        }

        // F9 → webszerver be/ki
        if (event == ftxui::Event::F9 && pipeline && pipeline->ready()) {
            if (web_server_running) {
                // Webszerver leallitasa
                if (web_server) {
                    web_server->stop();
                    web_server.reset();
                }
                web_server_running = false;
                status_info->web_server_active = false;
            } else {
                // Webszerver inditasa
                try {
                    web_server = std::make_unique<masodik::server::HttpServer>();
                    masodik::server::register_openai_routes(
                        *web_server, service_config, *pipeline, {},
                        embedding.get());
                    web_server->start({
                        .bind_host = service_config.bind_host,
                        .bind_port = service_config.bind_port});
                    web_server_running = true;
                    status_info->web_server_active = true;
                    status_info->web_server_port = service_config.bind_port;
                } catch (...) {
                    web_server.reset();
                    web_server_running = false;
                }
            }
            return true;
        }

        // F5 → modell ujratoltés (offload + lazy reload)
        if (event == ftxui::Event::F5 && !generating && !loading_in_progress) {
            if (web_server) { web_server->stop(); web_server.reset(); }
            web_server_running = false;
            status_info->web_server_active = false;
            // Offload a jelenlegi pipeline-t
            offload_pipeline();
            // Azonnal ujratoltjuk
            ensure_pipeline_loaded();
            return true;
        }

        // F12 → diagnosztika
        if (event == ftxui::Event::F12) {
            modal_mode = ModalMode::diagnostics;
            return true;
        }

        // --- Uzenet navigacio (US7) ---
        // Segedfuggveny: felhasznaloi uzenetek szama
        auto count_user_messages = [&]() -> int {
            int count = 0;
            for (const auto& m : chat_state->messages) {
                if (m.role == masodik::tui::ChatMessage::Role::user) ++count;
            }
            return count;
        };

        // Segedfuggveny: az n-edik felhasznaloi uzenet indexe a messages tombben
        auto user_message_index = [&](int n) -> int {
            int count = -1;
            for (int j = 0; j < static_cast<int>(chat_state->messages.size()); ++j) {
                if (chat_state->messages[j].role == masodik::tui::ChatMessage::Role::user) {
                    ++count;
                    if (count == n) return j;
                }
            }
            return -1;
        };

        // --- Uzenet navigacio (US7) ---
        // Felfele nyil: navigacio inditasa CSAK ures beviteli mezobol.
        // Ha mar browse modban vagyunk, tovabb navigalunk felfele.
        if (event == ftxui::Event::ArrowUp && !generating) {
            if (browse_index >= 0) {
                // Mar browse modban — elozo uzenet
                if (browse_index > 0) {
                    --browse_index;
                }
                return true;
            }
            // Uj browse mod inditas — csak ures mezobol
            auto trimmed = input_content;
            while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(0, 1);
            if (trimmed.empty()) {
                int user_count = count_user_messages();
                if (user_count > 0) {
                    browse_index = user_count - 1;
                    status_info->hint_message = "Fel/Le: navigacio, Enter: ujrakuldes, Esc: megse";
                    tui_log("ArrowUp: browse_index=" + std::to_string(browse_index) + " user_count=" + std::to_string(user_count));
                    return true;
                }
            }
        }

        // Lefele nyil: navigacio visszafele vagy visszateres a beviteli mezore
        if (event == ftxui::Event::ArrowDown && !generating && browse_index >= 0) {
            int user_count = count_user_messages();
            if (browse_index < user_count - 1) {
                ++browse_index;
            } else {
                // Visszateres a beviteli mezore
                browse_index = -1;
                status_info->hint_message.clear();
            }
            return true;
        }

        // Enter → navigacio kozben: uzenet betoltese a beviteli mezobe es AZONNALI kuldese
        if (event == ftxui::Event::Return && !generating && !loading_in_progress) {
            if (browse_index >= 0) {
                // A kivalasztott uzenet BETOLTESE a beviteli mezobe szerkesztesre.
                // SEMMI nem torlodik meg — a torles csak a kuldeskor tortenik (megerositessel).
                {
                    std::lock_guard lock(chat_mutex);
                    int msg_idx = user_message_index(browse_index);
                    if (msg_idx >= 0) {
                        input_content = chat_state->messages[msg_idx].content;
                        edit_source_msg_idx = msg_idx;  // Megjegyezzuk honnan toltottuk
                    }
                    tui_log("Browse Load: msg_idx=" + std::to_string(msg_idx) + " input='" + input_content.substr(0, 40) + "'");
                }

                browse_index = -1;
                status_info->hint_message.clear();
                return true;
            }

            // Normal uzenet kuldese
            if (!input_content.empty()) {
                // Kontextus meret ellenorzes kuldes elott
                // Hozzavetoleges becsles: 1 token ~ 4 karakter
                {
                    int total_chars = static_cast<int>(input_content.size());
                    total_chars += static_cast<int>(system_prompt_content.size());
                    {
                        std::lock_guard lock(chat_mutex);
                        for (const auto& m : chat_state->messages) {
                            total_chars += static_cast<int>(m.content.size());
                        }
                    }
                    int estimated_tokens = total_chars / 4;
                    int max_context = service_config.generation_config.desired_context_tokens;
                    if (estimated_tokens > max_context * 9 / 10) {
                        // Tul nagy — figyelmeztetjuk a felhasznalot
                        overflow_estimated_tokens = estimated_tokens;
                        overflow_max_tokens = max_context;
                        modal_mode = ModalMode::context_overflow;
                        return true;
                    }
                }

                // Ha korabbi uzenetet szerkesztunk, megerositest kerunk a torlesrol
                if (edit_source_msg_idx >= 0) {
                    std::lock_guard lock(chat_mutex);
                    int msgs_after = static_cast<int>(chat_state->messages.size()) - edit_source_msg_idx;
                    if (msgs_after > 0) {
                        pending_edit_loss_count = msgs_after;
                        modal_mode = ModalMode::edit_confirm;
                        return true;
                    }
                    edit_source_msg_idx = -1;
                }
                send_message();
                return true;
            }
        }

        // Generalas, betoltes vagy browse mod kozben a beviteli mezo inaktiv.
        if (generating || loading_in_progress || browse_index >= 0) {
            return true;
        }

        // --- Sajat karakter-bevitel kezeles ---
        // Az FTXUI Input komponens belso kurzorpozicioja elcsuszhat ha mi
        // kulon modositjuk az input_content-et (paste, Ctrl+U, browse betoltes).
        // Ezert MINDEN karakter inputot mi kezelunk kozvetlenul — az Input
        // komponens soha nem kap eventet.

        // Backspace → utolso UTF-8 karakter torlese
        if (event == ftxui::Event::Backspace) {
            if (input_content.size() > 1) {  // Minimum a space workaround marad
                // UTF-8 tudatos torles: a folytato byte-okat (10xxxxxx) atlepjuk
                auto i = input_content.size() - 1;
                while (i > 0 && (static_cast<unsigned char>(input_content[i]) & 0xC0) == 0x80) {
                    --i;
                }
                // Ha a torles utan csak space maradna, megtartjuk
                if (i == 0) {
                    input_content = " ";
                } else {
                    input_content.erase(i);
                }
            }
            return true;
        }

        // Karakter bevitel → hozzafuzes az input vegehez
        if (event.is_character()) {
            input_content += event.character();
            return true;
        }

        // Eger eventek → elnyeljuk (return true). Az FTXUI belso selection
        // kezeloje (HandleSelection) MAR LEFUTOTT mielott a CatchEvent handler
        // megkapja az eventet. A return true megakadalyozza hogy az Input
        // komponens CaptureMouse()-szal zavarja a kijelolest.
        if (event.is_mouse()) {
            return true;
        }

        // Egyeb billentyuk (Tab, stb.) → elnyeljuk
        return true;
    });

    // --- Eroforras monitor periodikus frissitese ---
    // Masodpercenkent lekerdezes, az allapotsor frissitese.
    auto resource_monitor = std::make_shared<masodik::tui::ResourceMonitor>();
    std::atomic<bool> monitor_running = true;
    int hint_clear_countdown = 0;
    // Az elozo eroforras-pillanatkep — osszehasonlitashoz, hogy kell-e ujrarajzolni
    masodik::tui::ResourceSnapshot prev_resources{};
    int prev_uptime = 0;
    std::thread monitor_thread([&]() {
        while (monitor_running) {
            bool needs_redraw = false;

            auto new_resources = resource_monitor->snapshot();
            const auto elapsed = std::chrono::steady_clock::now() - app_start_time;
            int new_uptime = static_cast<int>(
                std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());

            // Eroforras frissites — csak ha valtozott (valodi delta detektalas)
            status_info->resources = new_resources;
            status_info->uptime_seconds = new_uptime;

            // Eroforras valtozas detektalas (kerulunk felesleges ujrarajzolast)
            if (new_uptime != prev_uptime) {
                needs_redraw = true;
                prev_uptime = new_uptime;
            }

            // Ha a modell betoltodott es van varakozo uzenet
            if (pending_send && pipeline_loaded && !generating && !loading_in_progress && pipeline && pipeline->ready()) {
                pending_send = false;
                {
                    std::lock_guard lock(chat_mutex);
                    if (!chat_state->messages.empty()) {
                        auto& ai_msg = chat_state->messages.back();
                        if (ai_msg.role == masodik::tui::ChatMessage::Role::assistant) {
                            ai_msg.content.clear();
                        }
                    }
                }
                start_generation();
                needs_redraw = true;
            }

            // Hint uzenet automatikus torlese 3 masodperc utan
            if (!status_info->hint_message.empty()) {
                ++hint_clear_countdown;
                if (hint_clear_countdown >= 3) {
                    status_info->hint_message.clear();
                    hint_clear_countdown = 0;
                    needs_redraw = true;
                }
            } else {
                hint_clear_countdown = 0;
            }

            // Auto-offload: 10 percig nem volt hasznalva
            if (pipeline_loaded && !generating && !loading_in_progress) {
                const auto idle_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - last_activity_time).count();
                if (idle_seconds >= offload_timeout_seconds) {
                    offload_pipeline();
                    needs_redraw = true;
                }
            }

            // Kurzor villogas — csak ha nincs browse mod es nem general
            bool new_cursor = !cursor_visible.load();
            if (new_cursor != cursor_visible.load()) {
                cursor_visible = new_cursor;
                if (browse_index < 0 && !generating) {
                    needs_redraw = true;
                }
            }

            // CSAK valtozas eseten rajzolunk ujra — ez megvedi a kijelolest
            if (needs_redraw) {
                screen.Post(ftxui::Event::Custom);
            }

            const int sleep_ms = 1000;
            for (int i = 0; i < sleep_ms / 100 && monitor_running; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
            }
        }
    });

    // --- FTXUI Selection → Windows vagolap ---
    // Amikor a felhasznalo egerrel kijelol szoveget, az FTXUI belso
    // selection_data_ tarolja a koordinatakat. A SelectionChange callback
    // automatikusan a vagolpara masolja a kijelolt szoveget.
    screen.SelectionChange([&]() {
        auto selected = screen.GetSelection();
        if (selected.empty()) return;

        int wide_len = MultiByteToWideChar(CP_UTF8, 0, selected.c_str(),
            static_cast<int>(selected.size()), nullptr, 0);
        if (wide_len > 0 && OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wide_len + 1) * sizeof(wchar_t));
            if (hMem) {
                auto* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
                MultiByteToWideChar(CP_UTF8, 0, selected.c_str(),
                    static_cast<int>(selected.size()), pMem, wide_len);
                pMem[wide_len] = L'\0';
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();

            int lines = 1;
            for (const char ch : selected) { if (ch == '\n') ++lines; }
            status_info->hint_message = std::to_string(lines) + " sor, " +
                std::to_string(selected.size()) + " kar. a vagolpara kerult.";
        }
    });

    screen.Loop(event_handler);

    monitor_running = false;
    generating = false;
    SetConsoleCtrlHandler(tui_console_ctrl_handler, FALSE);
    g_status_info.reset();
    g_screen = nullptr;
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    if (web_server) {
        web_server->stop();
        web_server.reset();
    }

    return 0;
}
