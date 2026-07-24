#include "panels.h"
#include "theme.h"
#include "config.h"

#include <clocale>
#include <thread>
#include <chrono>
#include <utility>

// Global state (declared extern in state.h)
AppState G;

// Layout
static void render(notcurses* nc, ncplane* n,
                   const cpustat&                cur_cpu,
                   double                        cpu_pct,
                   const std::vector<netdev>&    cur_net,
                   const std::vector<diskstats>& cur_disk,
                   const std::vector<thermal>&   therm,
                   const std::vector<cpufreq>&   freqs,
                   const std::vector<double>&    core_pcts,
                   const std::vector<HwmonChip>& hwmon,
                   const std::vector<GpuInfo>&   gpus) {

    nc_bg_apply(n);
    ncplane_erase(n);

    unsigned rows_u, cols_u;
    ncplane_dim_yx(n, &rows_u, &cols_u);

    int rows = static_cast<int>(rows_u);
    int cols = static_cast<int>(cols_u);

    draw_titlebar(n, cols);

    int avail = rows - 1;
    int top_h = avail * 3 / 5;
    int bot_h = avail - top_h;

    // GPU sits only under the CPU column, sized to its content (2 border
    // rows + one row per card, capped so a machine with many GPUs doesn't
    // swallow the whole CPU panel) rather than a fixed proportion of top_h.
    // If no GPU was found, CPU keeps the full column height and the panel
    // isn't drawn at all.
    auto split_cpu_gpu = [&](int col_h) -> std::pair<int, int> {
        if (gpus.empty()) return { col_h, 0 };
        int gpu_rows = std::min<int>(4, static_cast<int>(gpus.size()));
        int gpu_h    = gpu_rows + 2; // + top/bottom border
        int cpu_h    = col_h - gpu_h;
        if (cpu_h < 8) { cpu_h = std::max(6, col_h - 2); gpu_h = col_h - cpu_h; }
        return { cpu_h, gpu_h };
    };

    if (cols >= 130) {
        int c1 = cols / 2;
        int c2 = (cols - c1) / 2;
        int c3 = cols - c1 - c2;

        int b1 = cols / 2;
        int b2 = cols - b1;

        auto [cpu_h, gpu_h] = split_cpu_gpu(top_h);

        panel_cpu    (n, 1,        0,     cpu_h, c1,       cur_cpu, cpu_pct, freqs, core_pcts);
        if (gpu_h > 0)
            panel_gpu(n, 1+cpu_h,  0,     gpu_h, c1,       gpus);
        panel_memory (n, 1,        c1,    top_h, c2);
        panel_thermal(n, 1,        c1+c2, top_h, c3,       therm, hwmon, gpus);
        panel_network(n, 1+top_h,  0,     bot_h, b1,       cur_net);
        panel_storage(n, 1+top_h,  b1,    bot_h, b2,       cur_disk);

    } else if (cols >= 80) {
        int half  = cols / 2;
        int mid_h = avail * 2 / 5;
        top_h     = avail * 2 / 5;
        bot_h     = avail - top_h - mid_h;

        auto [cpu_h, gpu_h] = split_cpu_gpu(top_h);

        panel_cpu    (n, 1,             0,    cpu_h, half,      cur_cpu, cpu_pct, freqs, core_pcts);
        if (gpu_h > 0)
            panel_gpu(n, 1+cpu_h,       0,    gpu_h, half,      gpus);
        panel_memory (n, 1,             half, top_h, cols-half);
        panel_network(n, 1+top_h,       0,    mid_h, half,      cur_net);
        panel_storage(n, 1+top_h,       half, mid_h, cols-half, cur_disk);
        panel_thermal(n, 1+top_h+mid_h, 0,    bot_h, cols,      therm, hwmon, gpus);

    } else {
        // Narrow: stacked single-column, GPU right after CPU
        int np = gpus.empty() ? 5 : 6;
        int ph = avail / np, rem = avail % np;

        int r = 0;
        panel_cpu    (n, ph*r,   0, ph, cols, cur_cpu, cpu_pct, freqs, core_pcts); r++;
        if (!gpus.empty()) { panel_gpu(n, ph*r, 0, ph, cols, gpus); r++; }
        panel_memory (n, ph*r,   0, ph, cols); r++;
        panel_network(n, ph*r,   0, ph, cols, cur_net); r++;
        panel_storage(n, ph*r,   0, ph, cols, cur_disk); r++;
        panel_thermal(n, ph*r,   0, ph+rem, cols, therm, hwmon, gpus);
    }

    if (G.settings_open)
        panel_settings(n, rows, cols);

    notcurses_render(nc);
}

// Main
int main() {
    setlocale(LC_ALL, "");

    notcurses_options opts{};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;   // suppress notcurses version line
    notcurses* nc = notcurses_init(&opts, nullptr);
    if (!nc) {
        fprintf(stderr, "notcurses_init failed\n");
        return 1;
    }

    ncplane* n = notcurses_stdplane(nc);
    ncplane_set_bg_default(n);

    // Theme / background — restore from ~/.config/vitals/config, or fall
    // back to the defaults (Catppuccin Mocha, transparent) on first run.
    {
        Config cfg = load_config();
        int ti = find_theme_index(cfg.theme_name);
        set_theme_index(ti >= 0 ? ti : 0);
        G.bg_idx = (cfg.bg_mode == "solid") ? 1 : 0;
    }

    // Static init
    try { G.ci = parse_cpuinfo();     } catch (...) {}
    try { G.un = parse_systemuname(); } catch (...) {}
    G.local_ip = get_local_ip();

    try { G.prev_cpu  = parse_cpustat();   } catch (...) {}
    try { G.prev_core = parse_percpu();    } catch (...) {}
    try { G.prev_net  = parse_netdev();    } catch (...) {}
    try { G.prev_disk = parse_diskstats(); } catch (...) {}
    G.t_prev = Clock::now();

    // Short warm-up so the first delta is meaningful
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Cache of the most recently rendered frame's data. Reused verbatim when
    // the Settings overlay is open, so navigating themes doesn't blank the
    // panels behind it or force a procfs re-read on every keypress.
    cpustat                last_cpu{};
    double                 last_pct = 0.0;
    std::vector<netdev>    last_net;
    std::vector<diskstats> last_disk;
    std::vector<thermal>   last_therm;
    std::vector<cpufreq>   last_freqs;
    std::vector<double>    last_core_pcts;
    std::vector<HwmonChip> last_hwmon;

    // Main loop
    for (;;) {
        ncinput ni{};
        uint32_t ch = notcurses_get_nblock(nc, &ni);

        if (!G.settings_open) {
            if (ch == 'q' || ch == 'Q') break;

            // Some terminals report Escape as ASCII 27 instead of NCKEY_ESC.
            if (ch == NCKEY_ESC || ch == 27) {
                // Snapshot current selection so Esc-to-close-without-saving
                // can revert a live preview the person didn't confirm.
                G.settings_saved_theme = G.theme_idx;
                G.settings_saved_bg    = G.bg_idx;
                G.settings_focus       = 0;
                G.settings_open        = true;
            }
        } else {
            const int n_themes = static_cast<int>(all_themes().size());

            // Some terminals report Escape as ASCII 27 instead of NCKEY_ESC.
            if (ch == NCKEY_ESC || ch == 27) {
                G.theme_idx     = G.settings_saved_theme;
                G.bg_idx        = G.settings_saved_bg;
                G.settings_open = false;

            } else if (ch == '\t') {
                G.settings_focus = 1 - G.settings_focus;

            } else if (ch == NCKEY_UP) {
                if (G.settings_focus == 0)
                    G.theme_idx = (G.theme_idx - 1 + n_themes) % n_themes;
                else
                    G.bg_idx    = (G.bg_idx - 1 + 2) % 2;

            } else if (ch == NCKEY_DOWN) {
                if (G.settings_focus == 0)
                    G.theme_idx = (G.theme_idx + 1) % n_themes;
                else
                    G.bg_idx    = (G.bg_idx + 1) % 2;

            } else if (ch == NCKEY_ENTER || ch == '\n' || ch == '\r') {
                Config cfg;
                cfg.theme_name = current_theme().name;
                cfg.bg_mode    = (G.bg_idx == 1) ? "solid" : "transparent";
                save_config(cfg);
                G.settings_open = false;
            }

            // While the overlay is open, skip this iteration's data refresh —
            // just re-render the last known frame (so panels behind the
            // overlay don't blank out) and poll input again quickly so
            // navigation feels instant.
            render(nc, n, last_cpu, last_pct, last_net, last_disk,
                  last_therm, last_freqs, last_core_pcts, last_hwmon, G.gpus);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        const auto t_now = Clock::now();
        G.dt = std::chrono::duration<double>(t_now - G.t_prev).count();
        if (G.dt < 0.001) G.dt = 1.0;

        // Read current data
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
        try { G.gpus   = parse_gpus();      } catch (...) {}

        // Derived metrics
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

        render(nc, n, cur_cpu, pct, cur_net, cur_disk, therm, freqs, core_pcts, hwmon, G.gpus);

        // Cache for the settings overlay (see main loop top)
        last_cpu       = cur_cpu;
        last_pct       = pct;
        last_net       = cur_net;
        last_disk      = cur_disk;
        last_therm     = therm;
        last_freqs     = freqs;
        last_core_pcts = core_pcts;
        last_hwmon     = hwmon;

        // State rollover
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
