#include "render.h"
#include "theme.h"
#include "config.h"
#include "selftest.h"
#include "paths.h"
#include "log.h"
#include "diag.h"
#include "console.h"
#include "input.h"

#include <cerrno>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <unistd.h>
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

// ---- startup report on stderr (see console.h) ----

static void report_terminal_failure() {
    con_say("error: cannot start the terminal UI (notcurses_init failed)");
    const char* term = getenv("TERM");
    if (!isatty(STDOUT_FILENO))
        con_say("hint: stdout is not a terminal; run vitals directly in a terminal, not through a pipe or redirect");
    else if (!term || !*term)
        con_say("hint: TERM is not set; set it to a real terminal type, e.g. xterm-256color");
    else if (std::strcmp(term, "dumb") == 0)
        con_say("hint: TERM is \"dumb\"; set TERM to a real terminal type, e.g. xterm-256color");
    if (!log_path().empty()) con_say("see %s for details", log_path().c_str());
}

static void report_terminal(notcurses* nc) {
    const char* term = getenv("TERM");
    unsigned rows = 0, cols = 0;
    notcurses_stddim_yx(nc, &rows, &cols);

    const bool utf8 = notcurses_canutf8(nc);
    char colors[32];
    if (notcurses_cantruecolor(nc)) std::snprintf(colors, sizeof(colors), "truecolor");
    else                            std::snprintf(colors, sizeof(colors), "%u colors", notcurses_palette_size(nc));

    con_say("[3/6] terminal: %s, %s, %s, %ux%u", term && *term ? term : "(unset)",
            utf8 ? "UTF-8" : "not UTF-8", colors, cols, rows);
    if (!utf8) {
        const char* loc = std::setlocale(LC_CTYPE, nullptr);
        con_say("warning: locale \"%s\" is not UTF-8, borders and graphs may be drawn incorrectly", loc ? loc : "?");
    }
}

static void report_config() {
    const std::string path = config_path();
    if (path.empty()) { con_say("[4/6] config: unavailable (HOME is not set), using defaults"); return; }
    if (access(path.c_str(), R_OK) == 0) { con_say("[4/6] config: %s", path.c_str()); return; }
    if (errno == ENOENT) { con_say("[4/6] config: %s (not found, using defaults)", path.c_str()); return; }
    con_say("warning: cannot read %s: %s, using defaults", path.c_str(), std::strerror(errno));
    con_say("[4/6] config: %s (unreadable, using defaults)", path.c_str());
}

static void report_data() {
    static const PanelId ORDER[] = { P_CPU, P_MEMORY, P_NETWORK, P_STORAGE, P_GPU, P_THERMAL, P_FAN };
    std::string s;
    for (PanelId id : ORDER) {
        std::string name = PANELS[id].label;
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        s += (s.empty() ? "" : ", ") + name + (G.panel_avail[static_cast<size_t>(id)] ? " ok" : " n/a");
    }
    con_say("[5/6] data: %s", s.c_str());
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

    con_install_crash_handlers();
    con_say("[1/6] arguments ok");

    setlocale(LC_ALL, "");

    con_stage("log");
    log_open();
    log_startup(argc, argv);
    con_say("[2/6] log: %s", log_path().empty() ? "disabled" : log_path().c_str());

    con_stage("terminal");
    notcurses_options opts{};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;
    notcurses* nc = notcurses_init(&opts, nullptr);
    if (!nc) {
        LOG_ERROR("terminal", "notcurses_init failed");
        report_terminal_failure();
        log_close();
        return 1;
    }
    con_hold(nc);
    log_terminal(nc);
    report_terminal(nc);

    ncplane* n = notcurses_stdplane(nc);
    ncplane_set_bg_default(n);

    con_stage("config");
    G.opt        = settings_from_config(load_config());
    G.tty_active = resolve_tty_active(G.opt.tty_force);

    report_config();

    con_stage("first-sample");
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
    LOG_INFO("main", "entering main loop, refresh %d ms", G.opt.refresh_ms);

    for (;;) {
        ncinput ni{};

        // sleep until input or the next tick; frozen and settings only wake on input
        uint32_t ch;
        std::chrono::nanoseconds left{0};
        if (!G.settings_open && !G.frozen)
            left = std::chrono::milliseconds(G.opt.refresh_ms) - (Clock::now() - last_tick);

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

        const bool settings_was_open = G.settings_open;
        if (handle_key(ch)) { exit_reason = "quit key"; break; }

        // rates must not span a pause: start from a fresh baseline and tick within a second
        const bool paused = G.settings_open || G.frozen;
        if (was_paused && !paused) {
            take_baseline();
            sample("gpu", [] { G.gpus = parse_gpus(); });
            LOG_DEBUG("tick", "resumed after a pause, baseline refreshed");
            const auto wait = std::min(G.opt.refresh_ms, 1000);
            last_tick = G.t_prev - std::chrono::milliseconds(G.opt.refresh_ms - wait);
        }
        was_paused = paused;

        const auto t_now = Clock::now();
        bool due_for_tick = !G.settings_open && !G.frozen &&
            (t_now - last_tick >= std::chrono::milliseconds(G.opt.refresh_ms));

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
            sample("fans",      [&] { G.fans   = collect_fans(G.gpus); });
            {   // one line whenever the list changes: what the Fans panel is given
                static std::string last_fans;
                std::string d;
                for (const auto& f : G.fans)
                    d += (d.empty() ? "" : ", ") + f.name + "=" +
                         (f.rpm >= 0 ? std::to_string(f.rpm) + " RPM" : std::to_string(f.pct) + " %");
                if (d != last_fans) { LOG_INFO("fans", "%zu fans: %s", G.fans.size(), d.empty() ? "(none)" : d.c_str()); last_fans = d; }
            }
            sample("meminfo",   [&] { G.mem      = parse_meminfo();   });
            sample("snmp",      [&] { G.net_snmp = parse_snmp();      });
            sample("uptime",    [&] { G.up       = parse_uptime();    });
            sample("loadavg",   [&] { G.load     = parse_loadavg();   });
            if (!sample("statvfs", [&] { G.root_fs = parse_filesystemstat("/"); })) G.root_fs = {};

            G.panel_avail = panel_availability(G.mem, G.gpus, G.root_fs, cur_core, cur_net, cur_disk, therm, hwmon, G.fans);

            if (tick_count == 1) {
                report_data();
                con_say("[6/6] starting, refresh %d ms", G.opt.refresh_ms);
                con_stage("main-loop");
            }

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
            if (took_ms > 250.0 || took_ms > G.opt.refresh_ms / 2.0)
                LOG_WARN("tick", "slow tick #%llu: %.1f ms (refresh %d ms)", tick_count, took_ms, G.opt.refresh_ms);
        }

        if (due_for_tick || settings_was_open || G.settings_open || ch != 0) {
            render(nc, n, last_pct, last_net, last_disk,
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
    con_release();
    con_say("exited: %s", exit_reason);
    log_close();
    return 0;
}
