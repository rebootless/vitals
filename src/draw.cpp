#include "draw.h"
#include "draw_tty.h"
#include "sysinfo.h"

#include <cstring>

// Text utilities
std::string str_trunc(const std::string& s, int maxcols) {
    if (maxcols <= 0) return "";
    int cols = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int bytes = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
        if (cols + 1 > maxcols) break;
        cols++;
        i += static_cast<size_t>(bytes);
    }
    return s.substr(0, i);
}

// Internal: horizontal line of one UTF-8 glyph
static void hline(ncplane* n, int y, int x, int w, const char* glyph,
                  uint32_t color, unsigned style = NCSTYLE_NONE) {
    nc_set(n, color, style);
    for (int i = 0; i < w; ++i)
        ncplane_putstr_yx(n, y, x + i, glyph);
}

// Box drawing
std::tuple<int,int,int,int>
draw_box(ncplane* n, int y, int x, int h, int w,
         const std::string& title, const std::string& bl_label) {
    if (G.tty_active) return draw_box_tty(n, y, x, h, w, title, bl_label);
    if (h < 2 || w < 2) return {y, x, 0, 0};

    nc_set(n, theme().SURFACE2);

    // Top border: ┌─────┐
    ncplane_putstr_yx(n, y, x, "┌");
    hline(n, y, x + 1, w - 2, "─", theme().SURFACE2);
    ncplane_putstr_yx(n, y, x + w - 1, "┐");

    // Side borders + background fill
    for (int i = 1; i < h - 1; ++i) {
        nc_set(n, theme().SURFACE2);
        ncplane_putstr_yx(n, y + i, x, "│");
        ncplane_putstr_yx(n, y + i, x + w - 1, "│");

        // Fill interior
        nc_set(n, theme().BASE);
        for (int j = x + 1; j < x + w - 1; ++j)
            ncplane_putstr_yx(n, y + i, j, " ");
    }

    // Bottom border: └─────┘
    nc_set(n, theme().SURFACE2);
    ncplane_putstr_yx(n, y + h - 1, x, "└");
    hline(n, y + h - 1, x + 1, w - 2, "─", theme().SURFACE2);
    ncplane_putstr_yx(n, y + h - 1, x + w - 1, "┘");

    // Title embedded in top border: ─ Title ─
    if (!title.empty() && w > 4) {
        std::string t = " " + str_trunc(title, w - 4) + " ";
        nc_set(n, theme().MAUVE, NCSTYLE_BOLD);
        ncplane_putstr_yx(n, y, x + 2, t.c_str());
    }

    // Bottom-left label: └─ q:Quit ─...
    if (!bl_label.empty() && w > 4) {
        std::string t = " " + str_trunc(bl_label, w - 4) + " ";
        // Overdraw with a border-colored hline first so the area is clean
        hline(n, y + h - 1, x + 1, static_cast<int>(t.size()) + 1, "─", theme().SURFACE2);
        nc_set(n, theme().BLUE);
        ncplane_putstr_yx(n, y + h - 1, x + 2, t.c_str());
    }

    return {y + 1, x + 1, h - 2, w - 2};
}

// Separator
void draw_sep(ncplane* n, int y, int x, int w) {
    hline(n, y, x, w, "-", theme().OVERLAY0);
}

// Brackets
void lbr(ncplane* n, int y, int x) {
    nc_set(n, theme().OVERLAY0);
    ncplane_putstr_yx(n, y, x, "[");
}

void rbr(ncplane* n, int y, int x) {
    nc_set(n, theme().OVERLAY0);
    ncplane_putstr_yx(n, y, x, "]");
}

// Gradient bar
void draw_bar_grad(ncplane* n, int y, int x, int w, double fill, GradType gt) {
    if (G.tty_active) { draw_bar_tty(n, y, x, w, fill, gt); return; }
    if (w <= 0) return;
    fill = std::max(0.0, std::min(1.0, fill));

    double cells = fill * w * 8.0;
    int full  = static_cast<int>(cells / 8.0);
    int frac  = static_cast<int>(cells) % 8;

    for (int i = 0; i < w; ++i) {
        // Position within the bar [0,1] — drives the gradient color.
        double pos = (w > 1) ? static_cast<double>(i) / (w - 1) : 0.0;

        if (i < full) {
            uint32_t col = grad_color(gt, pos);
            nc_set(n, col, NCSTYLE_BOLD);
            ncplane_putstr_yx(n, y, x + i, BLOCK[8]);
        } else if (i == full && frac > 0) {
            uint32_t col = grad_color(gt, pos);
            nc_set(n, col, NCSTYLE_BOLD);
            ncplane_putstr_yx(n, y, x + i, BLOCK[frac]);
        } else {
            nc_set(n, theme().SURFACE1);
            ncplane_putstr_yx(n, y, x + i, BAR_BG);
        }
    }
}

// Sparkline
void draw_spark(ncplane* n, int y, int x, int w, const std::deque<double>& hist) {
    if (G.tty_active) { draw_spark_tty(n, y, x, w, hist); return; }
    if (w <= 0 || hist.empty()) return;
    int start = (static_cast<int>(hist.size()) > w)
                ? static_cast<int>(hist.size()) - w : 0;

    for (int i = start, col = x; i < static_cast<int>(hist.size()); ++i, ++col) {
        double v   = std::max(0.0, std::min(100.0, hist[i]));
        int    idx = static_cast<int>(v / 100.0 * 7.0 + 0.5);
        uint32_t color = grad_color(GRAD_HIST, v / 100.0);
        nc_set(n, color);
        ncplane_putstr_yx(n, y, col, SPARK[std::max(0, std::min(7, idx))]);
    }
}

// Title bar
void draw_titlebar(ncplane* n, int cols) {
    auto up = parse_uptime();
    auto la = parse_loadavg();

    int col = 0;

    // Banner: "Vitals — Resource Monitor" (ASCII dash in TTY mode)
    const char* banner = G.tty_active
        ? " Vitals - Resource Monitor"
        : " Vitals \xe2\x80\x94 Resource Monitor";
    nc_set(n, theme().MAUVE, NCSTYLE_BOLD);
    ncplane_putstr_yx(n, 0, col, banner);
    col += static_cast<int>(strlen(banner));

    // Key-value pairs: [key: value]
    auto kv = [&](const char* key, const std::string& val,
                  uint32_t val_color = theme().TEXT) {
        int need = 4 + static_cast<int>(strlen(key) + val.size());
        if (col + need >= cols) return;

        nc_set(n, theme().OVERLAY0);
        ncplane_putstr_yx(n, 0, col, " ["); col += 2;

        nc_set(n, theme().BLUE);
        ncplane_putstr_yx(n, 0, col, key);
        col += static_cast<int>(strlen(key));

        nc_set(n, val_color);
        ncplane_putstr_yx(n, 0, col, val.c_str());
        col += static_cast<int>(val.size());

        nc_set(n, theme().OVERLAY0);
        ncplane_putstr_yx(n, 0, col, "]"); col++;
    };

    char lbuf[32];
    snprintf(lbuf, sizeof(lbuf), "%.2f %.2f %.2f", la.load1, la.load5, la.load15);

    kv("hostname: ", G.un.hostname,                theme().TEAL);
    kv("IP: ",       G.local_ip,                   theme().TEAL);
    kv("kernel: ",   G.un.kernel_release);
    kv("AVG load: ", lbuf);
    kv("uptime: ",   fmt_uptime(up.uptime_seconds), theme().GREEN);
    kv("time: ",     fmt_time());
}
