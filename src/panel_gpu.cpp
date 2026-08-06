#include "panels.h"

// Compact GPU panel — for the system's one GPU:
//   Model: NVIDIA GeForce GT 730
//   Util: [ 67%][=======bar=======]
//   VRAM: [ 42%][=======bar=======]
// Two separate bars (one per metric) instead of one shared bar, so it's
// obvious which number each bar belongs to. VRAM's row is only drawn when
// the driver actually reports usage. Temperature lives in the Thermal panel
// instead — this panel is squeezed under CPU, so it only carries what has
// no other home.
//
// gpus holds at most one entry (parse_gpus() only reports the primary
// card — see gpu.h) — still a vector so the empty/"no GPU" case doesn't
// need a separate type, not because multiple cards are expected here.
void panel_gpu(ncplane* n, int y, int x, int h, int w,
              const std::vector<GpuInfo>& gpus) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "GPU");
    if (ih <= 0 || iw <= 0) return;

    if (gpus.empty()) {
        nc_set(n, theme().SURFACE2);
        ncplane_putstr_yx(n, iy, ix, str_trunc("No GPU found", iw).c_str());
        return;
    }
    const auto& g = gpus.front();

    const int LBL       = 7; // width of "Model: " / "Util:  " / "VRAM:  " labels
    const int BRACKET_W = 6; // "[" + 4 content chars + "]" (" 67%" / " n/a")
    const int bw = std::max(4, iw - LBL - BRACKET_W - 2); // -2 = bar's own brackets

    // Draws "LABEL: [ nn%][=====bar=====]" at row, with the label dim and
    // the value colored either graded (val_color) or n/a-dimmed.
    auto stat_bar = [&](int row, const char* label, double val,
                        uint32_t val_color, GradType gt) {
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, row, ix, "%-*s", LBL, label);

        int bx = ix + LBL;
        lbr(n, row, bx);
        if (val >= 0) {
            nc_set(n, val_color, NCSTYLE_BOLD);
            ncplane_printf_yx(n, row, bx + 1, "%3.0f%%", val);
        } else {
            nc_set(n, theme().SURFACE2);
            ncplane_printf_yx(n, row, bx + 1, "%4s", "n/a");
        }
        rbr(n, row, bx + BRACKET_W - 1);

        int barx = bx + BRACKET_W;
        lbr(n, row, barx);
        draw_bar_grad(n, row, barx + 1, bw, std::max(0.0, val) / 100.0, gt);
        rbr(n, row, barx + 1 + bw);
    };

    int row = iy;

    // Model: <name>
    nc_set(n, theme().BLUE);
    ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Model:");
    nc_set(n, theme().TEXT);
    ncplane_putstr_yx(n, row, ix + LBL,
                      str_trunc(g.name, iw - LBL).c_str());
    row++;
    if (row >= iy + ih) return;

    // Util: [ nn%][bar]
    stat_bar(row, "Util:", g.util_pct,
            pct_color(std::max(0.0, g.util_pct)), GRAD_CPU);
    row++;

    // VRAM: [ nn%][bar]
    if (g.mem_total_mb > 0 && row < iy + ih) {
        double mpct = 100.0 * g.mem_used_mb / g.mem_total_mb;
        stat_bar(row, "VRAM:", mpct, theme().MAUVE, GRAD_MEM);
    }
}
