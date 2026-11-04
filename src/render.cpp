#include "render.h"
#include "layout.h"

void render(notcurses* nc, ncplane* n,
            const cpustat&                cur_cpu,
            double                        cpu_pct,
            const std::vector<netdev>&    cur_net,
            const std::vector<diskstats>& cur_disk,
            const std::vector<thermal>&   therm,
            const std::vector<cpufreq>&   freqs,
            const std::vector<double>&    core_pcts,
            const std::vector<HwmonChip>& hwmon,
            const std::vector<GpuInfo>&   gpus,
            const std::map<std::string, double>& net_rx_rate,
            const std::map<std::string, double>& net_tx_rate,
            const std::map<std::string, double>& disk_rd_rate,
            const std::map<std::string, double>& disk_wr_rate) {

    nc_bg_apply(n);
    ncplane_erase(n);

    unsigned rows_u, cols_u;
    ncplane_dim_yx(n, &rows_u, &cols_u);

    int rows = static_cast<int>(rows_u);
    int cols = static_cast<int>(cols_u);

    draw_titlebar(n, cols);

    const Layout L = compute_layout(rows, cols, gpus, panel_visibility(G.panel_mode, G.panel_avail));

    if (const Rect& a = L.panel[P_CPU]; a.h > 0)
        panel_cpu(n, a.y, a.x, a.h, a.w, cur_cpu, cpu_pct, freqs, core_pcts);
    if (const Rect& a = L.panel[P_GPU]; a.h > 0)
        panel_gpu(n, a.y, a.x, a.h, a.w, gpus);
    if (const Rect& a = L.panel[P_MEMORY]; a.h > 0)
        panel_memory(n, a.y, a.x, a.h, a.w);
    if (const Rect& a = L.panel[P_NETWORK]; a.h > 0)
        panel_network(n, a.y, a.x, a.h, a.w, cur_net, net_rx_rate, net_tx_rate);
    if (const Rect& a = L.panel[P_STORAGE]; a.h > 0)
        panel_storage(n, a.y, a.x, a.h, a.w, cur_disk, disk_rd_rate, disk_wr_rate);
    if (const Rect& a = L.panel[P_THERMAL]; a.h > 0)
        panel_thermal(n, a.y, a.x, a.h, a.w, therm, hwmon, gpus);
    if (const Rect& a = L.panel[P_FAN]; a.h > 0)
        panel_fan(n, a.y, a.x, a.h, a.w);

    draw_footer(n, rows, cols);

    if (G.settings_open)
        panel_settings(n, rows, cols);

    notcurses_render(nc);
}
