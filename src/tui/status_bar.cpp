#include "masodik/tui/status_bar.hpp"

#include <iomanip>
#include <sstream>

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

namespace {

std::string pct_str(double percent) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(0) << std::max(0.0, std::min(100.0, percent)) << "%";
    return ss.str();
}

std::string gb_str(std::uint64_t bytes) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    return ss.str();
}

ftxui::Element mem_bar(double percent, int width = 6, ftxui::Color base_color = ftxui::Color::Green) {
    using namespace ftxui;
    const auto clamped = std::max(0.0, std::min(100.0, percent));
    const int filled = static_cast<int>(clamped / 100.0 * width);
    std::string bar;
    for (int i = 0; i < width; ++i) {
        bar += (i < filled) ? "\xe2\x96\x88" : "\xe2\x96\x91";
    }
    auto bar_color = base_color;
    if (clamped > 80.0) bar_color = Color::Red;
    else if (clamped > 60.0) bar_color = Color::Yellow;
    return text(bar) | color(bar_color);
}

ftxui::Element core_block(double percent, bool is_p_core) {
    using namespace ftxui;
    const auto clamped = std::max(0.0, std::min(100.0, percent));
    Color block_color;
    if (clamped < 5.0) {
        block_color = Color::GrayDark;
    } else if (clamped < 25.0) {
        block_color = is_p_core ? Color::Cyan : Color::Blue;
    } else if (clamped < 50.0) {
        block_color = is_p_core ? Color::CyanLight : Color::BlueLight;
    } else if (clamped < 75.0) {
        block_color = is_p_core ? Color::Yellow : Color::YellowLight;
    } else {
        block_color = is_p_core ? Color::Red : Color::RedLight;
    }
    return text("\xe2\x96\x88") | color(block_color);
}

}  // namespace

ftxui::Element render_status_bar(const StatusBarInfo& info) {
    using namespace ftxui;

    const auto& r = info.resources;

    // Allapot LED szin mapping:
    // ● Zold = modell betoltve, kiszolgalasra kesz
    // ● Cyan = valasz generalas alatt
    // ● Sarga = modell betoltes folyamatban
    // ● Piros = hiba
    // ● Sotet = modell nincs betoltve (offload / inditaskor)
    auto state_color = Color::GrayDark;  // Alapertelmezett: nincs betoltve
    if (info.state == "Kesz") state_color = Color::Green;
    else if (info.state == "Betoltes...") state_color = Color::Yellow;
    else if (info.state == "Hiba") state_color = Color::Red;
    else if (info.state == "General...") state_color = Color::Cyan;

    // --- 1. sor: Hardver metrikak (CPU + RAM + dGPU + iGPU) ---
    Elements hw_row;
    // CPU — ⚡ villam
    hw_row.push_back(text(" \xe2\x9a\xa1") | color(Color::Cyan));
    hw_row.push_back(text("["));
    if (!r.cores.empty()) {
        for (const auto& core : r.cores) {
            hw_row.push_back(core_block(core.percent, core.is_p_core));
        }
    } else {
        hw_row.push_back(text("....................") | dim);
    }
    hw_row.push_back(text("]"));
    hw_row.push_back(text(pct_str(r.cpu_percent) + " ") | dim);
    // RAM — 💾 floppy
    hw_row.push_back(text("\xf0\x9f\x92\xbe") | color(Color::Magenta));
    hw_row.push_back(mem_bar(r.ram_percent, 6, Color::Magenta));
    hw_row.push_back(text(gb_str(r.ram_used_bytes) + "/" + gb_str(r.ram_total_bytes) + "G ") | dim);
    // dGPU — 🎮 gamepad
    hw_row.push_back(text("\xf0\x9f\x8e\xae") | color(Color::Green));
    hw_row.push_back(mem_bar(r.dgpu_vram_percent, 6, Color::Green));
    hw_row.push_back(text(gb_str(r.dgpu_vram_used_bytes) + "/" + gb_str(r.dgpu_vram_total_bytes) + "G ") | dim);
    // iGPU — 🎮 gamepad (ugyanaz, de kek szinnel megkulonboztetve)
    hw_row.push_back(text("\xf0\x9f\x8e\xae") | color(Color::Blue));
    hw_row.push_back(mem_bar(r.igpu_vram_percent, 4, Color::Blue));
    hw_row.push_back(text(gb_str(r.igpu_vram_used_bytes) + "/" + gb_str(r.igpu_vram_total_bytes) + "G") | dim);

    // --- 2. sor: Context + tok/s + uptime + web + allapot ---
    Elements app_row;
    // Context
    const double ctx_percent = info.context_total > 0
        ? static_cast<double>(info.context_used) * 100.0 / static_cast<double>(info.context_total)
        : 0.0;
    auto ctx_short = [](int tokens) -> std::string {
        if (tokens >= 1000000) return std::to_string(tokens / 1000000) + "M";
        if (tokens >= 1000) return std::to_string(tokens / 1000) + "k";
        return std::to_string(tokens);
    };
    app_row.push_back(text(" \xe2\x89\xa1") | color(Color::Yellow));  // ≡ context
    app_row.push_back(mem_bar(ctx_percent, 6, Color::Yellow));
    app_row.push_back(text(ctx_short(info.context_used) + "/" + ctx_short(info.context_total) + " ") | dim);

    // Tok/s — ⏱ stopperora
    {
        std::ostringstream tps;
        tps << std::fixed << std::setprecision(1) << info.tokens_per_second << "t/s";
        app_row.push_back(text("\xe2\x8f\xb1") | color(Color::Cyan));  // ⏱
        app_row.push_back(text(tps.str() + " ") | (info.tokens_per_second > 0.1 ? (bold | color(Color::Cyan)) : dim));
    }

    // Uptime
    {
        const int h = info.uptime_seconds / 3600;
        const int m = (info.uptime_seconds % 3600) / 60;
        const int s = info.uptime_seconds % 60;
        std::ostringstream ut;
        if (h > 0) ut << h << "h";
        ut << m << "m" << s << "s";
        app_row.push_back(text("\xe2\x86\x91") | color(Color::White));  // ↑ uptime
        app_row.push_back(text(ut.str() + " ") | dim);
    }

    app_row.push_back(filler());

    // Web szerver
    if (info.web_server_active) {
        app_row.push_back(text("Web:" + std::to_string(info.web_server_port) + " ") | color(Color::Green));
    }

    // Allapot LED — szines kitoltott kor (●) mint diagnosztikai LED
    // Zold: modell betoltve, kiszolgalasra kesz
    // Cyan: valasz generalas alatt
    // Sarga: modell betoltes folyamatban
    // Piros: hiba
    // Sotet: modell nincs betoltve (offload-olva vagy meg nem toltodott)
    app_row.push_back(text(" \xe2\x97\x8f ") | color(state_color));  // ●

    // --- Osszeallitas ---
    Elements rows;
    rows.push_back(hbox(std::move(hw_row)));
    rows.push_back(hbox(std::move(app_row)));

    // Hint sor (opcionalis 3. sor)
    if (!info.hint_message.empty()) {
        rows.push_back(text(" " + info.hint_message) | dim | color(Color::YellowLight));
    }

    return vbox(std::move(rows));
}

}  // namespace masodik::tui
