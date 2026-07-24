#include "panels.h"

// Compact GPU panel — one row per card, in the same visual grammar as the
// per-core grid in panel_cpu.cpp: " (N) [util%][VRAM%][bar]". Temperature
// is intentionally NOT shown here (it now lives in the Thermal panel,
// alongside CPU/hwmon sensors) — this panel is squeezed under CPU, so it
// only carries the two metrics that don't already have a home elsewhere.
void panel_gpu(ncplane* n, int y, int x, int h, int w,
              const std::vector<GpuInfo>& gpus) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "GPU");
    if (ih <= 0 || iw <= 0) return;

    if (gpus.empty()) {
        nc_set(n, theme().SURFACE2);
        ncplane_putstr_yx(n, iy, ix, str_trunc("No GPU found", iw).c_str());
        return;
    }

    // " (N) [util%][VRAM%][bar]"
    // Fixed per-row: 6(prefix) + 6([util%]) + 6([VRAM%], if available)
    for (size_t gi = 0; gi < gpus.size(); ++gi) {
        int r = iy + static_cast<int>(gi);
        if (r >= iy + ih) break;

        const auto& g = gpus[gi];
        bool has_vram = g.mem_total_mb > 0;
        int  fixed    = 6 + 6 + (has_vram ? 6 : 0);
        int  bw       = std::max(4, iw - fixed - 2); // -2 = bar brackets

        // (N)
        nc_set(n, theme().OVERLAY0);
        ncplane_putstr_yx(n, r, ix, " (");
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, r, ix + 2, "%02d", static_cast<int>(gi));
        nc_set(n, theme().OVERLAY0);
        ncplane_putstr_yx(n, r, ix + 4, ") ");

        // [util%]
        if (g.util_pct >= 0) {
            uint32_t pc = pct_color(g.util_pct);
            lbr(n, r, ix + 6);
            nc_set(n, pc, NCSTYLE_BOLD);
            ncplane_printf_yx(n, r, ix + 7, "%3.0f%%", g.util_pct);
            rbr(n, r, ix + 11);
        } else {
            lbr(n, r, ix + 6);
            nc_set(n, theme().SURFACE2);
            ncplane_putstr_yx(n, r, ix + 7, "n/a");
            rbr(n, r, ix + 11);
        }

        // [VRAM%]
        if (has_vram) {
            double mpct = 100.0 * g.mem_used_mb / g.mem_total_mb;
            int vx = ix + 12;
            lbr(n, r, vx);
            nc_set(n, theme().MAUVE, NCSTYLE_BOLD);
            ncplane_printf_yx(n, r, vx + 1, "%3.0f%%", mpct);
            rbr(n, r, vx + 5);
        }

        // [bar] — util%, same gradient family as the CPU Usage bar
        int bx = ix + fixed;
        lbr(n, r, bx);
        double frac = std::max(0.0, g.util_pct) / 100.0;
        draw_bar_grad(n, r, bx + 1, bw, frac, GRAD_CPU);
        rbr(n, r, bx + 1 + bw);
    }
}
