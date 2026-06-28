#include "panels.h"

#include <clocale>
#include <thread>
#include <chrono>

AppState G;

static void render(notcurses* nc, ncplane* n,
                   const cpustat&                cur_cpu,
                   double                        cpu_pct,
                   const std::vector<netdev>&    cur_net,
                   const std::vector<diskstats>& cur_disk,
                   const std::vector<thermal>&   therm,
                   const std::vector<cpufreq>&   freqs,
                   const std::vector<double>&    core_pcts,
                   const std::vector<HwmonChip>& hwmon) {

    ncplane_erase(n);

    unsigned rows_u, cols_u;
    ncplane_dim_yx(n, &rows_u, &cols_u);
    int rows = static_cast<int>(rows_u);
    int cols = static_cast<int>(cols_u);

    ncplane_set_bg_default(n);

    draw_titlebar(n, cols);

    int avail = rows - 1;
    int top_h = avail * 3 / 5;
    int bot_h = avail - top_h;

    if (cols >= 130) {
        int c1 = cols / 2;
        int c2 = (cols - c1) / 2;
        int c3 = cols - c1 - c2;

        panel_cpu    (n, 1,        0,     top_h, c1,       cur_cpu, cpu_pct, freqs, core_pcts);
        panel_memory (n, 1,        c1,    top_h, c2);
        panel_thermal(n, 1,        c1+c2, top_h, c3,       therm, hwmon);
        panel_network(n, 1+top_h,  0,     bot_h, c1,       cur_net);
        panel_storage(n, 1+top_h,  c1,    bot_h, cols-c1,  cur_disk);

    } else if (cols >= 80) {
        int half  = cols / 2;
        int mid_h = avail * 2 / 5;
        top_h     = avail * 2 / 5;
        bot_h     = avail - top_h - mid_h;

        panel_cpu    (n, 1,             0,    top_h, half,      cur_cpu, cpu_pct, freqs, core_pcts);
        panel_memory (n, 1,             half, top_h, cols-half);
        panel_network(n, 1+top_h,       0,    mid_h, half,      cur_net);
        panel_storage(n, 1+top_h,       half, mid_h, cols-half, cur_disk);
        panel_thermal(n, 1+top_h+mid_h, 0,    bot_h, cols,      therm, hwmon);

    } else {
        int np = 5, ph = avail / np, rem = avail % np;
        panel_cpu    (n, ph*0, 0, ph,      cols, cur_cpu, cpu_pct, freqs, core_pcts);
        panel_memory (n, ph*1, 0, ph,      cols);
        panel_network(n, ph*2, 0, ph,      cols, cur_net);
        panel_storage(n, ph*3, 0, ph,      cols, cur_disk);
        panel_thermal(n, ph*4, 0, ph+rem,  cols, therm, hwmon);
    }

    notcurses_render(nc);
}

int main() {
    setlocale(LC_ALL, "");

    notcurses_options opts{};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;
    notcurses* nc = notcurses_init(&opts, nullptr);
    if (!nc) {
        fprintf(stderr, "notcurses_init failed\n");
        return 1;
    }

    ncplane* n = notcurses_stdplane(nc);
    ncplane_set_bg_default(n);

    try { G.ci = parse_cpuinfo();     } catch (...) {}
    try { G.un = parse_systemuname(); } catch (...) {}
    G.local_ip = get_local_ip();

    try { G.prev_cpu  = parse_cpustat();   } catch (...) {}
    try { G.prev_core = parse_percpu();    } catch (...) {}
    try { G.prev_net  = parse_netdev();    } catch (...) {}
    try { G.prev_disk = parse_diskstats(); } catch (...) {}
    G.t_prev = Clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (;;) {
        ncinput ni{};
        uint32_t ch = notcurses_get_nblock(nc, &ni);
        if (ch == 'q' || ch == 'Q') break;

        const auto t_now = Clock::now();
        G.dt = std::chrono::duration<double>(t_now - G.t_prev).count();
        if (G.dt < 0.001) G.dt = 1.0;

        cpustat                cur_cpu{};
        std::vector<cpustat>   cur_core;
        std::vector<netdev>    cur_net;
        std::vector<diskstats> cur_disk;
        std::vector<thermal>   therm;
        std::vector<cpufreq>   freqs;
        std::vector<HwmonChip> hwmon;

        try { cur_cpu  = parse_cpustat();   } catch (...) { cur_cpu = G.prev_cpu; }
        try { cur_core = parse_percpu();    } catch (...) {}
        try { cur_net  = parse_netdev();    } catch (...) {}
        try { cur_disk = parse_diskstats(); } catch (...) {}
        try { therm    = parse_thermal();   } catch (...) {}
        try { freqs    = parse_cpufreq();   } catch (...) {}
        try { hwmon    = parse_hwmon();     } catch (...) {}

        const double pct = cpu_delta(G.prev_cpu, cur_cpu);
        G.cpu_hist.push_back(pct);
        if (static_cast<int>(G.cpu_hist.size()) > 200)
            G.cpu_hist.pop_front();

        std::vector<double> core_pcts;
        core_pcts.reserve(cur_core.size());
        for (size_t i = 0; i < cur_core.size(); ++i) {
            core_pcts.push_back(
                i < G.prev_core.size()
                ? cpu_delta(G.prev_core[i], cur_core[i])
                : 0.0);
        }

        double rx_now = 0.0, tx_now = 0.0;
        for (const auto& nd : cur_net) {
            if (nd.interface == "lo") continue;
            for (const auto& p : G.prev_net) {
                if (p.interface != nd.interface) continue;
                if (nd.rx_bytes >= p.rx_bytes)
                    rx_now += static_cast<double>(nd.rx_bytes - p.rx_bytes) / G.dt;
                if (nd.tx_bytes >= p.tx_bytes)
                    tx_now += static_cast<double>(nd.tx_bytes - p.tx_bytes) / G.dt;
                break;
            }
        }
        G.peak_rx = std::max(G.peak_rx, rx_now);
        G.peak_tx = std::max(G.peak_tx, tx_now);

        render(nc, n, cur_cpu, pct, cur_net, cur_disk, therm, freqs, core_pcts, hwmon);

        G.prev_cpu  = cur_cpu;
        G.prev_core = std::move(cur_core);
        G.prev_net  = std::move(cur_net);
        G.prev_disk = std::move(cur_disk);
        G.t_prev    = t_now;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    notcurses_stop(nc);
    return 0;
}
