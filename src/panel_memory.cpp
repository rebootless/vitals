#include "panels.h"

void panel_memory(ncplane* n, int y, int x, int h, int w) {
    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Memory");
    if (ih <= 0 || iw <= 0) return;

    const meminfo& mi = G.mem;
    int row = iy;
    const int LBL = 8;

    // See mem_used() (sysinfo.cpp) for why this isn't MemTotal - MemAvailable.
    const MemUsed mu = mem_used(mi);
    const bool has_mem = mi.MemTotal > 0;

    if (row < iy + ih) {
        draw_pct_row(n, row, ix, iw, LBL, "Memory:", mu.pct, has_mem, pct_color(mu.pct), GRAD_MEM);
        row++;
    }

    if (row < iy + ih) {
        nc_set(n, has_mem ? theme().FG : theme().BORDER);
        const std::string used_str = has_mem
            ? fmt_mem_kib(mu.used_kib) + " / " + fmt_mem_kib(mi.MemTotal) : "N/A";
        ncplane_printf_yx(n, row, ix + LBL, "%s",
                          str_trunc(used_str, iw - LBL).c_str());
        row++;
    }

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    const bool   has_swap = mi.SwapTotal > 0;
    const ull    swp_used = mi.SwapTotal > mi.SwapFree ? mi.SwapTotal - mi.SwapFree : 0;
    const double spct     = has_swap ? 100.0 * static_cast<double>(swp_used) / mi.SwapTotal : 0.0;

    if (row < iy + ih) {
        draw_pct_row(n, row, ix, iw, LBL, "Swap:", spct, true,
                     has_swap ? pct_color(spct) : theme().BORDER, GRAD_MEM);
        row++;
    }

    if (row < iy + ih) {
        nc_set(n, theme().FG);
        const std::string s = has_swap
            ? fmt_mem_kib(swp_used) + " / " + fmt_mem_kib(mi.SwapTotal) : "0 B / 0 B";
        ncplane_printf_yx(n, row, ix + LBL, "%s", str_trunc(s, iw - LBL).c_str());
        row++;
    }

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    struct MemRow { const char* label; ull value; uint32_t color; };
    const MemRow rows[] = {
        {"Cached",  mi.Cached,  theme().DATA_B},
        {"Buffers", mi.Buffers, theme().DATA_B},
        {"Slab",    mi.Slab,    theme().FG},
        {"Shared",  mi.Shmem,   theme().FG},
        {"Dirty",   mi.Dirty,   theme().MID},
    };

    const int VAL_W = std::min(11, std::max(6, iw - LBL - 2));

    for (const MemRow& r : rows) {
        if (row >= iy + ih) break;
        nc_set(n, theme().LABEL);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, r.label);

        nc_set(n, theme().MUTED);
        ncplane_putstr_yx(n, row, ix + LBL, "(");

        nc_set(n, r.color);
        ncplane_printf_yx(n, row, ix + LBL + 1, "%*s", VAL_W, fmt_mem_kib(r.value).c_str());

        nc_set(n, theme().MUTED);
        ncplane_putstr_yx(n, row, ix + LBL + 1 + VAL_W, ")");
        row++;
    }
}
