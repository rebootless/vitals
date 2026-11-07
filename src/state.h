#pragma once

#include "common.h"
#include "procfs.h"
#include "gpu.h"
#include "settings.h"

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

// rpm or pct is -1 when not reported; name is at most FAN_NAME_W characters
struct FanInfo {
    std::string name;
    int         rpm = -1;
    int         pct = -1;
};

struct AppState {
    cpustat                prev_cpu  = {};
    std::vector<cpustat>   prev_core;
    std::vector<int>       core_ids;   // CPU number of each entry in the per-core results
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

    // sampled once per tick: panels draw these, so redraws never touch /proc
    meminfo                mem       = {};
    snmp                   net_snmp  = {};
    uptime                 up        = {};
    loadavg                load      = {};
    filesystemstat         root_fs   = {};
    std::string            local_ip  = "N/A";

    std::vector<GpuInfo>   gpus;
    std::vector<FanInfo>   fans;

    Settings opt;
    Settings opt_saved;
    bool     tty_active = false; // opt.tty_force resolved against the terminal

    PanelFlags panel_avail = {}; // refreshed every tick: which panels have data

    bool frozen               = false;
    bool settings_open        = false;
    int  settings_focus       = F_THEME;
    int  settings_panel_cursor = 0;
    int  settings_theme_top   = 0;   // first visible row of the theme list
    int  settings_right_top   = 0;   // first visible line of the right-hand column
};

extern AppState G;
