#pragma once

#include "draw.h"
#include "sysinfo.h"
#include "gpu.h"

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
                   const std::vector<HwmonChip>& hwmon,
                   const std::vector<GpuInfo>&   gpus);

void panel_gpu    (ncplane* n, int y, int x, int h, int w,
                   const std::vector<GpuInfo>& gpus);

// Settings overlay (Esc): theme + background picker, drawn centered on top
// of the normal panel grid. rows/cols are the full terminal dimensions.
void panel_settings(ncplane* n, int rows, int cols);
