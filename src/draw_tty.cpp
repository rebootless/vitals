#include "draw_tty.h"

#include <algorithm>
#include <cstring>

void draw_bar_tty(ncplane* n, int y, int x, int w, double fill, GradType gt) {
    if (w <= 0) return;
    fill = std::clamp(fill, 0.0, 1.0);

    int full = static_cast<int>(fill * w + 0.5);
    uint32_t col = grad_color(gt, fill);

    if (full > 0) {
        nc_set(n, col, NCSTYLE_BOLD);
        std::string filled(static_cast<size_t>(full), '#');
        ncplane_putstr_yx(n, y, x, filled.c_str());
    }
    if (full < w) {
        nc_set(n, theme().BAR_EMPTY);
        std::string empty(static_cast<size_t>(w - full), '.');
        ncplane_putstr_yx(n, y, x + full, empty.c_str());
    }
}

const char* glyph_down() { return G.tty_active ? "v" : "\xe2\x96\xbc"; }
const char* glyph_up()   { return G.tty_active ? "^" : "\xe2\x96\xb2"; }
const char* deg_suffix() { return G.tty_active ? "C" : "\xc2\xb0" "C"; }
