#include "panels.h"

// Compact GPU panel — one row per card, in the same visual grammar as the
// per-core grid in panel_cpu.cpp: " (N) [UTIL: 67%][VRAM: 42%][bar]".
// Temperature is intentionally NOT shown here (it lives in the Thermal
// panel, alongside CPU/hwmon sensors) — this panel is squeezed under CPU,
// so it only carries the two metrics that don't already have a home.
void panel_gpu(ncplane* n, int y, int x, int h, int w,
              const std::vector<GpuInfo>& gpus) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "GPU");
    if (ih <= 0 || iw <= 0) return;

    if (gpus.empty()) {
        nc_set(n, theme().SURFACE2);
        ncplane_putstr_yx(n, iy, ix, str_trunc("No GPU found", iw).c_str());
        return;
    }

    // " (N) [UTIL: 67%][VRAM: 42%][=====bar=====]"
    // Content inside each bracket is a fixed 9 chars either way ("UTIL " +
    // " 67%" / " n/a") so numeric and n/a rows line up in a mixed list.
    for (size_t gi = 0; gi < gpus.size(); ++gi) {
        int r = iy + static_cast<int>(gi);
        if (r >= iy + ih) break;

        const auto& g = gpus[gi];
        bool has_vram   = g.mem_total_mb > 0;
        int  bracket_w  = 11; // "[" + 9 content chars + "]"
        int  fixed      = 6 + bracket_w + (has_vram ? bracket_w : 0);
        int  bw         = std::max(4, iw - fixed - 2); // -2 = bar brackets

        // (N)
        nc_set(n, theme().OVERLAY0);
        ncplane_putstr_yx(n, r, ix, " (");
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, r, ix + 2, "%02d", static_cast<int>(gi));
        nc_set(n, theme().OVERLAY0);
        ncplane_putstr_yx(n, r, ix + 4, ") ");

        // [UTIL: nn%]
        {
            int bx = ix + 6;
            lbr(n, r, bx);
            nc_set(n, theme().SUBTEXT0);
            ncplane_putstr_yx(n, r, bx + 1, "UTIL:");
            if (g.util_pct >= 0) {
                nc_set(n, pct_color(g.util_pct), NCSTYLE_BOLD);
                ncplane_printf_yx(n, r, bx + 6, "%3.0f%%", g.util_pct);
            } else {
                nc_set(n, theme().SURFACE2);
                ncplane_printf_yx(n, r, bx + 6, "%4s", "n/a");
            }
            rbr(n, r, bx + bracket_w - 1);
        }

        // [VRAM: nn%] — only when the driver actually reports VRAM usage
        // (AMD does; Intel iGPUs and nouveau currently don't).
        if (has_vram) {
            double mpct = 100.0 * g.mem_used_mb / g.mem_total_mb;
            int bx = ix + 6 + bracket_w;
            lbr(n, r, bx);
            nc_set(n, theme().SUBTEXT0);
            ncplane_putstr_yx(n, r, bx + 1, "VRAM:");
            nc_set(n, theme().MAUVE, NCSTYLE_BOLD);
            ncplane_printf_yx(n, r, bx + 6, "%3.0f%%", mpct);
            rbr(n, r, bx + bracket_w - 1);
        }

        // [bar] — util%, same gradient family as the CPU Usage bar
        int bx = ix + fixed;
        lbr(n, r, bx);
        double frac = std::max(0.0, g.util_pct) / 100.0;
        draw_bar_grad(n, r, bx + 1, bw, frac, GRAD_CPU);
        rbr(n, r, bx + 1 + bw);
    }
}
