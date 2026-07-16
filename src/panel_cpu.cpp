#include "panels.h"

void panel_cpu(ncplane* n, int y, int x, int h, int w,
               const cpustat&              cur,
               double                      pct,
               const std::vector<cpufreq>& freqs,
               const std::vector<double>&  core_pcts) {

    (void)cur;
    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "CPU");
    if (ih <= 0 || iw <= 0) return;

    int row = iy;
    const int LBL = 9;

    if (row < iy + ih) {
        nc_set(n, Cat::BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Model:");
        nc_set(n, Cat::TEXT);
        ncplane_putstr_yx(n, row, ix + LBL,
                          str_trunc(G.ci.model_name, iw - LBL).c_str());
        row++;
    }

    if (row < iy + ih) {
        uint32_t pc = pct_color(pct);
        nc_set(n, Cat::BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Usage:");

        lbr(n, row, ix + LBL);
        nc_set(n, pc, NCSTYLE_BOLD);
        ncplane_printf_yx(n, row, ix + LBL + 1, "%3.0f%%", pct);
        rbr(n, row, ix + LBL + 5);

        int bx = ix + LBL + 6, bw = iw - (LBL + 6 + 2);
        if (bw > 2) {
            lbr(n, row, bx);
            draw_bar_grad(n, row, bx + 1, bw, pct / 100.0, GRAD_CPU);
            rbr(n, row, bx + 1 + bw);
        }
        row++;
    }

    if (row < iy + ih) {
        nc_set(n, Cat::BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "History:");

        int bx = ix + LBL, bw = iw - LBL - 2;
        if (bw > 2) {
            lbr(n, row, bx);
            draw_spark(n, row, bx + 1, bw, G.cpu_hist);
            int drawn = std::min(bw, static_cast<int>(G.cpu_hist.size()));
            nc_set(n, Cat::SURFACE1);
            for (int i = drawn; i < bw; ++i)
                ncplane_putstr_yx(n, row, bx + 1 + i, " ");
            rbr(n, row, bx + 1 + bw);
        }
        row++;
    }

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    if (!core_pcts.empty() && row < iy + ih) {
        int num  = static_cast<int>(core_pcts.size());
        int ncol = (iw >= 80) ? 2 : 1;
        int slot = iw / ncol;
        bool has_freq = !freqs.empty();
        int fixed = 6 + 6 + (has_freq ? 10 : 0);
        int bw    = std::max(4, slot - fixed - 2);

        for (int i = 0; i < num; ++i) {
            int r  = row + (i / ncol);
            if (r >= iy + ih - 1) break;
            int cx = ix + (i % ncol) * slot;
            double cp = core_pcts[i];
            uint32_t pc = pct_color(cp);

            nc_set(n, Cat::OVERLAY0);
            ncplane_putstr_yx(n, r, cx, " (");
            nc_set(n, Cat::BLUE);
            ncplane_printf_yx(n, r, cx + 2, "%02d", i);
            nc_set(n, Cat::OVERLAY0);
            ncplane_putstr_yx(n, r, cx + 4, ") ");

            lbr(n, r, cx + 6);
            nc_set(n, pc, NCSTYLE_BOLD);
            ncplane_printf_yx(n, r, cx + 7, "%3.0f%%", cp);
            rbr(n, r, cx + 11);

            if (has_freq && i < static_cast<int>(freqs.size())) {
                int fx = cx + 12;
                lbr(n, r, fx);
                nc_set(n, Cat::MAUVE);
                ncplane_printf_yx(n, r, fx + 1, "%4.0f MHz",
                                  freqs[i].current_khz / 1000.0);
                rbr(n, r, fx + 9);
            }

            int bx = cx + fixed;
            lbr(n, r, bx);
            draw_bar_grad(n, r, bx + 1, bw, cp / 100.0, GRAD_CPU);
            rbr(n, r, bx + 1 + bw);
        }
        row += (num + ncol - 1) / ncol;
    }

    int frow = iy + ih - 1;
    if (frow >= row && frow >= iy) {
        nc_set(n, Cat::BLUE);
        ncplane_putstr_yx(n, frow, ix, "Threads: ");
        nc_set(n, Cat::TEXT);
        ncplane_printf_yx(n, frow, ix + 9, "%d cores / %d threads",
                          G.ci.cpu_cores, G.ci.cpu_siblings);
    }
}
