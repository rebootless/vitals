
#include "panels.h"

void panel_cpu(ncplane* n, int y, int x, int h, int w,
               const cpustat&,
               double                      pct,
               const std::vector<cpufreq>& freqs,
               const std::vector<double>&  core_pcts) {

    // raw cpustat is unused: pct and core_pcts are pre-computed
    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "CPU");
    if (ih <= 0 || iw <= 0) return;

    int row = iy;

    // General layout settings
    const int LBL = 9; // Width reserved for information labels

    // Core layout settings
    const int CORE_ID_WIDTH  = 4;   // Width of the core ID: (01)
    const int CORE_PCT_WIDTH = 6;   // Width of the usage block: [  0%]
    const int CORE_FREQ_WIDTH = 10; // Width of the frequency block: [ 800 MHz]
    const int CORE_GROUP_GAP = 1;   // Empty columns between core groups

    if (row < iy + ih) {
        nc_set(n, theme().LABEL);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Model:");
        const bool has_model = !G.ci.model_name.empty();
        nc_set(n, has_model ? theme().FG : theme().BORDER);
        ncplane_putstr_yx(n, row, ix + LBL,
                          str_trunc(has_model ? G.ci.model_name : "N/A", iw - LBL).c_str());
        row++;
    }

    if (row < iy + ih) {
        uint32_t pc = pct_color(pct);
        nc_set(n, theme().LABEL);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Usage:");

        lbr(n, row, ix + LBL);
        if (!core_pcts.empty()) {
            nc_set(n, pc, NCSTYLE_BOLD);
            ncplane_printf_yx(n, row, ix + LBL + 1, "%3.0f%%", pct);
        } else {
            nc_set(n, theme().BORDER);
            ncplane_printf_yx(n, row, ix + LBL + 1, "%4s", "N/A");
        }
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
        nc_set(n, theme().LABEL);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, "History:");

        int bx = ix + LBL, bw = iw - LBL - 2;
        if (bw > 2) {
            lbr(n, row, bx);
            draw_spark(n, row, bx + 1, bw, G.cpu_hist);
            int drawn = std::min(bw, static_cast<int>(G.cpu_hist.size()));
            nc_set(n, theme().BAR_EMPTY);
            for (int i = drawn; i < bw; ++i)
                ncplane_putstr_yx(n, row, bx + 1 + i, " ");
            rbr(n, row, bx + 1 + bw);
        }
        row++;
    }

    if (row < iy + ih) {
        draw_sep(n, row, ix, iw);
        row++;
    }


    if (!core_pcts.empty() && row < iy + ih) {
        int num = static_cast<int>(core_pcts.size());

        // Use two columns when the panel is wide enough
        const int ncol = (iw >= 80) ? 2 : 1;

        // Core group layout
        bool has_freq = !freqs.empty();

        int fixed = CORE_ID_WIDTH + CORE_PCT_WIDTH;
        if (has_freq)
            fixed += CORE_FREQ_WIDTH;

        // Divide the available width between columns
        const int slot = (iw - CORE_GROUP_GAP) / ncol;

        for (int i = 0; i < num; ++i) {
            int r = row + (i / ncol);
            if (r >= iy + ih - 1) break;

            int col = i % ncol;

            // Calculate the starting position and width of each column
            int cx = ix + col * (slot + CORE_GROUP_GAP);
            int col_width = (col == ncol - 1)
            ? iw - (cx - ix)
            : slot;

            double cp = core_pcts[i];
            const int cpu_id = i < static_cast<int>(G.core_ids.size())
            ? G.core_ids[i] : i;

            uint32_t pc = pct_color(cp);

            // Core ID: (01)
            nc_set(n, theme().MUTED);
            ncplane_putstr_yx(n, r, cx, "(");

            nc_set(n, theme().LABEL);
            ncplane_printf_yx(n, r, cx + 1, "%02d", cpu_id);

            nc_set(n, theme().MUTED);
            ncplane_putstr_yx(n, r, cx + 3, ")");

            // Core usage: [  0%]
            int ux = cx + CORE_ID_WIDTH;

            lbr(n, r, ux);
            nc_set(n, pc, NCSTYLE_BOLD);
            ncplane_printf_yx(n, r, ux + 1, "%3.0f%%", cp);
            rbr(n, r, ux + CORE_PCT_WIDTH - 1);

            // Core frequency: [ 800 MHz]
            const auto freq = std::find_if(freqs.begin(), freqs.end(),
                                           [&](const cpufreq& f) { return f.cpu == cpu_id; });

            if (has_freq && freq != freqs.end()) {
                int fx = ux + CORE_PCT_WIDTH;

                lbr(n, r, fx);
                nc_set(n, theme().TITLE);
                ncplane_printf_yx(n, r, fx + 1, "%4.0f MHz",
                                  freq->current_khz / 1000.0);
                rbr(n, r, fx + CORE_FREQ_WIDTH - 1);
            }

            // Usage bar: fill the remaining width of this column
            int bx = cx + fixed;
            int bw = col_width - fixed - 2;

            if (bw > 0) {
                lbr(n, r, bx);
                draw_bar_grad(n, r, bx + 1, bw, cp / 100.0, GRAD_CPU);
                rbr(n, r, bx + 1 + bw);
            }
        }

        row += (num + ncol - 1) / ncol;
    }

    // Footer: physical cores and logical threads
    int frow = iy + ih - 1;
    if (frow >= row && frow >= iy) {
        nc_set(n, theme().LABEL);
        ncplane_putstr_yx(n, frow, ix, "Threads: ");

        if (G.ci.cpu_siblings > 0) {
            nc_set(n, theme().FG);
            ncplane_printf_yx(n, frow, ix + 9, "%d cores / %d threads",
                              G.ci.cpu_cores, G.ci.cpu_siblings);
        } else {
            nc_set(n, theme().BORDER);
            ncplane_putstr_yx(n, frow, ix + 9, "N/A");
        }
    }
}
