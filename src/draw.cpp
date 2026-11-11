#include "draw.h"
#include "draw_tty.h"
#include "sysinfo.h"

#include <cstring>

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

int str_cols(std::string_view s) {
    int cols = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++cols;
    return cols;
}

static void hline(ncplane* n, int y, int x, int w, const char* glyph,
                  uint32_t color, unsigned style = NCSTYLE_NONE) {
    nc_set(n, color, style);
    for (int i = 0; i < w; ++i)
        ncplane_putstr_yx(n, y, x + i, glyph);
}

std::tuple<int,int,int,int>
draw_box(ncplane* n, int y, int x, int h, int w,
         const std::string& title, const std::string& bl_label) {
    if (h < 2 || w < 2) return {y, x, 0, 0};

    const char *tl, *tr, *bl, *br, *hz, *vt;
    if (G.tty_active) {
        tl = tr = bl = br = "+";
        hz = "-";
        vt = "|";
    } else {
        const bool rounded = G.opt.rounded;
        tl = rounded ? "╭" : "┌";
        tr = rounded ? "╮" : "┐";
        bl = rounded ? "╰" : "└";
        br = rounded ? "╯" : "┘";
        hz = "─";
        vt = "│";
    }

    nc_set(n, theme().BORDER);

    ncplane_putstr_yx(n, y, x, tl);
    hline(n, y, x + 1, w - 2, hz, theme().BORDER);
    ncplane_putstr_yx(n, y, x + w - 1, tr);

    for (int i = 1; i < h - 1; ++i) {
        nc_set(n, theme().BORDER);
        ncplane_putstr_yx(n, y + i, x, vt);
        ncplane_putstr_yx(n, y + i, x + w - 1, vt);

        nc_set(n, theme().BG);
        for (int j = x + 1; j < x + w - 1; ++j)
            ncplane_putstr_yx(n, y + i, j, " ");
    }

    nc_set(n, theme().BORDER);
    ncplane_putstr_yx(n, y + h - 1, x, bl);
    hline(n, y + h - 1, x + 1, w - 2, hz, theme().BORDER);
    ncplane_putstr_yx(n, y + h - 1, x + w - 1, br);

    // labels stop two columns short of each corner
    if (!title.empty() && w > 6) {
        std::string t = " " + str_trunc(title, w - 6) + " ";
        nc_set(n, theme().TITLE, NCSTYLE_BOLD);
        ncplane_putstr_yx(n, y, x + 2, t.c_str());
    }

    if (!bl_label.empty() && w > 6) {
        std::string t = " " + str_trunc(bl_label, w - 6) + " ";
        nc_set(n, theme().LABEL);
        ncplane_putstr_yx(n, y + h - 1, x + 2, t.c_str());
    }

    return {y + 1, x + 1, h - 2, w - 2};
}

void draw_sep(ncplane* n, int y, int x, int w) {
    hline(n, y, x, w, "-", theme().MUTED);
}

void lbr(ncplane* n, int y, int x) {
    nc_set(n, theme().MUTED);
    ncplane_putstr_yx(n, y, x, "[");
}

void rbr(ncplane* n, int y, int x) {
    nc_set(n, theme().MUTED);
    ncplane_putstr_yx(n, y, x, "]");
}

void draw_bar_grad(ncplane* n, int y, int x, int w, double fill, GradType gt) {
    if (G.tty_active) { draw_bar_tty(n, y, x, w, fill, gt); return; }
    if (w <= 0) return;
    fill = std::clamp(fill, 0.0, 1.0);

    double cells = fill * w * 8.0;
    int full  = static_cast<int>(cells / 8.0);
    int frac  = static_cast<int>(cells) % 8;

    for (int i = 0; i < w; ++i) {
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
            nc_set(n, theme().BAR_EMPTY);
            ncplane_putstr_yx(n, y, x + i, BAR_BG);
        }
    }
}

void draw_pct_row(ncplane* n, int y, int x, int w, int label_w, const char* label,
                  double pct, bool valid, uint32_t color, GradType gt) {
    nc_set(n, theme().LABEL);
    ncplane_printf_yx(n, y, x, "%-*s", label_w, label);

    const int px = x + label_w;
    lbr(n, y, px);
    if (valid) {
        nc_set(n, color, NCSTYLE_BOLD);
        ncplane_printf_yx(n, y, px + 1, "%3.0f%%", pct);
    } else {
        nc_set(n, theme().BORDER);
        ncplane_printf_yx(n, y, px + 1, "%4s", "N/A");
    }
    rbr(n, y, px + 5);

    const int bw = w - (label_w + 6 + 2);
    if (bw > 2) {
        lbr(n, y, px + 6);
        draw_bar_grad(n, y, px + 7, bw, valid ? pct / 100.0 : 0.0, gt);
        rbr(n, y, px + 7 + bw);
    }
}

namespace {

struct Ramp { const char* const* glyphs; int n; };

const char* const TTY_RAMP[] = { " ", ".", ",", ":", ";", "=", "+", "*", "#", "@" };

Ramp spark_ramp(bool bottom_row) {
    if (G.tty_active) return { TTY_RAMP, 10 };
    const bool braille = G.opt.braille;
    if (bottom_row && G.opt.mirror)
        return { braille ? SPARK_BRAILLE_MIRROR : SPARK_MIRROR, 8 };
    return { braille ? SPARK_BRAILLE : SPARK, 8 };
}

// Bold only for braille (heavier dots); block glyphs stay regular.
unsigned spark_style() {
    return (!G.tty_active && G.opt.braille) ? NCSTYLE_BOLD : NCSTYLE_NONE;
}

const char* empty_cell() { return G.tty_active ? "." : BAR_BG; }

int first_visible(const std::deque<double>& h, int w) {
    return static_cast<int>(h.size()) > w ? static_cast<int>(h.size()) - w : 0;
}

}

void draw_spark(ncplane* n, int y, int x, int w, const std::deque<double>& hist) {
    if (w <= 0 || hist.empty()) return;
    const Ramp ramp = spark_ramp(false);

    for (int i = first_visible(hist, w), col = x; i < static_cast<int>(hist.size()); ++i, ++col) {
        const double v = std::clamp(hist[i], 0.0, 100.0);
        const int idx  = std::clamp(static_cast<int>(v / 100.0 * (ramp.n - 1) + 0.5), 0, ramp.n - 1);
        nc_set(n, grad_color(GRAD_HIST, v / 100.0), spark_style());
        ncplane_putstr_yx(n, y, col, ramp.glyphs[idx]);
    }
}

void draw_spark_bidir(ncplane* n, int y, int x, int w,
                      const std::deque<double>& top_hist, uint32_t top_color,
                      const std::deque<double>& bot_hist, uint32_t bot_color) {
    if (w <= 0) return;

    auto visible_max = [&](const std::deque<double>& h) {
        double m = 0.0;
        for (int i = first_visible(h, w); i < static_cast<int>(h.size()); ++i) m = std::max(m, h[i]);
        return m;
    };
    const double max_v = std::max({ visible_max(top_hist), visible_max(bot_hist), 1.0 });

    auto draw_row = [&](const std::deque<double>& h, int row, uint32_t color, const Ramp& ramp) {
        const int st = first_visible(h, w);
        const int drawn = static_cast<int>(h.size()) - st;
        for (int i = st, col = x; i < static_cast<int>(h.size()); ++i, ++col) {
            const double v = std::clamp(h[i], 0.0, max_v);
            const int idx  = std::clamp(static_cast<int>(v / max_v * (ramp.n - 1) + 0.5), 0, ramp.n - 1);
            nc_set(n, color, spark_style());
            ncplane_putstr_yx(n, row, col, ramp.glyphs[idx]);
        }
        nc_set(n, theme().BAR_EMPTY);
        for (int col = x + drawn; col < x + w; ++col)
            ncplane_putstr_yx(n, row, col, empty_cell());
    };
    draw_row(top_hist, y,     top_color, spark_ramp(false));
    draw_row(bot_hist, y + 1, bot_color, spark_ramp(true));
}

void draw_pair_header(ncplane* n, int y, int x, int w, const PairCols& c,
                      const char* name, const char* down, const char* up) {
    nc_set(n, theme().LABEL);
    ncplane_printf_yx(n, y, x, "%-*s", c.name_w, name);
    if (c.c2 < w) {
        nc_set(n, theme().DATA_A, c.down_bold ? NCSTYLE_BOLD : NCSTYLE_NONE);
        ncplane_printf_yx(n, y, x + c.c2, "%-14s", down);
    }
    if (c.c3 < w) {
        nc_set(n, theme().DATA_B, c.up_bold ? NCSTYLE_BOLD : NCSTYLE_NONE);
        ncplane_printf_yx(n, y, x + c.c3, "%-14s", up);
    }
}

void draw_pair_row(ncplane* n, int y, int x, int w, const PairCols& c,
                   const std::string& name, const std::string& down, const std::string& up) {
    nc_set(n, theme().FG);
    ncplane_printf_yx(n, y, x, "%-*s", c.name_w, str_trunc(name, c.name_w).c_str());
    if (c.c2 < w) {
        nc_set(n, theme().DATA_A, c.down_bold ? NCSTYLE_BOLD : NCSTYLE_NONE);
        ncplane_printf_yx(n, y, x + c.c2, "%s ", glyph_down());
        nc_set(n, theme().FG);
        ncplane_printf_yx(n, y, x + c.c2 + 2, "%-12s", str_trunc(down, 12).c_str());
    }
    if (c.c3 < w) {
        nc_set(n, theme().DATA_B, c.up_bold ? NCSTYLE_BOLD : NCSTYLE_NONE);
        ncplane_printf_yx(n, y, x + c.c3, "%s ", glyph_up());
        nc_set(n, theme().FG);
        ncplane_printf_yx(n, y, x + c.c3 + 2, "%-12s", str_trunc(up, 12).c_str());
    }
}

void draw_history_pair(ncplane* n, int y, int x, int w, int label_w,
                       const char* down_label, const char* up_label,
                       const std::deque<double>& down_hist, const std::deque<double>& up_hist) {
    const int bx = x + 1 + 1 + label_w + 1;
    const int sx = bx + 1;
    const int sw = w - (sx - x) - 1;

    nc_set(n, theme().DATA_A, NCSTYLE_BOLD);
    ncplane_printf_yx(n, y, x, "%s %-*s ", glyph_down(), label_w, down_label);
    nc_set(n, theme().DATA_B, NCSTYLE_BOLD);
    ncplane_printf_yx(n, y + 1, x, "%s %-*s ", glyph_up(), label_w, up_label);

    if (sw <= 0) return;
    lbr(n, y,     bx);
    lbr(n, y + 1, bx);
    draw_spark_bidir(n, y, sx, sw, down_hist, theme().DATA_A, up_hist, theme().DATA_B);
    rbr(n, y,     sx + sw);
    rbr(n, y + 1, sx + sw);
}

void draw_titlebar(ncplane* n, int cols) {
    const auto& up = G.up;
    const auto& la = G.load;

    int col = 0;

    const char* banner = G.tty_active
        ? " Vitals - Resource Monitor"
        : " Vitals \xe2\x80\x94 Resource Monitor";
    nc_set(n, theme().TITLE, NCSTYLE_BOLD);
    ncplane_putstr_yx(n, 0, col, banner);
    col += str_cols(banner) + 2;

    auto kv = [&](const char* key, const std::string& val,
                  uint32_t val_color = theme().FG) {
        int need = 4 + static_cast<int>(strlen(key) + val.size());
        if (col + need >= cols) return;

        nc_set(n, theme().MUTED);
        ncplane_putstr_yx(n, 0, col, " ["); col += 2;

        nc_set(n, theme().LABEL);
        ncplane_putstr_yx(n, 0, col, key);
        col += static_cast<int>(strlen(key));

        nc_set(n, val_color);
        ncplane_putstr_yx(n, 0, col, val.c_str());
        col += static_cast<int>(val.size());

        nc_set(n, theme().MUTED);
        ncplane_putstr_yx(n, 0, col, "]"); col++;
    };

    char lbuf[32];
    snprintf(lbuf, sizeof(lbuf), "%.2f %.2f %.2f", la.load1, la.load5, la.load15);

    kv("hostname: ", G.un.hostname,                theme().DATA_B);
    kv("IP: ",       G.local_ip,                   theme().DATA_B);
    kv("kernel: ",   G.un.kernel_release);
    kv("AVG load: ", lbuf);
    kv("uptime: ",   fmt_uptime(up.uptime_seconds), theme().LOW);
    kv("time: ",     G.clock);
    if (G.frozen) kv("", "FROZEN", theme().HIGH);
}

void draw_footer(ncplane* n, int rows, int cols) {
    if (rows < 2) return;
    nc_set(n, theme().LABEL);
    ncplane_putstr_yx(n, rows - 1, 1, str_trunc("q:Quit f:Freeze esc:Settings", cols - 1).c_str());
}
