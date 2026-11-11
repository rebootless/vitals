#include "panels.h"

void panel_gpu(ncplane* n, int y, int x, int h, int w,
              const std::vector<GpuInfo>& gpus) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "GPU");
    if (ih <= 0 || iw <= 0) return;

    const GpuInfo none{};
    const auto& g = gpus.empty() ? none : gpus.front();

    const int LBL = 7;

    int row = iy;

    nc_set(n, theme().LABEL);
    ncplane_printf_yx(n, row, ix, "%-*s", LBL, "Model:");
    nc_set(n, g.name.empty() ? theme().BORDER : theme().FG);
    ncplane_putstr_yx(n, row, ix + LBL,
                      str_trunc(g.name.empty() ? "N/A" : g.name, iw - LBL).c_str());
    row++;
    if (row >= iy + ih) return;

    draw_pct_row(n, row, ix, iw, LBL, "Util:", g.util_pct, g.util_pct >= 0,
                 pct_color(std::max(0.0, g.util_pct)), GRAD_CPU);
    row++;

    if (g.mem_total_mb > 0 && row < iy + ih) {
        const double mpct = 100.0 * g.mem_used_mb / g.mem_total_mb;
        draw_pct_row(n, row, ix, iw, LBL, "VRAM:", mpct, mpct >= 0, theme().TITLE, GRAD_MEM);
    }
}
