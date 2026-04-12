#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "tui_app.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
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

    enum class ModalMode { none, help, quit_confirm, diagnostics };
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
                .content = user_text,
            });
            // Asszisztensi uzenet — a varakozasi allapotot mutatja
            chat_state->messages.push_back({
                .role = masodik::tui::ChatMessage::Role::assistant,
                .is_streaming = true,
            });
            chat_state->generating = true;
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
                request.max_tokens = 1024;

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
                            last_msg.is_streaming = false;
                            chat_state->generating = false;
                        }
                        return true;
                    },
                    [&]() -> bool { return !generating; });

            } catch (const std::exception& error) {
                std::lock_guard lock(chat_mutex);
                auto& last_msg = chat_state->messages.back();
                last_msg.content += "\n[HIBA: " + std::string{error.what()} + "]";
                last_msg.is_streaming = false;
                chat_state->generating = false;
            }

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
            chat_area = masodik::tui::render_chat_screen(*chat_state, status_info->iter);
        }

        auto base_ui = ftxui::vbox({
            chat_area | ftxui::flex,
            ftxui::separator(),
            ftxui::hbox({
                ftxui::text(" > ") | ftxui::bold | ftxui::color(ftxui::Color::Green),
                cursor_visible.load()
                    ? (input_component->Render() | ftxui::flex)
                    : (ftxui::text(input_content) | ftxui::flex),
            }),
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

        // Modal ablak bezarasa Enter-rel vagy Esc-kel
        if (modal_mode != ModalMode::none && modal_mode != ModalMode::quit_confirm) {
            if (event == ftxui::Event::Return || event == ftxui::Event::Escape) {
                modal_mode = ModalMode::none;
                return true;
            }
            return true;  // Modal modban minden billentyut elnyeljuk
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

        // Ctrl+C → hint uzenet, NEM kilepes
        if (event == ftxui::Event::CtrlC) {
            status_info->hint_message = "Kilepeshez hasznald az F10 billentyut.";
            return true;  // Elnyeljuk — nem lep ki
        }

        // Esc → modal ablakok bezarasa (NEM kilepes!)
        if (event == ftxui::Event::Escape) {
            return true;  // Elnyeljuk
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

        // Enter → uzenet kuldese (ha nem ures es nem general)
        if (event == ftxui::Event::Return && !generating && !loading_in_progress) {
            if (!input_content.empty()) {
                send_message();
                return true;
            }
        }

        return false;
    });

    // --- Eroforras monitor periodikus frissitese ---
    // Masodpercenkent lekerdezes, az allapotsor frissitese.
    auto resource_monitor = std::make_shared<masodik::tui::ResourceMonitor>();
    std::atomic<bool> monitor_running = true;
    int hint_clear_countdown = 0;
    std::thread monitor_thread([&]() {
        while (monitor_running) {
            status_info->resources = resource_monitor->snapshot();
            // Uptime frissites
            const auto elapsed = std::chrono::steady_clock::now() - app_start_time;
            status_info->uptime_seconds = static_cast<int>(
                std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());

            // Ha a modell betoltodott es van varakozo uzenet, a streaming generalast elindítjuk
            if (pending_send && pipeline_loaded && !generating && !loading_in_progress && pipeline && pipeline->ready()) {
                pending_send = false;
                // Az AI uzenet frissitese
                {
                    std::lock_guard lock(chat_mutex);
                    if (!chat_state->messages.empty()) {
                        auto& ai_msg = chat_state->messages.back();
                        if (ai_msg.role == masodik::tui::ChatMessage::Role::assistant) {
                            ai_msg.content.clear();  // Toroljuk a "Varakozas" szoveget
                        }
                    }
                }
                // A start_generation lambda-t hivjuk (lasd lentebb)
                start_generation();
            }

            // Hint uzenet automatikus torlese 3 masodperc utan
            if (!status_info->hint_message.empty()) {
                ++hint_clear_countdown;
                if (hint_clear_countdown >= 3) {
                    status_info->hint_message.clear();
                    hint_clear_countdown = 0;
                }
            } else {
                hint_clear_countdown = 0;
            }

            // Auto-offload: ha a pipeline betoltve van es 10 percig nem volt hasznalva
            if (pipeline_loaded && !generating && !loading_in_progress) {
                const auto idle_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - last_activity_time).count();
                if (idle_seconds >= offload_timeout_seconds) {
                    offload_pipeline();
                }
            }

            // Kurzor villogas: stabil allapotvaltas masodpercenkent.
            // A transform olvassa a cursor_visible erteket es donti el
            // hogy a focus (kurzor) lathato-e az input mezoben.
            cursor_visible = !cursor_visible.load();

            screen.Post(ftxui::Event::Custom);
            const int sleep_ms = 1000;
            for (int i = 0; i < sleep_ms / 100 && monitor_running; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
            }
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
