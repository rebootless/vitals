#include "draw_tty.h"
#include "draw.h" // str_trunc

#include <algorithm>
#include <cstring>

namespace {

void hline_tty(ncplane* n, int y, int x, int w, char glyph, uint32_t color) {
    nc_set(n, color);
    std::string s(static_cast<size_t>(std::max(0, w)), glyph);
    ncplane_putstr_yx(n, y, x, s.c_str());
}

} // namespace

// Box drawing — plain "+", "-", "|" instead of ┌─┐│└┘.
std::tuple<int,int,int,int>
draw_box_tty(ncplane* n, int y, int x, int h, int w,
            const std::string& title, const std::string& bl_label) {
    if (h < 2 || w < 2) return {y, x, 0, 0};

    nc_set(n, theme().SURFACE2);
    ncplane_putstr_yx(n, y, x, "+");
    hline_tty(n, y, x + 1, w - 2, '-', theme().SURFACE2);
    ncplane_putstr_yx(n, y, x + w - 1, "+");

    for (int i = 1; i < h - 1; ++i) {
        nc_set(n, theme().SURFACE2);
        ncplane_putstr_yx(n, y + i, x, "|");
        ncplane_putstr_yx(n, y + i, x + w - 1, "|");

        nc_set(n, theme().BASE);
        for (int j = x + 1; j < x + w - 1; ++j)
            ncplane_putstr_yx(n, y + i, j, " ");
    }

    nc_set(n, theme().SURFACE2);
    ncplane_putstr_yx(n, y + h - 1, x, "+");
    hline_tty(n, y + h - 1, x + 1, w - 2, '-', theme().SURFACE2);
    ncplane_putstr_yx(n, y + h - 1, x + w - 1, "+");

    if (!title.empty() && w > 4) {
        std::string t = " " + str_trunc(title, w - 4) + " ";
        nc_set(n, theme().MAUVE, NCSTYLE_BOLD);
        ncplane_putstr_yx(n, y, x + 2, t.c_str());
    }

    if (!bl_label.empty() && w > 4) {
        std::string t = " " + str_trunc(bl_label, w - 4) + " ";
        hline_tty(n, y + h - 1, x + 1, static_cast<int>(t.size()) + 1, '-', theme().SURFACE2);
        nc_set(n, theme().BLUE);
        ncplane_putstr_yx(n, y + h - 1, x + 2, t.c_str());
    }

    return {y + 1, x + 1, h - 2, w - 2};
}

// Bar — "#" filled / "." empty, single representative color rather than a
// per-cell gradient (16-color terminals make a smooth gradient pointless).
void draw_bar_tty(ncplane* n, int y, int x, int w, double fill, GradType gt) {
    if (w <= 0) return;
    fill = std::max(0.0, std::min(1.0, fill));

    int full = static_cast<int>(fill * w + 0.5);
    uint32_t col = grad_color(gt, fill);

    if (full > 0) {
        nc_set(n, col, NCSTYLE_BOLD);
        std::string filled(static_cast<size_t>(full), '#');
        ncplane_putstr_yx(n, y, x, filled.c_str());
    }
    if (full < w) {
        nc_set(n, theme().SURFACE1);
        std::string empty(static_cast<size_t>(w - full), '.');
        ncplane_putstr_yx(n, y, x + full, empty.c_str());
    }
}

// Sparkline — classic ASCII shading ramp instead of Unicode ▁▂▃▄▅▆▇█.
void draw_spark_tty(ncplane* n, int y, int x, int w, const std::deque<double>& hist) {
    if (w <= 0 || hist.empty()) return;

    static const char RAMP[] = " .,:;=+*#@";
    constexpr int RAMP_N = static_cast<int>(sizeof(RAMP) / sizeof(RAMP[0])) - 2; // usable index range

    int start = (static_cast<int>(hist.size()) > w)
                ? static_cast<int>(hist.size()) - w : 0;

    for (int i = start, col = x; i < static_cast<int>(hist.size()); ++i, ++col) {
        double v   = std::max(0.0, std::min(100.0, hist[i]));
        int    idx = static_cast<int>(v / 100.0 * RAMP_N + 0.5);
        idx = std::max(0, std::min(RAMP_N, idx));

        uint32_t color = grad_color(GRAD_HIST, v / 100.0);
        nc_set(n, color);
        char glyph[2] = { RAMP[idx], '\0' };
        ncplane_putstr_yx(n, y, col, glyph);
    }
}

const char* glyph_down() { return G.tty_active ? "v" : "\xe2\x96\xbc"; }
const char* glyph_up()   { return G.tty_active ? "^" : "\xe2\x96\xb2"; }
const char* glyph_dash() { return G.tty_active ? "-" : "\xe2\x80\x94"; }
const char* deg_suffix() { return G.tty_active ? "C" : "\xc2\xb0" "C"; }
