#pragma once

#include <string>
#include <vector>

enum class GpuVendor { AMD, Intel, Nvidia, Unknown };

// One GPU. Fields default to -1.0 meaning "not available" — the panel renders
// "n/a" rather than a bogus zero, since availability differs a lot by vendor
// and driver (e.g. Intel iGPUs rarely expose VRAM or power via sysfs).
struct GpuInfo {
    GpuVendor   vendor = GpuVendor::Unknown;
    std::string name;

    double util_pct    = -1.0; // 0-100
    double mem_used_mb  = -1.0;
    double mem_total_mb = -1.0;
    double temp_c        = -1.0;
    double power_w        = -1.0;
};

// Detects the system's (one) GPU:
//  - AMD / Intel: pure sysfs (/sys/class/drm/card*/device), no external deps.
//  - NVIDIA: sysfs exposes almost nothing useful under the proprietary driver,
//    so this shells out to `nvidia-smi` instead. If the binary isn't present
//    (no NVIDIA driver installed), this silently contributes zero rows.
// Returns at most one entry — the lowest-numbered /sys/class/drm/cardN,
// which is normally the boot/primary GPU. Still a vector (empty when no
// GPU is found, one element otherwise) purely so callers don't need a
// separate "found or not" branch; multi-GPU machines only ever get their
// first card reported. Never throws; a read failure just omits the card.
std::vector<GpuInfo> parse_gpus();
