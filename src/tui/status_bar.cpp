#include "masodik/tui/status_bar.hpp"

#include <iomanip>
#include <sstream>

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

namespace {

// ASCII progress bar rendereles: [████░░░░] formatumban.
// A szelesseg a bar belso karaktereinek szama (alapertelmezett 8).
ftxui::Element ascii_bar(double percent, int width = 8, ftxui::Color fill_color = ftxui::Color::Green) {
    using namespace ftxui;
    const auto clamped = std::max(0.0, std::min(100.0, percent));
    const int filled = static_cast<int>(clamped / 100.0 * width);
    std::string bar;
    for (int i = 0; i < width; ++i) {
        bar += (i < filled) ? "\xe2\x96\x88" : "\xe2\x96\x91";  // █ es ░ UTF-8
    }
    // Szin valasztas a terheles alapjan
    auto bar_color = fill_color;
    if (clamped > 80.0) bar_color = Color::Red;
    else if (clamped > 60.0) bar_color = Color::Yellow;

    return text(bar) | color(bar_color);
}

// Szazalekos erteket 3 karakter szelessegure formaz
std::string pct_str(double percent) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(0) << std::max(0.0, std::min(100.0, percent)) << "%";
    return ss.str();
}

// Byte-ot GB-re konvertal (1 tizedes)
std::string gb_str(std::uint64_t bytes) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    return ss.str();
}

}  // namespace

ftxui::Element render_status_bar(const StatusBarInfo& info) {
    using namespace ftxui;

    const auto& r = info.resources;

    // Allapot szin
    auto state_color = Color::Green;
    if (info.state == "Betoltes...") state_color = Color::Yellow;
    else if (info.state == "Hiba") state_color = Color::Red;
    else if (info.state == "General...") state_color = Color::Cyan;

    // Elso sor: CPU + RAM
    auto row1 = hbox({
        text(" CPU") | bold | dim,
        ascii_bar(r.cpu_percent, 8, Color::Cyan),
        text(pct_str(r.cpu_percent) + " ") | dim,
        text("RAM") | bold | dim,
        ascii_bar(r.ram_percent, 8, Color::Blue),
        text(gb_str(r.ram_used_bytes) + "/" + gb_str(r.ram_total_bytes) + "G ") | dim,
        filler(),
        // Webszerver allapot (csak ha aktiv)
        info.web_server_active
            ? (text("Web:" + std::to_string(info.web_server_port) + " ") | color(Color::Green))
            : text(""),
        text(info.state + " ") | color(state_color),
    });

    // Masodik sor: dGPU + iGPU + Context + tok/s
    // Context szazalek es bar
    const double ctx_percent = info.context_total > 0
        ? static_cast<double>(info.context_used) * 100.0 / static_cast<double>(info.context_total)
        : 0.0;
    // Context rovid kijelzes (k vagy M formatum)
    auto ctx_short = [](int tokens) -> std::string {
        if (tokens >= 1000000) return std::to_string(tokens / 1000000) + "M";
        if (tokens >= 1000) return std::to_string(tokens / 1000) + "k";
        return std::to_string(tokens);
    };

    // Tok/s
    std::string tps_str;
    if (info.tokens_per_second > 0.1) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << info.tokens_per_second << "t/s";
        tps_str = ss.str();
    }

    auto row2 = hbox({
        text(" dGPU") | bold | dim,
        ascii_bar(r.dgpu_vram_percent, 6, Color::Magenta),
        text(gb_str(r.dgpu_vram_used_bytes) + "/" + gb_str(r.dgpu_vram_total_bytes) + "G ") | dim,
        text("iGPU") | bold | dim,
        ascii_bar(r.igpu_vram_percent, 6, Color::Blue),
        text(gb_str(r.igpu_vram_used_bytes) + "/" + gb_str(r.igpu_vram_total_bytes) + "G ") | dim,
        text("Ctx") | bold | dim,
        ascii_bar(ctx_percent, 6, Color::Yellow),
        text(ctx_short(info.context_used) + "/" + ctx_short(info.context_total) + " ") | dim,
        filler(),
        tps_str.empty() ? text("") : (text(tps_str + " ") | color(Color::Cyan) | bold),
    });

    return vbox({ row1, row2 });
}

}  // namespace masodik::tui
