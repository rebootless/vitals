#include "render.h"
#include "theme.h"
#include "config.h"
#include "selftest.h"

#include <clocale>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <utility>

AppState G;

static const int REFRESH_STEPS[] = {
    100, 200, 300, 500, 750, 1000, 1500, 2000, 3000, 5000,
    7500, 10000, 15000, 20000, 30000, 45000, 60000
};
static const int REFRESH_STEPS_N = sizeof(REFRESH_STEPS) / sizeof(REFRESH_STEPS[0]);

static int refresh_step_index() {
    int best = 0;
    int best_diff = std::abs(REFRESH_STEPS[0] - G.refresh_ms);
    for (int i = 1; i < REFRESH_STEPS_N; ++i) {
        int diff = std::abs(REFRESH_STEPS[i] - G.refresh_ms);
        if (diff < best_diff) { best = i; best_diff = diff; }
    }
    return best;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--self-test") return run_self_test();

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

    {
        Config cfg = load_config();
        int ti = find_theme_index(cfg.theme_name);
        set_theme_index(ti >= 0 ? ti : 0);
        G.bg_idx     = (cfg.bg_mode == "solid") ? 1 : 0;
        G.tty_force  = tty_force_from_string(cfg.tty_mode);
        G.tty_active = resolve_tty_active(G.tty_force);
        G.refresh_ms = cfg.refresh_ms;
        G.corners_idx = (cfg.corners == "rounded") ? 1 : 0;
        G.graph_style_idx = (cfg.graph_style == "braille") ? 1 : 0;
        G.symbol_mirror_idx = cfg.symbol_mirroring ? 1 : 0;
    }

    try { G.ci = parse_cpuinfo();     } catch (...) {}
    try { G.un = parse_systemuname(); } catch (...) {}
    G.local_ip = get_local_ip();

    try { G.prev_cpu  = parse_cpustat();   } catch (...) {}
    try { G.prev_core = parse_percpu();    } catch (...) {}
    try { G.prev_net  = parse_netdev();    } catch (...) {}
    try { G.prev_disk = parse_diskstats(); } catch (...) {}
    G.t_prev = Clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    cpustat                last_cpu{};
    double                 last_pct = 0.0;
    std::vector<netdev>    last_net;
    std::vector<diskstats> last_disk;
    std::vector<thermal>   last_therm;
    std::vector<cpufreq>   last_freqs;
    std::vector<double>    last_core_pcts;
    std::vector<HwmonChip> last_hwmon;
    std::map<std::string, double> last_net_rx, last_net_tx;
    std::map<std::string, double> last_disk_rd, last_disk_wr;

    // input is polled every INPUT_POLL_MS, independent of the refresh tick (up to 60s)
    const auto INPUT_POLL = std::chrono::milliseconds(30);

    auto last_tick = Clock::now() - std::chrono::hours(1);

    for (;;) {
        ncinput ni{};
        uint32_t ch = notcurses_get_nblock(nc, &ni);

        // Kitty protocol sends RELEASE events too; ignore them or Esc-up re-closes Settings
        if (ni.evtype == NCTYPE_RELEASE) ch = 0;

        bool settings_was_open = G.settings_open;

        if (!G.settings_open) {
            if (ch == 'q' || ch == 'Q') break;

            if (ch == 'f' || ch == 'F') {
                G.frozen = !G.frozen;
                if (!G.frozen) last_tick = Clock::now() - std::chrono::hours(1);
            }

            // Some terminals report Escape as ASCII 27 instead of NCKEY_ESC.
            if (ch == NCKEY_ESC || ch == 27) {
                G.settings_saved_theme   = G.theme_idx;
                G.settings_saved_bg      = G.bg_idx;
                G.settings_saved_tty     = G.tty_force;
                G.settings_saved_refresh = G.refresh_ms;
                G.settings_saved_corners = G.corners_idx;
                G.settings_saved_graph_style = G.graph_style_idx;
                G.settings_saved_symbol_mirror = G.symbol_mirror_idx;
                G.settings_focus         = 0;
                G.settings_open          = true;
            }
        } else {
            const int n_themes = static_cast<int>(all_themes().size());

            // Some terminals report Escape as ASCII 27 instead of NCKEY_ESC.
            if (ch == NCKEY_ESC || ch == 27) {
                G.theme_idx     = G.settings_saved_theme;
                G.bg_idx        = G.settings_saved_bg;
                G.tty_force     = G.settings_saved_tty;
                G.tty_active    = resolve_tty_active(G.tty_force);
                G.refresh_ms    = G.settings_saved_refresh;
                G.corners_idx   = G.settings_saved_corners;
                G.graph_style_idx = G.settings_saved_graph_style;
                G.symbol_mirror_idx = G.settings_saved_symbol_mirror;
                G.settings_open = false;

            } else if (ch == '\t') {
                G.settings_focus = (G.settings_focus + 1) % 7;

            } else if (ch == NCKEY_UP) {
                if (G.settings_focus == 0) {
                    G.theme_idx = (G.theme_idx - 1 + n_themes) % n_themes;
                } else if (G.settings_focus == 1) {
                    G.bg_idx    = (G.bg_idx - 1 + 2) % 2;
                } else if (G.settings_focus == 2) {
                    int f = (static_cast<int>(G.tty_force) - 1 + 3) % 3;
                    G.tty_force  = static_cast<TtyForce>(f);
                    G.tty_active = resolve_tty_active(G.tty_force);
                } else if (G.settings_focus == 3) {
                    G.corners_idx = (G.corners_idx - 1 + 2) % 2;
                } else if (G.settings_focus == 4) {
                    G.graph_style_idx = (G.graph_style_idx - 1 + 2) % 2;
                } else if (G.settings_focus == 5) {
                    G.symbol_mirror_idx = (G.symbol_mirror_idx - 1 + 2) % 2;
                } else {
                    int idx = std::min(REFRESH_STEPS_N - 1, refresh_step_index() + 1);
                    G.refresh_ms = REFRESH_STEPS[idx];
                }

            } else if (ch == NCKEY_DOWN) {
                if (G.settings_focus == 0) {
                    G.theme_idx = (G.theme_idx + 1) % n_themes;
                } else if (G.settings_focus == 1) {
                    G.bg_idx    = (G.bg_idx + 1) % 2;
                } else if (G.settings_focus == 2) {
                    int f = (static_cast<int>(G.tty_force) + 1) % 3;
                    G.tty_force  = static_cast<TtyForce>(f);
                    G.tty_active = resolve_tty_active(G.tty_force);
                } else if (G.settings_focus == 3) {
                    G.corners_idx = (G.corners_idx + 1) % 2;
                } else if (G.settings_focus == 4) {
                    G.graph_style_idx = (G.graph_style_idx + 1) % 2;
                } else if (G.settings_focus == 5) {
                    G.symbol_mirror_idx = (G.symbol_mirror_idx + 1) % 2;
                } else {
                    int idx = std::max(0, refresh_step_index() - 1);
                    G.refresh_ms = REFRESH_STEPS[idx];
                }

            } else if (ch == NCKEY_ENTER || ch == '\n' || ch == '\r') {
                Config cfg;
                cfg.theme_name = current_theme().name;
                cfg.bg_mode    = (G.bg_idx == 1) ? "solid" : "transparent";
                cfg.tty_mode   = tty_force_to_string(G.tty_force);
                cfg.refresh_ms = G.refresh_ms;
                cfg.corners    = (G.corners_idx == 1) ? "rounded" : "square";
                cfg.graph_style = (G.graph_style_idx == 1) ? "braille" : "sparkline";
                cfg.symbol_mirroring = (G.symbol_mirror_idx == 1);
                save_config(cfg);
                G.settings_open = false;
            }
        }

        const auto t_now = Clock::now();
        bool due_for_tick = !G.settings_open && !G.frozen &&
            (t_now - last_tick >= std::chrono::milliseconds(G.refresh_ms));

        if (due_for_tick) {
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
            try { G.gpus   = parse_gpus();      } catch (...) {}

            const double pct = cpu_delta(G.prev_cpu, cur_cpu);
            G.cpu_hist.push_back(pct);
            if (static_cast<int>(G.cpu_hist.size()) > HIST_CAP)
                G.cpu_hist.pop_front();

            std::vector<double> core_pcts;
            core_pcts.reserve(cur_core.size());
            for (size_t i = 0; i < cur_core.size(); ++i) {
                core_pcts.push_back(
                    i < G.prev_core.size()
                    ? cpu_delta(G.prev_core[i], cur_core[i])
                    : 0.0);
            }

            // per-interface/device rates must use G.prev_* before the rollover below overwrites them
            std::map<std::string, double> net_rx_rate, net_tx_rate;
            double rx_now = 0.0, tx_now = 0.0;
            for (const auto& nd : cur_net) {
                if (is_hidden_iface(nd.interface)) continue;
                double rx = 0.0, tx = 0.0;
                for (const auto& p : G.prev_net) {
                    if (p.interface != nd.interface) continue;
                    rx = net_rate_bytes(p.rx_bytes, nd.rx_bytes, G.dt);
                    tx = net_rate_bytes(p.tx_bytes, nd.tx_bytes, G.dt);
                    break;
                }
                net_rx_rate[nd.interface] = rx;
                net_tx_rate[nd.interface] = tx;
                rx_now += rx;
                tx_now += tx;
            }
            G.peak_rx = std::max(G.peak_rx, rx_now);
            G.peak_tx = std::max(G.peak_tx, tx_now);

            G.net_rx_hist.push_back(rx_now);
            if (static_cast<int>(G.net_rx_hist.size()) > HIST_CAP)
                G.net_rx_hist.pop_front();
            G.net_tx_hist.push_back(tx_now);
            if (static_cast<int>(G.net_tx_hist.size()) > HIST_CAP)
                G.net_tx_hist.pop_front();

            double disk_rd_now = 0.0, disk_wr_now = 0.0;
            std::map<std::string, double> disk_rd_rate, disk_wr_rate;
            for (const auto& ds : cur_disk) {
                double rd = 0.0, wr = 0.0;
                for (const auto& p : G.prev_disk) {
                    if (p.device != ds.device) continue;
                    DiskRate r = disk_rate(p.sectors_read,    ds.sectors_read,
                                           p.sectors_written, ds.sectors_written, G.dt);
                    rd = r.read_bytes_per_sec;
                    wr = r.write_bytes_per_sec;
                    break;
                }
                disk_rd_rate[ds.device] = rd;
                disk_wr_rate[ds.device] = wr;
                // whole disks only: partitions count in their parent; dm-/loop/sr/ram/zram are virtual
                const std::string& d = ds.device;
                const bool virt = d.rfind("dm-", 0) == 0 || d.rfind("loop", 0) == 0 ||
                                  d.rfind("sr", 0) == 0   || d.rfind("ram", 0) == 0  ||
                                  d.rfind("zram", 0) == 0;
                if (virt || !parent_of(d).empty()) continue;
                disk_rd_now += rd;
                disk_wr_now += wr;
            }

            G.disk_rd_hist.push_back(disk_rd_now);
            if (static_cast<int>(G.disk_rd_hist.size()) > HIST_CAP)
                G.disk_rd_hist.pop_front();
            G.disk_wr_hist.push_back(disk_wr_now);
            if (static_cast<int>(G.disk_wr_hist.size()) > HIST_CAP)
                G.disk_wr_hist.pop_front();

            last_cpu       = cur_cpu;
            last_pct       = pct;
            last_net       = cur_net;
            last_disk      = cur_disk;
            last_therm     = therm;
            last_freqs     = freqs;
            last_core_pcts = core_pcts;
            last_hwmon     = hwmon;
            last_net_rx    = std::move(net_rx_rate);
            last_net_tx    = std::move(net_tx_rate);
            last_disk_rd   = std::move(disk_rd_rate);
            last_disk_wr   = std::move(disk_wr_rate);

            G.prev_cpu  = cur_cpu;
            G.prev_core = std::move(cur_core);
            G.prev_net  = std::move(cur_net);
            G.prev_disk = std::move(cur_disk);
            G.t_prev    = t_now;

            last_tick = t_now;
        }

        if (due_for_tick || settings_was_open || G.settings_open || ch != 0) {
            render(nc, n, last_cpu, last_pct, last_net, last_disk,
                  last_therm, last_freqs, last_core_pcts, last_hwmon, G.gpus,
                  last_net_rx, last_net_tx, last_disk_rd, last_disk_wr);
        }

        std::this_thread::sleep_for(INPUT_POLL);
    }

    notcurses_stop(nc);
    return 0;
}
