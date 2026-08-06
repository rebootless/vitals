#include "panels.h"
#include "theme.h"
#include "draw_tty.h"

// Universal (not per-theme) option lists — names live here rather than in
// theme.h/theme.cpp or tty.h/tty.cpp.
static const char* BG_MODE_NAMES[2] = { "Theme background: False", "Theme background: True" };
static const char* TTY_MODE_NAMES[3] = { "Auto-detect", "Force TTY mode", "Force graphics mode" };
static const char* CORNER_MODE_NAMES[2] = { "Square", "Rounded" };

void panel_settings(ncplane* n, int rows, int cols) {
    const auto& themes = all_themes();

    int w = std::min(68, std::max(30, cols - 4));
    // Right column stacks Background (2) + Terminal (3) + Corners (2)
    // groups, each with its own header — that's the tallest column, so
    // it drives h. (In practice the theme list on the left is taller
    // still, so this rarely ends up being the binding constraint.)
    int right_rows = 2 + 2 + 3 + 2 + 2; // hdr+2 + hdr+3 + hdr+2, +1 gap each between groups
    int content_h  = std::max(static_cast<int>(themes.size()), right_rows) + 6;
    int h = std::min(content_h, rows - 4);
    if (w < 24 || h < 6) return; // terminal too small to show the overlay

    int x = (cols - w) / 2;
    int y = (rows - h) / 2;

    std::string hint = G.tty_active
        ? "Esc:close  Tab:switch  Up/Down:select  Enter:save"
        : "Esc:close  Tab:switch  \xe2\x86\x91\xe2\x86\x93:select  Enter:save";

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Settings", hint);
    if (ih <= 0 || iw <= 0) return;

    int left_w  = iw * 3 / 5;
    int right_x = ix + left_w + 1;

    // Which group currently has keyboard focus is now shown with a ">"
    // arrow on that group's own header line (moves there on Tab) instead
    // of bolding the focused item — bold and the "selected value" ">"
    // marker looked too similar at a glance. Item rows below a header
    // keep their plain "> "/"  " selection marker, just never bold.
    auto draw_group_header = [&](int r, int group_idx, const char* title) {
        bool focused = (G.settings_focus == group_idx);
        nc_set(n, theme().BLUE, NCSTYLE_BOLD);
        std::string hdr = std::string(focused ? "> " : "  ") + title;
        ncplane_putstr_yx(n, r, right_x, hdr.c_str());
    };

    // Left column: themes
    {
        bool focused = (G.settings_focus == 0);
        nc_set(n, theme().BLUE, NCSTYLE_BOLD);
        std::string hdr = std::string(focused ? "> " : "  ") + "Theme";
        ncplane_putstr_yx(n, iy, ix, hdr.c_str());
    }

    for (size_t i = 0; i < themes.size(); ++i) {
        int r = iy + 2 + static_cast<int>(i);
        if (r >= iy + ih) break;

        bool selected = (static_cast<int>(i) == G.theme_idx);

        uint32_t col = selected ? theme().GREEN : theme().SUBTEXT0;
        nc_set(n, col, NCSTYLE_NONE);

        std::string label = (selected ? "> " : "  ") +
                            str_trunc(themes[i].name, left_w - 3);
        ncplane_putstr_yx(n, r, ix, label.c_str());
    }

    // Right column: three stacked groups — Background, Terminal, Corners.
    if (right_x < ix + iw - 3) {
        int r = iy;

        draw_group_header(r, 1, "Background");
        r += 2;

        for (int i = 0; i < 2 && r < iy + ih; ++i, ++r) {
            bool selected = (i == G.bg_idx);

            uint32_t col = selected ? theme().GREEN : theme().SUBTEXT0;
            nc_set(n, col, NCSTYLE_NONE);

            std::string label = std::string(selected ? "> " : "  ") + BG_MODE_NAMES[i];
            ncplane_putstr_yx(n, r, right_x, label.c_str());
        }

        r++; // gap between groups

        if (r < iy + ih) {
            draw_group_header(r, 2, "Terminal");
            r += 2;

            for (int i = 0; i < 3 && r < iy + ih; ++i, ++r) {
                bool selected = (static_cast<int>(G.tty_force) == i);

                uint32_t col = selected ? theme().GREEN : theme().SUBTEXT0;
                nc_set(n, col, NCSTYLE_NONE);

                std::string label = std::string(selected ? "> " : "  ") + TTY_MODE_NAMES[i];
                ncplane_putstr_yx(n, r, right_x, label.c_str());
            }
        }

        r++; // gap between groups

        if (r < iy + ih) {
            draw_group_header(r, 3, "Corners");
            r += 2;

            for (int i = 0; i < 2 && r < iy + ih; ++i, ++r) {
                bool selected = (i == G.corners_idx);

                uint32_t col = selected ? theme().GREEN : theme().SUBTEXT0;
                nc_set(n, col, NCSTYLE_NONE);

                std::string label = std::string(selected ? "> " : "  ") + CORNER_MODE_NAMES[i];
                ncplane_putstr_yx(n, r, right_x, label.c_str());
            }
        }
    }

    // Footer row: Refresh rate control (focus group 4, Up/Down to adjust).
    // Reuses the existing last row rather than adding a new row group, so
    // the panel's w/h stay exactly as sized.
    int frow = iy + ih - 1;
    if (frow >= iy) {
        bool focused = (G.settings_focus == 4);

        nc_set(n, theme().BLUE);
        std::string lead = std::string(focused ? "> " : "  ") + "Refresh: ";
        ncplane_putstr_yx(n, frow, ix, lead.c_str());

        nc_set(n, focused ? theme().GREEN : theme().TEXT, NCSTYLE_NONE);
        std::string val = std::to_string(G.refresh_ms) + "ms";
        ncplane_putstr_yx(n, frow, ix + static_cast<int>(lead.size()), val.c_str());
    }
}
