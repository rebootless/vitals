#include "render.h"
#include "theme.h"
#include "config.h"
#include "selftest.h"

#include <clocale>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <utility>

// Global state (declared extern in state.h)
AppState G;

// Refresh-rate presets for the Settings overlay's Left/Right control —
// round, human-meaningful steps rather than a raw +/-100ms increment,
// so reaching either end of the [100ms, 60000ms] range doesn't take
// dozens of keypresses.
static const int REFRESH_STEPS[] = {
    100, 200, 300, 500, 750, 1000, 1500, 2000, 3000, 5000,
    7500, 10000, 15000, 20000, 30000, 45000, 60000
};
static const int REFRESH_STEPS_N = sizeof(REFRESH_STEPS) / sizeof(REFRESH_STEPS[0]);

// Index of the preset closest to G.refresh_ms (in case it was loaded from
// a config file with an off-grid value).
static int refresh_step_index() {
    int best = 0;
    int best_diff = std::abs(REFRESH_STEPS[0] - G.refresh_ms);
    for (int i = 1; i < REFRESH_STEPS_N; ++i) {
        int diff = std::abs(REFRESH_STEPS[i] - G.refresh_ms);
        if (diff < best_diff) { best = i; best_diff = diff; }
    }
    return best;
}

// Main
int main(int argc, char** argv) {
    // Headless self-test for CI: runs before notcurses is initialised.
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--self-test") return run_self_test();

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

    // Theme / background / terminal mode — restore from
    // ~/.config/vitals/config, or fall back to defaults on first run.
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
    std::map<std::string, double> last_net_rx, last_net_tx;
    std::map<std::string, double> last_disk_rd, last_disk_wr;

    // Input is now polled on a short, fixed cadence (INPUT_POLL_MS) that's
    // independent of the data-refresh tick (G.refresh_ms, which can be as
    // long as 60s). Previously a single notcurses_get_nblock() call sat at
    // the top of a loop that then blocked for the full refresh interval —
    // a keypress (q, Esc, ...) only registered once that whole tick had
    // elapsed. Now each loop iteration is cheap (poll input, maybe render
    // from cache) and the expensive procfs read + full render only runs
    // when a tick is actually due, tracked via last_tick below.
    const auto INPUT_POLL = std::chrono::milliseconds(30);

    // Force an immediate first tick regardless of G.refresh_ms.
    auto last_tick = Clock::now() - std::chrono::hours(1);

    // Main loop
    for (;;) {
        ncinput ni{};
        uint32_t ch = notcurses_get_nblock(nc, &ni);

        // Terminals that negotiate the Kitty keyboard protocol (kitty,
        // foot, wezterm, ... — common under Wayland/sway; rare on X11
        // terminals like Konsole/xterm) report separate PRESS and
        // RELEASE events per keystroke instead of a single legacy byte.
        // Every branch below is written for "key was pressed", so treat
        // a RELEASE the same as "no key" (ch == 0) rather than acting on
        // it — otherwise releasing Esc immediately re-triggers whatever
        // branch matches NCKEY_ESC, e.g. closing the just-opened Settings
        // overlay a frame after it opened. Terminals that don't report
        // event types leave evtype at NCTYPE_UNKNOWN, which is left
        // alone here, so behavior is unchanged where this bug can't occur.
        if (ni.evtype == NCTYPE_RELEASE) ch = 0;

        bool settings_was_open = G.settings_open;

        if (!G.settings_open) {
            if (ch == 'q' || ch == 'Q') break;

            // Some terminals report Escape as ASCII 27 instead of NCKEY_ESC.
            if (ch == NCKEY_ESC || ch == 27) {
                // Snapshot current selection so Esc-to-close-without-saving
                // can revert a live preview the person didn't confirm.
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

        // While the overlay is open, data refresh is skipped entirely —
        // only cached (last_*) data is ever rendered, so navigating
        // themes never triggers a procfs re-read. Reopening/closing the
        // overlay also forces a re-render on this same iteration so the
        // transition feels instant rather than waiting for the next poll.
        const auto t_now = Clock::now();
        bool due_for_tick = !G.settings_open &&
            (t_now - last_tick >= std::chrono::milliseconds(G.refresh_ms));

        if (due_for_tick) {
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

            // Per-interface / per-device rates must be computed here, against
            // G.prev_net / G.prev_disk, BEFORE the state rollover below
            // overwrites them with this tick's data. panel_network/panel_storage
            // used to recompute these themselves from the G.prev_* globals at
            // render time, but render() always runs after the rollover, so
            // prev == cur by then and every interface/device rate read as 0
            // (only the totals computed right here, before rollover, ever
            // showed real numbers — which is why Packets/Throughput worked
            // but per-interface RX/TX, and disk Read/Write, stayed at 0).
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
                // History aggregate: whole disks only. Partitions are already
                // counted in their parent; dm-/loop/sr/ram/zram are virtual
                // (same filter as panel_storage).
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

            // Cache for the settings overlay and for in-between polls
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

            // State rollover
            G.prev_cpu  = cur_cpu;
            G.prev_core = std::move(cur_core);
            G.prev_net  = std::move(cur_net);
            G.prev_disk = std::move(cur_disk);
            G.t_prev    = t_now;

            last_tick = t_now;
        }

        // Render every poll (not just on tick) so keypresses — opening/
        // closing Settings, navigating it, quitting — show up within one
        // INPUT_POLL interval instead of waiting for the next full tick.
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
