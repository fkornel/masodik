#include "masodik/tui/markdown_render.hpp"

#include <string>
#include <string_view>
#include <vector>

#include "ftxui/dom/elements.hpp"

namespace masodik::tui {

namespace {

// Egy sor inline elemeinek feldolgozasa: **bold** es `inline kod`.
// FONTOS: text() hasznalata paragraph() helyett az inline szegmensekhez,
// mert paragraph() hbox-ban onallo flex teret foglal es extra terkozokat okoz.
ftxui::Element render_inline(const std::string& line) {
    using namespace ftxui;
    Elements parts;
    std::size_t pos = 0;
    bool has_markup = false;

    while (pos < line.size()) {
        auto bold_start = line.find("**", pos);
        auto code_start = line.find('`', pos);

        if (bold_start != std::string::npos &&
            (code_start == std::string::npos || bold_start < code_start)) {
            has_markup = true;
            if (bold_start > pos) {
                parts.push_back(text(line.substr(pos, bold_start - pos)));
            }
            auto bold_end = line.find("**", bold_start + 2);
            if (bold_end != std::string::npos) {
                auto bold_text = line.substr(bold_start + 2, bold_end - bold_start - 2);
                parts.push_back(text(bold_text) | bold);
                pos = bold_end + 2;
            } else {
                parts.push_back(text(line.substr(pos)));
                pos = line.size();
            }
        } else if (code_start != std::string::npos) {
            has_markup = true;
            if (code_start > pos) {
                parts.push_back(text(line.substr(pos, code_start - pos)));
            }
            auto code_end = line.find('`', code_start + 1);
            if (code_end != std::string::npos) {
                auto code_text = line.substr(code_start + 1, code_end - code_start - 1);
                parts.push_back(text(code_text) | color(Color::Yellow));
                pos = code_end + 1;
            } else {
                parts.push_back(text(line.substr(pos)));
                pos = line.size();
            }
        } else {
            parts.push_back(text(line.substr(pos)));
            pos = line.size();
        }
    }

    if (parts.empty()) return text("");
    // Ha nincs markup, paragraph()-ot hasznalunk a szo-tordeleshez
    if (!has_markup) return paragraph(line);
    if (parts.size() == 1) return parts[0];
    return hbox(std::move(parts));
}

}  // namespace

ftxui::Element render_markdown(const std::string& md_text) {
    using namespace ftxui;
    Elements rows;

    std::vector<std::string> lines;
    {
        std::size_t start = 0;
        while (start < md_text.size()) {
            auto nl = md_text.find('\n', start);
            if (nl == std::string::npos) {
                lines.push_back(md_text.substr(start));
                break;
            }
            lines.push_back(md_text.substr(start, nl - start));
            start = nl + 1;
        }
    }

    bool in_code_block = false;
    Elements code_block_lines;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        if (line.starts_with("```")) {
            if (!in_code_block) {
                in_code_block = true;
                code_block_lines.clear();
            } else {
                auto code_content = vbox(std::move(code_block_lines));
                rows.push_back(code_content | dim | border);
                code_block_lines.clear();
                in_code_block = false;
            }
            continue;
        }

        if (in_code_block) {
            code_block_lines.push_back(text(line));
            continue;
        }

        if (line.empty()) {
            rows.push_back(text(""));
        } else {
            rows.push_back(render_inline(line));
        }
    }

    if (in_code_block && !code_block_lines.empty()) {
        auto code_content = vbox(std::move(code_block_lines));
        rows.push_back(code_content | dim | border);
    }

    if (rows.empty()) return text("");
    return vbox(std::move(rows));
}

}  // namespace masodik::tui
