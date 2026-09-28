#include "panels.h"

void panel_memory(ncplane* n, int y, int x, int h, int w) {
    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Memory");
    if (ih <= 0 || iw <= 0) return;

    const meminfo mi = parse_meminfo();
    int row = iy;
    const int LBL = 8;   // "Memory: " / "Swap: "

    // See mem_used() (sysinfo.cpp) for why this isn't MemTotal - MemAvailable.
    const MemUsed mu = mem_used(mi);

    // Memory: [ 10%][bar]
    if (row < iy + ih) {
        uint32_t pc = pct_color(mu.pct);
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Memory:");

        lbr(n, row, ix + LBL);
        nc_set(n, pc, NCSTYLE_BOLD);
        ncplane_printf_yx(n, row, ix + LBL + 1, "%3.0f%%", mu.pct);
        rbr(n, row, ix + LBL + 5);

        int bx = ix + LBL + 6, bw = iw - (LBL + 6 + 2);
        if (bw > 2) {
            lbr(n, row, bx);
            draw_bar_grad(n, row, bx + 1, bw, mu.pct / 100.0, GRAD_MEM);
            rbr(n, row, bx + 1 + bw);
        }
        row++;
    }

    // "3.19G / 31.30G"
    if (row < iy + ih) {
        nc_set(n, theme().TEXT);
        std::string used_str = fmt_mem_kib(mu.used_kib) + " / " + fmt_mem_kib(mi.MemTotal);
        ncplane_printf_yx(n, row, ix + LBL, "%s",
                          str_trunc(used_str, iw - LBL).c_str());
        row++;
    }

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    // Swap: [pct%][bar]
    if (row < iy + ih) {
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Swap:");

        // Same layout whether or not swap is configured — previously the
        // "no swap" branch hand-formatted " 0%" (one column narrower than
        // "%3.0f%%" ever produces) and closed its bracket one column
        // early, so the bar started one column to the left of where it
        // sits whenever swap *is* in use. Always going through the same
        // "%3.0f%%" formatting keeps the bracket — and therefore the bar
        // — at an identical column regardless of spct.
        const bool   has_swap = mi.SwapTotal > 0;
        const ull    swp_used = (has_swap && mi.SwapTotal > mi.SwapFree)
                                ? mi.SwapTotal - mi.SwapFree : 0;
        const double spct     = has_swap
                                ? 100.0 * static_cast<double>(swp_used) / mi.SwapTotal : 0.0;
        uint32_t sc = has_swap ? pct_color(spct) : theme().SURFACE2;

        lbr(n, row, ix + LBL);
        nc_set(n, sc, NCSTYLE_BOLD);
        ncplane_printf_yx(n, row, ix + LBL + 1, "%3.0f%%", spct);
        rbr(n, row, ix + LBL + 5);

        int bx = ix + LBL + 6, bw = iw - (LBL + 6 + 2);
        if (bw > 2) {
            lbr(n, row, bx);
            draw_bar_grad(n, row, bx + 1, bw, spct / 100.0, GRAD_MEM);
            rbr(n, row, bx + 1 + bw);
        }
        row++;
    }

    // Swap size line
    if (row < iy + ih) {
        nc_set(n, theme().TEXT);
        if (mi.SwapTotal > 0) {
            const ull swp_used = (mi.SwapTotal > mi.SwapFree)
                                 ? mi.SwapTotal - mi.SwapFree : 0;
            std::string s = fmt_mem_kib(swp_used) + " / " + fmt_mem_kib(mi.SwapTotal);
            ncplane_printf_yx(n, row, ix + LBL, "%s",
                              str_trunc(s, iw - LBL).c_str());
        } else {
            ncplane_putstr_yx(n, row, ix + LBL, "0B / 0B");
        }
        row++;
    }

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    // Detail rows — fixed VAL_W column so ')' is always aligned
    struct MemRow { const char* label; ull value; uint32_t color; };
    const std::vector<MemRow> rows = {
        {"Cached",  mi.Cached,  theme().TEAL},
        {"Buffers", mi.Buffers, theme().TEAL},
        {"Slab",    mi.Slab,    theme().TEXT},
        {"Shared",  mi.Shmem,   theme().TEXT},
        {"Dirty",   mi.Dirty,   theme().YELLOW},
    };

    std::vector<std::string> rendered;
    rendered.reserve(rows.size());
    for (const auto& r : rows) rendered.push_back(fmt_mem_kib(r.value));

    const int VAL_W = std::min(11, std::max(6, iw - LBL - 2));

    for (size_t i = 0; i < rows.size() && row < iy + ih; ++i) {
        const auto& r = rows[i];
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, r.label);

        nc_set(n, theme().OVERLAY0);
        ncplane_putstr_yx(n, row, ix + LBL, "(");

        nc_set(n, r.color);
        ncplane_printf_yx(n, row, ix + LBL + 1, "%*s", VAL_W, rendered[i].c_str());

        nc_set(n, theme().OVERLAY0);
        ncplane_putstr_yx(n, row, ix + LBL + 1 + VAL_W, ")");
        row++;
    }
}
