#pragma once

#include <string>
#include <vector>

enum class GpuVendor { AMD, Intel, Nvidia, Unknown };

// -1.0 means "not available" (panel renders n/a)
struct GpuInfo {
    GpuVendor   vendor = GpuVendor::Unknown;
    std::string name;

    double util_pct    = -1.0;
    double mem_used_mb  = -1.0;
    double mem_total_mb = -1.0;
    double temp_c        = -1.0;
    double power_w        = -1.0;
};

// at most one entry: the lowest-numbered /sys/class/drm/cardN
std::vector<GpuInfo> parse_gpus();
