#pragma once

#include "draw.h"
#include "sysinfo.h"

void panel_cpu    (ncplane* n, int y, int x, int h, int w,
                   const cpustat&              cur,
                   double                      pct,
                   const std::vector<cpufreq>& freqs,
                   const std::vector<double>&  core_pcts);

void panel_memory (ncplane* n, int y, int x, int h, int w);

void panel_network(ncplane* n, int y, int x, int h, int w,
                   const std::vector<netdev>& cur_net);

void panel_storage(ncplane* n, int y, int x, int h, int w,
                   const std::vector<diskstats>& cur_disk);

void panel_thermal(ncplane* n, int y, int x, int h, int w,
                   const std::vector<thermal>&   zones,
                   const std::vector<HwmonChip>& hwmon);
