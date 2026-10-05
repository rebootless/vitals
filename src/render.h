#pragma once

#include "panels.h"

void render(notcurses* nc, ncplane* n,
            const cpustat&                cur_cpu,
            double                        cpu_pct,
            const std::vector<netdev>&    cur_net,
            const std::vector<diskstats>& cur_disk,
            const std::vector<thermal>&   therm,
            const std::vector<cpufreq>&   freqs,
            const std::vector<double>&    core_pcts,
            const std::vector<HwmonChip>& hwmon,
            const std::vector<GpuInfo>&   gpus,
            const std::map<std::string, double>& net_rx_rate,
            const std::map<std::string, double>& net_tx_rate,
            const std::map<std::string, double>& disk_rd_rate,
            const std::map<std::string, double>& disk_wr_rate);
