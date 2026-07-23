#include "panels.h"
#include "theme.h"

static const char* BG_MODE_NAMES[2] = { "Theme background: False", "Theme background: True" };

void panel_settings(ncplane* n, int rows, int cols) {
    const auto& themes = all_themes();

    int w = std::min(68, std::max(30, cols - 4));
    int content_h = static_cast<int>(themes.size()) + 6;
    int h = std::min(content_h, rows - 4);
    if (w < 24 || h < 6) return;

    int x = (cols - w) / 2;
    int y = (rows - h) / 2;

    auto [iy, ix, ih, iw] =
        draw_box(n, y, x, h, w, "Settings", "Esc:close  Tab:switch  \xe2\x86\x91\xe2\x86\x93:select  Enter:save");
    if (ih <= 0 || iw <= 0) return;

    int left_w  = iw * 3 / 5;
    int right_x = ix + left_w + 1;

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

    if (right_x < ix + iw - 3) {
        nc_set(n, theme().BLUE, NCSTYLE_BOLD);
        ncplane_putstr_yx(n, iy, right_x, " Background");

        for (int i = 0; i < 2; ++i) {
            int r = iy + 2 + i;
            if (r >= iy + ih) break;

            bool selected = (i == G.bg_idx);
            bool focused  = (G.settings_focus == 1);

            uint32_t col = selected ? theme().GREEN : theme().SUBTEXT0;
            nc_set(n, col, selected && focused ? NCSTYLE_BOLD : NCSTYLE_NONE);

            std::string label = std::string(selected ? "> " : "  ") + BG_MODE_NAMES[i];
            ncplane_putstr_yx(n, r, right_x, label.c_str());
        }
    }

    int frow = iy + ih - 1;
    if (frow >= iy) {
        nc_set(n, theme().OVERLAY0);
        ncplane_putstr_yx(n, frow, ix,
            str_trunc(" Preview applies live \xe2\x80\x94 Enter saves, Esc reverts", iw).c_str());
    }
}
