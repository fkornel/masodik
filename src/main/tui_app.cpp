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

int run_tui_app(int argc, char** argv) {
    const auto args = parse_tui_arguments(argc, argv);

    auto service_config = masodik::runtime::build_service_config(
        args.profile_path, args.model_root, args.repo_root);

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    // --- Allapot ---
    auto loading_state = std::make_shared<masodik::tui::LoadingState>();
    loading_state->profile_name = service_config.active_profile;
    loading_state->model_name = service_config.active_model;

    auto chat_state = std::make_shared<masodik::tui::ChatState>();
    std::mutex chat_mutex;  // Chat allapot vedese a streaming szaltol

    auto status_info = std::make_shared<masodik::tui::StatusBarInfo>();
    status_info->model_name = service_config.active_model;
    status_info->device_profile = service_config.effective_device;
    status_info->context_total = service_config.generation_config.desired_context_tokens;
    status_info->state = "Betoltes...";

    // --- Pipeline ---
    std::shared_ptr<masodik::genai::LlmPipelineService> pipeline;
    std::shared_ptr<masodik::genai::EmbeddingPipelineService> embedding;

    std::atomic<bool> loading_done = false;
    std::atomic<bool> loading_error = false;
    std::string loading_error_message;

    // Chat input szoveg — az FTXUI Input komponens irja/olvassa
    std::string input_content;

    // Streaming allapot
    std::atomic<bool> generating = false;
    std::chrono::steady_clock::time_point generation_start;
    std::atomic<int> generated_token_count = 0;

    // Modal allapotok
    enum class ModalMode { none, help, quit_confirm, diagnostics };
    ModalMode modal_mode = ModalMode::none;

    // Webszerver allapot (F10)
    std::unique_ptr<masodik::server::HttpServer> web_server;
    bool web_server_running = false;

    // --- Betoltes szal ---
    std::thread loading_thread([&]() {
        try {
            loading_state->stage_1 = "Betoltes...";
            screen.Post(ftxui::Event::Custom);

            pipeline = std::make_shared<masodik::genai::LlmPipelineService>(service_config);
            pipeline->initialize();

            if (pipeline->ready()) {
                loading_state->stage_1 = "Kesz";
                loading_state->stage_2 = "Kesz";
            } else {
                loading_state->stage_1 = "Hiba";
                loading_error = true;
                loading_error_message = pipeline->status_message();
            }
            screen.Post(ftxui::Event::Custom);

            if (service_config.serving_profile.embedding_enabled) {
                loading_state->embedding = "Betoltes...";
                screen.Post(ftxui::Event::Custom);

                embedding = std::make_shared<masodik::genai::EmbeddingPipelineService>();
                embedding->initialize(
                    service_config.serving_profile.embedding_model_path,
                    service_config.serving_profile.embedding_device,
                    service_config.serving_profile.embedding_model_name,
                    service_config.serving_profile.embedding_reject_cpu_fallback);

                loading_state->embedding = embedding->ready() ? "Kesz" : "Hiba";
                screen.Post(ftxui::Event::Custom);
            }

            loading_done = true;
            status_info->state = "Kesz";
            screen.Post(ftxui::Event::Custom);
        } catch (const std::exception& error) {
            loading_error = true;
            loading_error_message = error.what();
            loading_done = true;
            screen.Post(ftxui::Event::Custom);
        }
    });

    // --- Uzenet kuldese a pipeline-nak ---
    auto send_message = [&]() {
        if (generating || !pipeline || !pipeline->ready()) return;
        std::string user_text;
        {
            std::lock_guard lock(chat_mutex);
            user_text = input_content;
            if (user_text.empty()) return;
            input_content.clear();

            // Felhasznaloi uzenet hozzaadasa
            chat_state->messages.push_back({
                .role = masodik::tui::ChatMessage::Role::user,
                .content = user_text,
            });
            // Ures asszisztensi uzenet a streaminghez
            chat_state->messages.push_back({
                .role = masodik::tui::ChatMessage::Role::assistant,
                .is_streaming = true,
            });
            chat_state->generating = true;
        }
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
        std::thread([&, user_text]() {
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
    input_option.placeholder = "Ird be az uzeneted...";
    input_option.multiline = false;
    auto input_component = ftxui::Input(&input_content, input_option);

    // Fo komponens: betoltes VAGY chat
    auto main_renderer = ftxui::Renderer(input_component, [&] {
        if (!loading_done) {
            return masodik::tui::render_loading_screen(*loading_state);
        }
        if (loading_error && chat_state->messages.empty()) {
            return ftxui::vbox({
                ftxui::text("  HIBA a modell betoltesenel  ") | ftxui::bold | ftxui::color(ftxui::Color::Red),
                ftxui::separator(),
                ftxui::paragraph(loading_error_message) | ftxui::color(ftxui::Color::Yellow),
                ftxui::separator(),
                ftxui::text("Nyomj Esc-et a kilepeshez.") | ftxui::dim,
            }) | ftxui::border;
        }

        // Chat mod
        ftxui::Element chat_area;
        {
            std::lock_guard lock(chat_mutex);
            chat_area = masodik::tui::render_chat_screen(*chat_state);
        }

        auto base_ui = ftxui::vbox({
            chat_area | ftxui::flex,
            ftxui::separator(),
            input_component->Render() | ftxui::border,
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
            auto diag = ftxui::vbox({
                ftxui::text("  Diagnosztika  ") | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
                ftxui::separator(),
                ftxui::hbox({ ftxui::text("  Profil: ") | ftxui::bold, ftxui::text(health.active_profile) }),
                ftxui::hbox({ ftxui::text("  Modell: ") | ftxui::bold, ftxui::text(health.active_model) }),
                ftxui::hbox({ ftxui::text("  Eszkoz: ") | ftxui::bold, ftxui::text(health.effective_device) }),
                ftxui::hbox({ ftxui::text("  Allapot: ") | ftxui::bold, ftxui::text(health.state) | ftxui::color(health.ready ? ftxui::Color::Green : ftxui::Color::Red) }),
                ftxui::hbox({ ftxui::text("  Stage-1: ") | ftxui::bold, ftxui::text(health.stage_1_state) }),
                ftxui::hbox({ ftxui::text("  Stage-2: ") | ftxui::bold, ftxui::text(health.stage_2_state) }),
                ftxui::hbox({ ftxui::text("  Context: ") | ftxui::bold, ftxui::text(std::to_string(health.validated_context_tokens) + " token") }),
                ftxui::hbox({ ftxui::text("  Embedding: ") | ftxui::bold, ftxui::text(health.embedding_state.empty() ? "N/A" : health.embedding_state) }),
                ftxui::hbox({ ftxui::text("  Web: ") | ftxui::bold, ftxui::text(web_server_running ? ("Aktiv :" + std::to_string(service_config.bind_port)) : "Kikapcsolva") }),
                ftxui::separator(),
                ftxui::text("  Nyomj Enter-t vagy Esc-et a bezarashoz.") | ftxui::dim | ftxui::center,
            }) | ftxui::border | ftxui::clear_under | ftxui::center;
            return ftxui::dbox({ base_ui | ftxui::dim, diag });
        }

        return base_ui;
    });

    // Billentyu kezeles
    auto event_handler = ftxui::CatchEvent(main_renderer, [&](ftxui::Event event) -> bool {
        // Modal ablak bezarasa Enter-rel vagy Esc-kel
        if (modal_mode != ModalMode::none && modal_mode != ModalMode::quit_confirm) {
            if (event == ftxui::Event::Return || event == ftxui::Event::Escape) {
                modal_mode = ModalMode::none;
                return true;
            }
            return true;  // Modal modban minden billentyut elnyeljuk
        }

        // Kilepes megerosites kezeles
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
            if (event == ftxui::Event::Escape) {
                modal_mode = ModalMode::none;
                return true;
            }
            return true;
        }

        // Esc → kilepes megerosites
        if (event == ftxui::Event::Escape) {
            modal_mode = ModalMode::quit_confirm;
            return true;
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
            input_content.clear();
            status_info->context_used = 0;
            return true;
        }

        // F10 → webszerver be/ki
        if (event == ftxui::Event::F10 && pipeline && pipeline->ready()) {
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

        // F5 → modell ujratoltés
        if (event == ftxui::Event::F5 && !generating) {
            // Webszerver leallitasa ha fut
            if (web_server) { web_server->stop(); web_server.reset(); }
            web_server_running = false;
            status_info->web_server_active = false;

            // Pipeline reset
            loading_done = false;
            loading_error = false;
            loading_error_message.clear();
            loading_state->stage_1 = "Varakozas...";
            loading_state->stage_2 = "Varakozas...";
            loading_state->embedding = "Varakozas...";
            status_info->state = "Betoltes...";
            status_info->tokens_per_second = 0.0;

            // Ujratoltés kulon szalon
            std::thread([&]() {
                try {
                    loading_state->stage_1 = "Betoltes...";
                    screen.Post(ftxui::Event::Custom);

                    pipeline = std::make_shared<masodik::genai::LlmPipelineService>(service_config);
                    pipeline->initialize();

                    if (pipeline->ready()) {
                        loading_state->stage_1 = "Kesz";
                        loading_state->stage_2 = "Kesz";
                    } else {
                        loading_state->stage_1 = "Hiba";
                        loading_error = true;
                        loading_error_message = pipeline->status_message();
                    }
                    screen.Post(ftxui::Event::Custom);

                    if (service_config.serving_profile.embedding_enabled) {
                        loading_state->embedding = "Betoltes...";
                        screen.Post(ftxui::Event::Custom);
                        embedding = std::make_shared<masodik::genai::EmbeddingPipelineService>();
                        embedding->initialize(
                            service_config.serving_profile.embedding_model_path,
                            service_config.serving_profile.embedding_device,
                            service_config.serving_profile.embedding_model_name,
                            service_config.serving_profile.embedding_reject_cpu_fallback);
                        loading_state->embedding = embedding->ready() ? "Kesz" : "Hiba";
                        screen.Post(ftxui::Event::Custom);
                    }

                    loading_done = true;
                    status_info->state = "Kesz";
                    screen.Post(ftxui::Event::Custom);
                } catch (const std::exception& error) {
                    loading_error = true;
                    loading_error_message = error.what();
                    loading_done = true;
                    screen.Post(ftxui::Event::Custom);
                }
            }).detach();
            return true;
        }

        // F12 → diagnosztika
        if (event == ftxui::Event::F12) {
            modal_mode = ModalMode::diagnostics;
            return true;
        }

        // Enter → uzenet kuldese (ha nem ures es nem general)
        if (event == ftxui::Event::Return && loading_done && !generating) {
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
    std::thread monitor_thread([&]() {
        while (monitor_running) {
            status_info->resources = resource_monitor->snapshot();
            screen.Post(ftxui::Event::Custom);
            for (int i = 0; i < 10 && monitor_running; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
            }
        }
    });

    screen.Loop(event_handler);

    monitor_running = false;
    generating = false;
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    if (web_server) {
        web_server->stop();
        web_server.reset();
    }
    if (loading_thread.joinable()) {
        loading_thread.join();
    }

    return 0;
}
