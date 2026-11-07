#include "render.h"
#include "layout.h"

void render(notcurses* nc, ncplane* n) {
    const Sample& s = G.last;
    const std::vector<GpuInfo>& gpus = G.gpus;

    nc_bg_apply(n);
    ncplane_erase(n);

    unsigned rows_u, cols_u;
    ncplane_dim_yx(n, &rows_u, &cols_u);

    int rows = static_cast<int>(rows_u);
    int cols = static_cast<int>(cols_u);

    draw_titlebar(n, cols);

    const Layout L = compute_layout(rows, cols, gpus, panel_visibility(G.opt.panels, G.panel_avail));

    if (const Rect& a = L.panel[P_CPU]; a.h > 0)
        panel_cpu(n, a.y, a.x, a.h, a.w, s.cpu_pct, s.freqs, s.core_pcts);
    if (const Rect& a = L.panel[P_GPU]; a.h > 0)
        panel_gpu(n, a.y, a.x, a.h, a.w, gpus);
    if (const Rect& a = L.panel[P_MEMORY]; a.h > 0)
        panel_memory(n, a.y, a.x, a.h, a.w);
    if (const Rect& a = L.panel[P_NETWORK]; a.h > 0)
        panel_network(n, a.y, a.x, a.h, a.w, s.net, s.net_rx, s.net_tx);
    if (const Rect& a = L.panel[P_STORAGE]; a.h > 0)
        panel_storage(n, a.y, a.x, a.h, a.w, s.disk, s.disk_rd, s.disk_wr);
    if (const Rect& a = L.panel[P_THERMAL]; a.h > 0)
        panel_thermal(n, a.y, a.x, a.h, a.w, s.therm, s.hwmon, gpus);
    if (const Rect& a = L.panel[P_FAN]; a.h > 0)
        panel_fan(n, a.y, a.x, a.h, a.w);

    draw_footer(n, rows, cols);

    if (G.settings_open)
        panel_settings(n, rows, cols);

    notcurses_render(nc);
}
