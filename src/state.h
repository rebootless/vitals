#pragma once

#include "common.h"
#include "procfs.h"

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

struct HwmonChip {
    std::string              name;
    std::vector<HwmonSensor> sensors;
};

struct AppState {
    cpustat                prev_cpu  = {};
    std::vector<cpustat>   prev_core;
    std::deque<double>     cpu_hist;
    std::vector<netdev>    prev_net;
    std::vector<diskstats> prev_disk;
    Clock::time_point      t_prev;
    double                 dt        = 1.0;
    double                 peak_rx   = 0.0;
    double                 peak_tx   = 0.0;
    cpuinfo                ci        = {};
    systemuname            un        = {};
    std::string            local_ip  = "N/A";
};

extern AppState G;
