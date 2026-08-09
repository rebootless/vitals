#include "panels.h"
#include "theme.h"
#include "config.h"

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
    // rows + 2-3 rows — "Model:", a UTIL bar, and a VRAM bar if the driver
    // reports it, same as panel_gpu.cpp) rather than a fixed proportion of
    // top_h. If no GPU was found, CPU keeps the full column height and the
    // panel isn't drawn at all. gpus holds at most one entry — see gpu.h.
    //
    // GPU's content budget is satisfied FIRST (up to what it actually
    // needs), and CPU gets whatever remains down to a legible floor —
    // the reverse priority silently starved GPU down to border-only (0
    // content rows) on shorter terminals, which looked like "GPU doesn't
    // render" even though data was present and correct.
    auto split_cpu_gpu = [&](int col_h) -> std::pair<int, int> {
        if (gpus.empty()) return { col_h, 0 };
        int gpu_rows = (gpus.front().mem_total_mb > 0) ? 3 : 2;
        int gpu_want = gpu_rows + 2; // + top/bottom border

        const int CPU_MIN = 5; // border(2) + Model + Usage + History, the bare minimum to stay legible
        int gpu_h = std::min(gpu_want, std::max(0, col_h - CPU_MIN));
        int cpu_h = col_h - gpu_h;
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
                G.settings_open = false;

            } else if (ch == '\t') {
                G.settings_focus = (G.settings_focus + 1) % 5;

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

            // Cache for the settings overlay and for in-between polls
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

            last_tick = t_now;
        }

        // Render every poll (not just on tick) so keypresses — opening/
        // closing Settings, navigating it, quitting — show up within one
        // INPUT_POLL interval instead of waiting for the next full tick.
        if (due_for_tick || settings_was_open || G.settings_open || ch != 0) {
            render(nc, n, last_cpu, last_pct, last_net, last_disk,
                  last_therm, last_freqs, last_core_pcts, last_hwmon, G.gpus);
        }

        std::this_thread::sleep_for(INPUT_POLL);
    }

    notcurses_stop(nc);
    return 0;
}
