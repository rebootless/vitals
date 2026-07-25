#include "panels.h"
#include "theme.h"
#include "draw_tty.h"

// Universal (not per-theme) option lists — names live here rather than in
// theme.h/theme.cpp or tty.h/tty.cpp.
static const char* BG_MODE_NAMES[2] = { "Theme background: False", "Theme background: True" };
static const char* TTY_MODE_NAMES[3] = { "Auto-detect", "Force TTY mode", "Force graphics mode" };

void panel_settings(ncplane* n, int rows, int cols) {
    const auto& themes = all_themes();

    int w = std::min(68, std::max(30, cols - 4));
    // Right column now stacks Background (2) + Terminal (3) groups, each
    // with its own header — that's the tallest column, so it drives h.
    int right_rows = 2 + 2 + 3 + 1; // "Background" hdr+2 + "Terminal" hdr+3 + gap
    int content_h  = std::max(static_cast<int>(themes.size()), right_rows) + 4;
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

    // Left column: themes
    nc_set(n, theme().BLUE, NCSTYLE_BOLD);
    ncplane_putstr_yx(n, iy, ix, " Theme");

    for (size_t i = 0; i < themes.size(); ++i) {
        int r = iy + 2 + static_cast<int>(i);
        if (r >= iy + ih) break;

        bool selected = (static_cast<int>(i) == G.theme_idx);
        bool focused  = (G.settings_focus == 0);

        uint32_t col = selected ? theme().GREEN : theme().SUBTEXT0;
        nc_set(n, col, selected && focused ? NCSTYLE_BOLD : NCSTYLE_NONE);

        std::string label = (selected ? "> " : "  ") +
                            str_trunc(themes[i].name, left_w - 3);
        ncplane_putstr_yx(n, r, ix, label.c_str());
    }

    // Right column: two stacked groups — Background, then Terminal mode.
    if (right_x < ix + iw - 3) {
        int r = iy;

        nc_set(n, theme().BLUE, NCSTYLE_BOLD);
        ncplane_putstr_yx(n, r, right_x, " Background");
        r += 2;

        for (int i = 0; i < 2 && r < iy + ih; ++i, ++r) {
            bool selected = (i == G.bg_idx);
            bool focused  = (G.settings_focus == 1);

            uint32_t col = selected ? theme().GREEN : theme().SUBTEXT0;
            nc_set(n, col, selected && focused ? NCSTYLE_BOLD : NCSTYLE_NONE);

            std::string label = std::string(selected ? "> " : "  ") + BG_MODE_NAMES[i];
            ncplane_putstr_yx(n, r, right_x, label.c_str());
        }

        r++; // gap between groups

        if (r < iy + ih) {
            nc_set(n, theme().BLUE, NCSTYLE_BOLD);
            ncplane_putstr_yx(n, r, right_x, " Terminal");
            r += 2;

            for (int i = 0; i < 3 && r < iy + ih; ++i, ++r) {
                bool selected = (static_cast<int>(G.tty_force) == i);
                bool focused  = (G.settings_focus == 2);

                uint32_t col = selected ? theme().GREEN : theme().SUBTEXT0;
                nc_set(n, col, selected && focused ? NCSTYLE_BOLD : NCSTYLE_NONE);

                std::string label = std::string(selected ? "> " : "  ") + TTY_MODE_NAMES[i];
                ncplane_putstr_yx(n, r, right_x, label.c_str());
            }
        }
    }

    // Footer hint
    int frow = iy + ih - 1;
    if (frow >= iy) {
        nc_set(n, theme().OVERLAY0);
        std::string note = G.tty_active
            ? " Preview applies live - Enter saves, Esc reverts"
            : " Preview applies live \xe2\x80\x94 Enter saves, Esc reverts";
        ncplane_putstr_yx(n, frow, ix, str_trunc(note, iw).c_str());
    }
}
