#include "panels.h"

void panel_memory(ncplane* n, int y, int x, int h, int w) {
    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Memory");
    if (ih <= 0 || iw <= 0) return;

    const meminfo mi = parse_meminfo();
    int row = iy;
    const int LBL = 8;

    const ull used_kb  = (mi.MemTotal > mi.MemAvailable)
                         ? mi.MemTotal - mi.MemAvailable : 0;
    const double rampct = mi.MemTotal > 0
                          ? 100.0 * static_cast<double>(used_kb) / mi.MemTotal : 0.0;

    if (row < iy + ih) {
        uint32_t pc = pct_color(rampct);
        nc_set(n, Cat::BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Memory:");

        lbr(n, row, ix + LBL);
        nc_set(n, pc, NCSTYLE_BOLD);
        ncplane_printf_yx(n, row, ix + LBL + 1, "%3.0f%%", rampct);
        rbr(n, row, ix + LBL + 5);

        int bx = ix + LBL + 6, bw = iw - (LBL + 6 + 2);
        if (bw > 2) {
            lbr(n, row, bx);
            draw_bar_grad(n, row, bx + 1, bw, rampct / 100.0, GRAD_MEM);
            rbr(n, row, bx + 1 + bw);
        }
        row++;
    }

    if (row < iy + ih) {
        nc_set(n, Cat::TEXT);
        std::string used_str = fmt_mem_kib(used_kb) + " / " + fmt_mem_kib(mi.MemTotal);
        ncplane_printf_yx(n, row, ix + LBL, "%s",
                          str_trunc(used_str, iw - LBL).c_str());
        row++;
    }

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    if (row < iy + ih) {
        nc_set(n, Cat::BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Swap:");

        if (mi.SwapTotal > 0) {
            const ull    swp_used = (mi.SwapTotal > mi.SwapFree)
                                    ? mi.SwapTotal - mi.SwapFree : 0;
            const double spct     = 100.0 * static_cast<double>(swp_used) / mi.SwapTotal;
            uint32_t sc = pct_color(spct);

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
        } else {
            lbr(n, row, ix + LBL);
            nc_set(n, Cat::SURFACE2);
            ncplane_putstr_yx(n, row, ix + LBL + 1, " 0%");
            rbr(n, row, ix + LBL + 4);

            int bx = ix + LBL + 5, bw = iw - (LBL + 5 + 2);
            if (bw > 2) {
                lbr(n, row, bx);
                draw_bar_grad(n, row, bx + 1, bw, 0.0, GRAD_MEM);
                rbr(n, row, bx + 1 + bw);
            }
        }
        row++;
    }

    if (row < iy + ih) {
        nc_set(n, Cat::TEXT);
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

    struct MemRow { const char* label; ull value; uint32_t color; };
    const std::vector<MemRow> rows = {
        {"Cached",  mi.Cached,  Cat::TEAL},
        {"Buffers", mi.Buffers, Cat::TEAL},
        {"Slab",    mi.Slab,    Cat::TEXT},
        {"Shared",  mi.Shmem,   Cat::TEXT},
        {"Dirty",   mi.Dirty,   Cat::YELLOW},
    };

    std::vector<std::string> rendered;
    rendered.reserve(rows.size());
    for (const auto& r : rows) rendered.push_back(fmt_mem_kib(r.value));

    const int VAL_W = std::min(11, std::max(6, iw - LBL - 2));

    for (size_t i = 0; i < rows.size() && row < iy + ih; ++i) {
        const auto& r = rows[i];
        nc_set(n, Cat::BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, r.label);

        nc_set(n, Cat::OVERLAY0);
        ncplane_putstr_yx(n, row, ix + LBL, "(");

        nc_set(n, r.color);
        ncplane_printf_yx(n, row, ix + LBL + 1, "%*s", VAL_W, rendered[i].c_str());

        nc_set(n, Cat::OVERLAY0);
        ncplane_putstr_yx(n, row, ix + LBL + 1 + VAL_W, ")");
        row++;
    }
}
