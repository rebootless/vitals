#pragma once

#include "common.h"
#include "procfs.h"
#include "gpu.h"
#include "tty.h"

enum GradType { GRAD_CPU, GRAD_MEM, GRAD_TEMP, GRAD_STORAGE, GRAD_HIST };

struct HwmonSensor {
    std::string label;
    double      temp_celsius = 0.0;
    double      temp_max     = 0.0;
    double      temp_crit    = 0.0;
    bool        is_package   = false;
    bool        has_max      = false;
    bool        has_crit     = false;
};

enum class HwmonCategory {
    CPU, Motherboard, Storage, Network, Laptop, Other, Unknown
};

struct HwmonChip {
    std::string              name;
    std::string              display;
    HwmonCategory             category = HwmonCategory::Unknown;
    std::vector<HwmonSensor> sensors;
};

struct AppState {
    cpustat                prev_cpu  = {};
    std::vector<cpustat>   prev_core;
    std::deque<double>     cpu_hist;

    std::deque<double>     net_rx_hist, net_tx_hist;
    std::deque<double>     disk_rd_hist, disk_wr_hist;

    std::vector<netdev>    prev_net;
    std::vector<diskstats> prev_disk;
    Clock::time_point      t_prev;
    double                 dt        = 1.0;
    double                 peak_rx   = 0.0;
    double                 peak_tx   = 0.0;
    cpuinfo                ci        = {};
    systemuname            un        = {};
    std::string            local_ip  = "N/A";

    std::vector<GpuInfo>   gpus;

    int  theme_idx = 0;
    int  bg_idx    = 0;   // 0 = transparent (terminal default), 1 = theme's own BG color

    TtyForce tty_force  = TtyForce::Auto;
    bool     tty_active = false;

    int refresh_ms = 1000;

    int corners_idx = 0;

    int graph_style_idx = 0;

    int symbol_mirror_idx = 0;

    bool frozen               = false;
    bool settings_open        = false;
    int  settings_focus       = 0;   // 0=theme 1=bg 2=terminal 3=corners 4=graph style 5=mirroring 6=refresh
    int  settings_saved_theme = 0;
    int  settings_saved_bg    = 0;
    TtyForce settings_saved_tty = TtyForce::Auto;
    int  settings_saved_refresh = 1000;
    int  settings_saved_corners = 0;
    int  settings_saved_graph_style = 0;
    int  settings_saved_symbol_mirror = 0;
};

extern AppState G;
