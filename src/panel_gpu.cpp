#include "panels.h"

static uint32_t gpu_temp_color(double temp_c) {
    if (temp_c < 0) return theme().SURFACE2;
    if (temp_c >= 85.0) return theme().RED;
    if (temp_c >= 75.0) return theme().YELLOW;
    return theme().GREEN;
}

void panel_gpu(ncplane* n, int y, int x, int h, int w,
              const std::vector<GpuInfo>& gpus) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "GPU");
    if (ih <= 0 || iw <= 0) return;

    if (gpus.empty()) {
        nc_set(n, theme().SURFACE2);
        ncplane_putstr_yx(n, iy, ix, str_trunc("No GPU found", iw).c_str());
        return;
    }

    int row = iy;
    const int LBL = 9;

    for (size_t gi = 0; gi < gpus.size(); ++gi) {
        const auto& g = gpus[gi];
        if (row >= iy + ih) break;

        nc_set(n, theme().MAUVE, NCSTYLE_BOLD);
        ncplane_putstr_yx(n, row, ix, str_trunc(g.name, iw).c_str());
        row++;
        if (row >= iy + ih) break;

        if (g.util_pct >= 0) {
            uint32_t pc = pct_color(g.util_pct);
            nc_set(n, theme().BLUE);
            ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Util:");

            lbr(n, row, ix + LBL);
            nc_set(n, pc, NCSTYLE_BOLD);
            ncplane_printf_yx(n, row, ix + LBL + 1, "%3.0f%%", g.util_pct);
            rbr(n, row, ix + LBL + 5);

            int bx = ix + LBL + 6, bw = iw - (LBL + 6 + 2);
            if (bw > 2) {
                lbr(n, row, bx);
                draw_bar_grad(n, row, bx + 1, bw, g.util_pct / 100.0, GRAD_CPU);
                rbr(n, row, bx + 1 + bw);
            }
        } else {
            nc_set(n, theme().BLUE);
            ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Util:");
            nc_set(n, theme().SURFACE2);
            ncplane_putstr_yx(n, row, ix + LBL, "n/a");
        }
        row++;
        if (row >= iy + ih) break;

        if (g.mem_total_mb > 0) {
            double mpct = 100.0 * g.mem_used_mb / g.mem_total_mb;
            nc_set(n, theme().BLUE);
            ncplane_printf_yx(n, row, ix, "%-*s", LBL, "VRAM:");

            lbr(n, row, ix + LBL);
            nc_set(n, pct_color(mpct), NCSTYLE_BOLD);
            ncplane_printf_yx(n, row, ix + LBL + 1, "%3.0f%%", mpct);
            rbr(n, row, ix + LBL + 5);

            int bx = ix + LBL + 6, bw = iw - (LBL + 6 + 2);
            if (bw > 2) {
                lbr(n, row, bx);
                draw_bar_grad(n, row, bx + 1, bw, mpct / 100.0, GRAD_MEM);
                rbr(n, row, bx + 1 + bw);
            }
            row++;

            if (row < iy + ih) {
                nc_set(n, theme().TEXT);
                char buf[64];
                snprintf(buf, sizeof(buf), "%.0f MiB / %.0f MiB",
                         g.mem_used_mb, g.mem_total_mb);
                ncplane_putstr_yx(n, row, ix + LBL, str_trunc(buf, iw - LBL).c_str());
                row++;
            }
        } else {
            nc_set(n, theme().BLUE);
            ncplane_printf_yx(n, row, ix, "%-*s", LBL, "VRAM:");
            nc_set(n, theme().SURFACE2);
            ncplane_putstr_yx(n, row, ix + LBL, "n/a");
            row++;
        }
        if (row >= iy + ih) break;

        {
            char tbuf[32] = "n/a", pbuf[32] = "n/a";
            if (g.temp_c  >= 0) snprintf(tbuf, sizeof(tbuf), "+%.0f" DEG_C, g.temp_c);
            if (g.power_w >= 0) snprintf(pbuf, sizeof(pbuf), "%.0fW", g.power_w);

            nc_set(n, theme().BLUE);
            ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Temp:");
            nc_set(n, gpu_temp_color(g.temp_c), NCSTYLE_BOLD);
            ncplane_putstr_yx(n, row, ix + LBL, tbuf);

            int px = ix + LBL + 10;
            if (px < ix + iw - 6) {
                nc_set(n, theme().BLUE);
                ncplane_putstr_yx(n, row, px, "Power: ");
                nc_set(n, theme().PEACH, NCSTYLE_BOLD);
                ncplane_putstr_yx(n, row, px + 7, pbuf);
            }
            row++;
        }

        if (gi + 1 < gpus.size() && row < iy + ih) {
            draw_sep(n, row, ix, iw);
            row++;
        }
    }
}
