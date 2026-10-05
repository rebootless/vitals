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

    const Layout L = compute_layout(rows, cols, gpus);

    panel_cpu(n, L.cpu.y, L.cpu.x, L.cpu.h, L.cpu.w, cur_cpu, cpu_pct, freqs, core_pcts);
    if (L.gpu.h > 0)
        panel_gpu(n, L.gpu.y, L.gpu.x, L.gpu.h, L.gpu.w, gpus);
    panel_memory (n, L.memory.y,  L.memory.x,  L.memory.h,  L.memory.w);
    panel_network(n, L.network.y, L.network.x, L.network.h, L.network.w,
                  cur_net, net_rx_rate, net_tx_rate);
    panel_storage(n, L.storage.y, L.storage.x, L.storage.h, L.storage.w,
                  cur_disk, disk_rd_rate, disk_wr_rate);
    panel_thermal(n, L.thermal.y, L.thermal.x, L.thermal.h, L.thermal.w,
                  therm, hwmon, gpus);

    if (G.settings_open)
        panel_settings(n, rows, cols);

    notcurses_render(nc);
}
