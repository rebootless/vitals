#include "render.h"
#include "theme.h"
#include "config.h"
#include "selftest.h"
#include "paths.h"
#include "log.h"
#include "diag.h"
#include "console.h"
#include "input.h"
#include "report.h"
#include "tick.h"

#include <algorithm>
#include <cerrno>
#include <clocale>
#include <cstdio>
#include <ctime>
#include <thread>
#include <chrono>
#include <unistd.h>

AppState G;

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
                printf("Log:            %s/MM-DD-YYYY_HH-MM-SS.log\n", cfg.c_str());
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
    log_set_max_mb(G.opt.log_mb);
    log_set_keep(G.opt.log_keep);
    G.tty_active = resolve_tty_active(G.opt.tty_force);

    report_config();

    con_stage("first-sample");
    sample("cpuinfo", [] { G.ci = parse_cpuinfo();     });
    sample("uname",   [] { G.un = parse_systemuname(); });

    log_data_sources();

    take_baseline();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto last_tick = Clock::now() - std::chrono::hours(1);
    bool was_paused = false;
    const char* exit_reason = "unknown";
    unsigned long long tick_count = 0;
    const auto started_at = Clock::now();
    LOG_INFO("main", "entering main loop, refresh %d ms", G.opt.refresh_ms);

    for (;;) {
        ncinput ni{};

        // sleep until input, the next data tick or the UI tick, whichever comes first
        uint32_t ch;
        std::chrono::nanoseconds left = std::chrono::milliseconds(UI_TICK_MS);
        if (!G.settings_open && !G.frozen)
            left = std::min<std::chrono::nanoseconds>(
                left, std::chrono::milliseconds(G.opt.refresh_ms) - (Clock::now() - last_tick));

        if (left.count() <= 0) {
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
            ++tick_count;
            tick(t_now, tick_count);
            last_tick = t_now;

            if (tick_count == 1) {
                report_data();
                con_say("[6/6] starting, refresh %d ms", G.opt.refresh_ms);
                con_stage("main-loop");
            }
        }

        // the plane must match the terminal before the layout is computed from it
        const bool resized = ch == NCKEY_RESIZE || terminal_resized(n);
        if (resized) {
            unsigned rows = 0, cols = 0;
            notcurses_refresh(nc, &rows, &cols);
            LOG_INFO("terminal", "resized to %ux%u", cols, rows);
        }

        if (due_for_tick || resized || ch != 0)
            render(nc, n);
    }

    LOG_INFO("main", "exiting: %s, %llu ticks in %.0f s", exit_reason, tick_count,
             std::chrono::duration<double>(Clock::now() - started_at).count());

    notcurses_stop(nc);
    con_release();
    con_say("exited: %s", exit_reason);
    log_close();
    return 0;
}
