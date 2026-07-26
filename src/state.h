#pragma once

#include "common.h"
#include "procfs.h"
#include "gpu.h"
#include "tty.h"

// Gradient type
enum GradType { GRAD_CPU, GRAD_MEM, GRAD_TEMP, GRAD_STORAGE, GRAD_HIST };

// Hwmon sensor data
struct HwmonSensor {
    std::string label;
    double      temp_celsius = 0.0;
    double      temp_max     = 0.0; // temp*_max  (high trip point)
    double      temp_crit    = 0.0; // temp*_crit
    bool        is_package   = false;
    bool        has_max      = false;
    bool        has_crit     = false;
};

struct HwmonChip {
    std::string              name;
    std::vector<HwmonSensor> sensors;
};

// Global application state
struct AppState {
    cpustat                prev_cpu  = {};
    std::vector<cpustat>   prev_core;
    std::deque<double>     cpu_hist; // Rolling CPU% history for sparkline
    std::vector<netdev>    prev_net;
    std::vector<diskstats> prev_disk;
    Clock::time_point      t_prev;
    double                 dt        = 1.0;
    double                 peak_rx   = 0.0;
    double                 peak_tx   = 0.0;
    cpuinfo                ci        = {};
    systemuname            un        = {};
    std::string            local_ip  = "N/A";

    // GPU (parsed once per frame, cached so panels don't re-parse mid-render)
    std::vector<GpuInfo>   gpus;

    // Theme / background — set from ~/.config/vitals/config at startup,
    // live-edited from the Settings overlay (Esc), see theme.h / config.h.
    int  theme_idx = 0;
    int  bg_idx    = 0;   // 0 = transparent (terminal default), 1 = theme's own BASE color

    // Terminal mode — Auto detects a raw Linux VT (see tty.cpp) and swaps
    // every panel to ASCII-only rendering (draw_tty.cpp); can be forced
    // either way from the Settings overlay. tty_active is the *resolved*
    // flag draw.cpp actually checks each frame; tty_force is the person's
    // stored preference (persisted to config).
    TtyForce tty_force  = TtyForce::Auto;
    bool     tty_active = false;

    // Refresh interval — how often the main loop re-reads procfs/sysfs and
    // redraws (default 1000ms). Adjustable from the Settings overlay with
    // Left/Right when the Refresh row is focused, persisted to config.
    int refresh_ms = 1000;

    // Settings overlay state
    bool settings_open        = false;
    int  settings_focus       = 0;   // 0 = theme, 1 = background, 2 = terminal mode, 3 = refresh rate
    int  settings_saved_theme = 0;   // snapshot on open, restored if the person cancels (Esc)
    int  settings_saved_bg    = 0;
    TtyForce settings_saved_tty = TtyForce::Auto;
    int  settings_saved_refresh = 1000;
};

// Defined in main.cpp, referenced throughout.
extern AppState G;
