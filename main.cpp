#include "render.h"
#include "theme.h"
#include "config.h"
#include "selftest.h"
#include "paths.h"
#include "log.h"
#include "diag.h"

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <utility>

AppState G;

// runs one data source; failures go to the log (once per outage) instead of being swallowed
template <class F>
static bool sample(const char* name, F&& fn) {
    try {
        fn();
        log_source(name, true);
        return true;
    } catch (const std::exception& e) {
        log_source(name, false, e.what());
    } catch (...) {
        log_source(name, false);
    }
    return false;
}

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
    bool self_test = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            printf("Usage: vitals [OPTION]\n\n"
                   "  -h, --help       show this help and exit\n"
                   "  -v, --version    show version and exit\n"
                   "      --self-test  check data sources and exit (0 = no FAIL)\n\n");
            const std::string cfg = config_dir();
            if (cfg.empty()) {
                printf("Settings, log and your themes: unavailable (HOME is not set)\n");
            } else {
                printf("Settings:       %s\n", config_path().c_str());
                printf("Log:            %s/MM-DD-YYYY.log\n", cfg.c_str());
                printf("User themes:    %s/themes\n", cfg.c_str());
            }
            printf("Bundled Themes: %s\n", system_theme_dir().c_str());
            return 0;
        }
        if (a == "-v" || a == "--version") {
            printf("vitals %s\n", VITALS_VERSION);
            return 0;
        }
        if (a == "--self-test") { self_test = true; continue; }
        fprintf(stderr, "vitals: unknown option '%s'\nTry 'vitals --help'.\n", a.c_str());
        return 2;
    }
    if (self_test) return run_self_test();

    setlocale(LC_ALL, "");

    log_open();
    log_startup(argc, argv);

    notcurses_options opts{};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;
    notcurses* nc = notcurses_init(&opts, nullptr);
    if (!nc) {
        LOG_ERROR("terminal", "notcurses_init failed");
        fprintf(stderr, "notcurses_init failed\n");
        log_close();
        return 1;
    }
    log_terminal(nc);

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

    sample("cpuinfo", [] { G.ci = parse_cpuinfo();     });
    sample("uname",   [] { G.un = parse_systemuname(); });

    log_data_sources();

    auto take_baseline = [] {
        sample("cpustat",   [] { G.prev_cpu  = parse_cpustat();   });
        sample("percpu",    [] { G.prev_core = parse_percpu();    });
        sample("netdev",    [] { G.prev_net  = parse_netdev();    });
        sample("diskstats", [] { G.prev_disk = parse_diskstats(); });
        G.t_prev = Clock::now();
    };
    take_baseline();

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

    auto last_tick = Clock::now() - std::chrono::hours(1);
    bool was_paused = false;
    const char* exit_reason = "unknown";
    unsigned long long tick_count = 0;
    const auto started_at = Clock::now();
    LOG_INFO("main", "entering main loop, refresh %d ms", G.refresh_ms);

    for (;;) {
        ncinput ni{};

        // sleep until input or the next tick; frozen and settings only wake on input
        uint32_t ch;
        std::chrono::nanoseconds left{0};
        if (!G.settings_open && !G.frozen)
            left = std::chrono::milliseconds(G.refresh_ms) - (Clock::now() - last_tick);

        if (G.settings_open || G.frozen) {
            ch = notcurses_get(nc, nullptr, &ni);
        } else if (left.count() <= 0) {
            ch = notcurses_get_nblock(nc, &ni);
        } else {
            timespec deadline{};
            clock_gettime(CLOCK_MONOTONIC, &deadline);
            const long long ns = deadline.tv_nsec + left.count();
            deadline.tv_sec  += static_cast<time_t>(ns / 1000000000LL);
            deadline.tv_nsec  = static_cast<long>(ns % 1000000000LL);
            ch = notcurses_get(nc, &deadline, &ni);
        }

        // stdin closed or input failed: nothing more can arrive
        if (ch == NCKEY_EOF || ch == static_cast<uint32_t>(-1)) {
            exit_reason = (ch == NCKEY_EOF) ? "stdin closed" : "input error";
            LOG_ERROR("input", "%s", exit_reason);
            break;
        }

        // Kitty protocol sends RELEASE events too; ignore them or Esc-up re-closes Settings
        if (ni.evtype == NCTYPE_RELEASE) ch = 0;
        if (ch != 0) LOG_DEBUG("input", "key %u", static_cast<unsigned>(ch));

        bool settings_was_open = G.settings_open;

        if (!G.settings_open) {
            if (ch == 'q' || ch == 'Q') { exit_reason = "quit key"; break; }

            if (ch == 'f' || ch == 'F') {
                G.frozen = !G.frozen;
                LOG_INFO("ui", "freeze %s", G.frozen ? "on" : "off");
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
                LOG_INFO("ui", "settings opened");
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
                LOG_INFO("ui", "settings discarded");

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
                LOG_INFO("ui", "settings saved: theme=%s background=%s terminal=%s refresh=%d corners=%s graph=%s mirroring=%d",
                         cfg.theme_name.c_str(), cfg.bg_mode.c_str(), cfg.tty_mode.c_str(), cfg.refresh_ms,
                         cfg.corners.c_str(), cfg.graph_style.c_str(), cfg.symbol_mirroring ? 1 : 0);
            }
        }

        // rates must not span a pause: start from a fresh baseline and tick within a second
        const bool paused = G.settings_open || G.frozen;
        if (was_paused && !paused) {
            take_baseline();
            sample("gpu", [] { G.gpus = parse_gpus(); });
            LOG_DEBUG("tick", "resumed after a pause, baseline refreshed");
            const auto wait = std::min(G.refresh_ms, 1000);
            last_tick = G.t_prev - std::chrono::milliseconds(G.refresh_ms - wait);
        }
        was_paused = paused;

        const auto t_now = Clock::now();
        bool due_for_tick = !G.settings_open && !G.frozen &&
            (t_now - last_tick >= std::chrono::milliseconds(G.refresh_ms));

        if (due_for_tick) {
            const auto tick_start = Clock::now();
            ++tick_count;
            G.dt = std::chrono::duration<double>(t_now - G.t_prev).count();
            if (G.dt < 0.001) G.dt = 1.0;

            cpustat                cur_cpu{};
            std::vector<cpustat>   cur_core;
            std::vector<netdev>    cur_net;
            std::vector<diskstats> cur_disk;
            std::vector<thermal>   therm;
            std::vector<cpufreq>   freqs;
            std::vector<HwmonChip> hwmon;

            if (!sample("cpustat", [&] { cur_cpu = parse_cpustat(); })) cur_cpu = G.prev_cpu;
            sample("percpu",    [&] { cur_core = parse_percpu();    });
            sample("netdev",    [&] { cur_net  = parse_netdev();    });
            sample("diskstats", [&] { cur_disk = parse_diskstats(); });
            sample("thermal",   [&] { therm    = parse_thermal();   });
            sample("cpufreq",   [&] { freqs    = parse_cpufreq();   });
            sample("hwmon",     [&] { hwmon    = parse_hwmon();     });
            sample("gpu",       [&] { G.gpus   = parse_gpus();      });
            sample("meminfo",   [&] { G.mem      = parse_meminfo();   });
            sample("snmp",      [&] { G.net_snmp = parse_snmp();      });
            sample("uptime",    [&] { G.up       = parse_uptime();    });
            sample("loadavg",   [&] { G.load     = parse_loadavg();   });
            if (!sample("statvfs", [&] { G.root_fs = parse_filesystemstat("/"); })) G.root_fs = {};

            const std::string ip = get_local_ip();
            if (ip != G.local_ip) LOG_INFO("network", "local address: %s -> %s", G.local_ip.c_str(), ip.c_str());
            G.local_ip = ip;

            const double pct = cpu_delta(G.prev_cpu, cur_cpu);
            G.cpu_hist.push_back(pct);
            if (static_cast<int>(G.cpu_hist.size()) > HIST_CAP)
                G.cpu_hist.pop_front();

            // match cores by CPU number: hotplug shifts positions
            std::map<int, const cpustat*> prev_by_cpu;
            for (const auto& p : G.prev_core) prev_by_cpu[p.cpu] = &p;

            std::vector<double> core_pcts;
            std::vector<int>    core_ids;
            core_pcts.reserve(cur_core.size());
            core_ids.reserve(cur_core.size());
            for (const auto& c : cur_core) {
                auto it = prev_by_cpu.find(c.cpu);
                core_pcts.push_back(it != prev_by_cpu.end() ? cpu_delta(*it->second, c) : 0.0);
                core_ids.push_back(c.cpu);
            }
            G.core_ids = std::move(core_ids);

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
                // whole disks only: partitions count in their parent
                const std::string& d = ds.device;
                if (is_virtual_disk(d) || !parent_of(d).empty()) continue;
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

            const double took_ms =
                std::chrono::duration<double, std::milli>(Clock::now() - tick_start).count();
            LOG_DEBUG("tick", "#%llu dt=%.3f took=%.1f ms cpu=%.1f%% mem=%.1f%% net rx=%.0f tx=%.0f B/s "
                              "disk rd=%.0f wr=%.0f B/s zones=%zu hwmon=%zu gpus=%zu",
                      tick_count, G.dt, took_ms, pct, mem_used(G.mem).pct, rx_now, tx_now,
                      disk_rd_now, disk_wr_now, last_therm.size(), last_hwmon.size(), G.gpus.size());
            if (took_ms > 250.0 || took_ms > G.refresh_ms / 2.0)
                LOG_WARN("tick", "slow tick #%llu: %.1f ms (refresh %d ms)", tick_count, took_ms, G.refresh_ms);
        }

        if (due_for_tick || settings_was_open || G.settings_open || ch != 0) {
            render(nc, n, last_cpu, last_pct, last_net, last_disk,
                  last_therm, last_freqs, last_core_pcts, last_hwmon, G.gpus,
                  last_net_rx, last_net_tx, last_disk_rd, last_disk_wr);

            if (ch == NCKEY_RESIZE) {
                unsigned rows = 0, cols = 0;
                notcurses_stddim_yx(nc, &rows, &cols);
                LOG_INFO("terminal", "resized to %ux%u", cols, rows);
            }
        }
    }

    LOG_INFO("main", "exiting: %s, %llu ticks in %.0f s", exit_reason, tick_count,
             std::chrono::duration<double>(Clock::now() - started_at).count());

    notcurses_stop(nc);
    log_close();
    return 0;
}
