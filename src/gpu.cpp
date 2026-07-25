#include "gpu.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sstream>
#include <unistd.h>

namespace {

std::string read_line(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::string s;
    std::getline(f, s);
    return s;
}

// Returns fallback if the file is missing, empty, or not a number.
double read_double(const std::string& path, double fallback = -1.0) {
    std::ifstream f(path);
    if (!f) return fallback;
    double v = 0.0;
    f >> v;
    return f.fail() ? fallback : v;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Basename of a symlink's target, e.g. device_dir/driver -> ".../nouveau" -> "nouveau".
std::string read_symlink_basename(const std::string& path) {
    char buf[512];
    ssize_t n = readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    std::string s(buf);
    size_t pos = s.find_last_of('/');
    return pos == std::string::npos ? s : s.substr(pos + 1);
}

// Finds the first hwmonN subdirectory under <device_dir>/hwmon and reads
// <field>_input (falling back to <field>_average if provided and present).
double read_hwmon_field(const std::string& device_dir, const std::string& field,
                        const std::string& fallback_field = "") {
    const std::string hwmon_base = device_dir + "/hwmon";
    DIR* d = opendir(hwmon_base.c_str());
    if (!d) return -1.0;

    double result = -1.0;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name.rfind("hwmon", 0) != 0) continue;

        double v = read_double(hwmon_base + "/" + name + "/" + field);
        if (v < 0 && !fallback_field.empty())
            v = read_double(hwmon_base + "/" + name + "/" + fallback_field);
        if (v >= 0) { result = v; break; }
    }
    closedir(d);
    return result;
}

GpuInfo parse_amd(const std::string& device_dir, const std::string& card) {
    GpuInfo g;
    g.vendor = GpuVendor::AMD;
    g.name   = "AMD GPU (" + card + ")";

    g.util_pct = read_double(device_dir + "/gpu_busy_percent");

    double used  = read_double(device_dir + "/mem_info_vram_used");
    double total = read_double(device_dir + "/mem_info_vram_total");
    if (used >= 0 && total > 0) {
        g.mem_used_mb  = used  / (1024.0 * 1024.0);
        g.mem_total_mb = total / (1024.0 * 1024.0);
    }

    double temp_m = read_hwmon_field(device_dir, "temp1_input");
    if (temp_m >= 0) g.temp_c = temp_m / 1000.0; // millidegrees -> C

    double power_u = read_hwmon_field(device_dir, "power1_average", "power1_input");
    if (power_u >= 0) g.power_w = power_u / 1000000.0; // microwatts -> W

    return g;
}

GpuInfo parse_intel(const std::string& device_dir, const std::string& card) {
    GpuInfo g;
    g.vendor = GpuVendor::Intel;
    g.name   = "Intel GPU (" + card + ")";

    // gpu_busy_percent exists on newer i915/xe kernels; absent on older ones.
    g.util_pct = read_double(device_dir + "/gpu_busy_percent");

    double temp_m = read_hwmon_field(device_dir, "temp1_input");
    if (temp_m >= 0) g.temp_c = temp_m / 1000.0;

    double power_u = read_hwmon_field(device_dir, "power1_average", "power1_input");
    if (power_u >= 0) g.power_w = power_u / 1000000.0;

    // Intel iGPUs share system RAM — sysfs has no generic "VRAM used/total"
    // node, so mem_used_mb/mem_total_mb are intentionally left at -1 (n/a).
    return g;
}

// ---- nouveau (open-source NVIDIA driver) ----
//
// nouveau does not implement gpu_busy_percent in sysfs the way amdgpu/i915
// do, so there's no single file to poll. Instead this uses the kernel's
// generic DRM client usage-stats API exposed via /proc/<pid>/fdinfo/<fd>
// (present since ~5.19, and only actually populated by drivers that call
// drm_show_fdinfo() — nouveau added this for its "gr" engine in later
// kernels; older kernels will simply yield no fdinfo hits and this falls
// back to n/a, same as if the feature were entirely absent).
//
// Approach: sum every process's cumulative "drm-engine-gr" busy time (ns)
// for file descriptors pointing at this GPU's PCI address, and turn two
// samples a second-or-so apart into a percentage — the same technique
// nvtop/btop use for nouveau.

struct FdinfoSample {
    unsigned long long busy_ns = 0;
    std::chrono::steady_clock::time_point t{};
    bool valid = false;
};

std::map<std::string, FdinfoSample>& fdinfo_cache() {
    static std::map<std::string, FdinfoSample> cache;
    return cache;
}

double read_engine_busy_percent(const std::string& pci_addr) {
    if (pci_addr.empty()) return -1.0;

    unsigned long long total_busy = 0;
    bool found = false;

    DIR* proc = opendir("/proc");
    if (!proc) return -1.0;

    struct dirent* pe;
    while ((pe = readdir(proc)) != nullptr) {
        std::string pidname = pe->d_name;
        if (pidname.empty() || !std::isdigit(static_cast<unsigned char>(pidname[0])))
            continue;

        std::string fdinfo_dir = "/proc/" + pidname + "/fdinfo";
        DIR* fdd = opendir(fdinfo_dir.c_str());
        if (!fdd) continue;

        struct dirent* fe;
        while ((fe = readdir(fdd)) != nullptr) {
            std::string fname = fe->d_name;
            if (fname == "." || fname == "..") continue;

            std::ifstream f(fdinfo_dir + "/" + fname);
            if (!f) continue;

            std::string line, pdev;
            unsigned long long busy = 0;
            bool has_engine = false;

            while (std::getline(f, line)) {
                if (line.rfind("drm-pdev:", 0) == 0) {
                    pdev = trim(line.substr(9));
                } else if (line.rfind("drm-engine-gr:", 0) == 0) {
                    std::istringstream ls(line.substr(14));
                    unsigned long long v = 0;
                    ls >> v; // value is "<ns> ns" — trailing unit is ignored
                    busy = v;
                    has_engine = true;
                }
            }

            if (has_engine && pdev == pci_addr) {
                total_busy += busy;
                found = true;
            }
        }
        closedir(fdd);
    }
    closedir(proc);

    if (!found) return -1.0;

    auto& sample = fdinfo_cache()[pci_addr];
    auto  now    = std::chrono::steady_clock::now();
    double result = -1.0; // first sample for this GPU: no delta yet

    if (sample.valid && total_busy >= sample.busy_ns) {
        double dt_ns = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - sample.t).count());
        if (dt_ns > 0) {
            double busy_delta = static_cast<double>(total_busy - sample.busy_ns);
            result = std::min(100.0, std::max(0.0, (busy_delta / dt_ns) * 100.0));
        }
    }

    sample.busy_ns = total_busy;
    sample.t       = now;
    sample.valid   = true;
    return result;
}

GpuInfo parse_nouveau(const std::string& device_dir, const std::string& card) {
    GpuInfo g;
    g.vendor = GpuVendor::Nvidia;
    g.name   = "NVIDIA GPU (nouveau, " + card + ")";

    // device_dir is a symlink to .../<pci-address>; its basename is the
    // "drm-pdev" value the kernel writes into /proc/*/fdinfo.
    std::string pci_addr = read_symlink_basename(device_dir);
    g.util_pct = read_engine_busy_percent(pci_addr);

    double temp_m = read_hwmon_field(device_dir, "temp1_input");
    if (temp_m >= 0) g.temp_c = temp_m / 1000.0;

    // Nouveau exposes no generic VRAM used/total sysfs node (unlike amdgpu) —
    // mem_used_mb/mem_total_mb stay at -1 (n/a).
    return g;
}

// ---- NVIDIA proprietary driver ----
//
// Parses `nvidia-smi --query-gpu=... --format=csv,noheader,nounits` output.
// One CSV line per GPU: name, util%, mem_used(MiB), mem_total(MiB), temp(C), power(W)
//
// Note: on some older/entry-level cards (e.g. Kepler-generation GeForce
// GT/GTX parts), NVML genuinely does not report utilization.gpu — nvidia-smi
// itself prints "[N/A]" for that field regardless of the query used. That's
// a driver/firmware limitation on those chips, not something fixable here.
std::vector<GpuInfo> parse_nvidia_proprietary() {
    std::vector<GpuInfo> result;

    FILE* pipe = popen(
        "nvidia-smi --query-gpu=name,utilization.gpu,memory.used,memory.total,"
        "temperature.gpu,power.draw --format=csv,noheader,nounits 2>/dev/null",
        "r");
    if (!pipe) return result;

    std::array<char, 512> buf{};
    std::string output;
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        output += buf.data();
    pclose(pipe);

    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (trim(line).empty()) continue;

        std::istringstream ls(line);
        std::string field;
        std::vector<std::string> fields;
        while (std::getline(ls, field, ',')) fields.push_back(trim(field));
        if (fields.size() < 6) continue;

        GpuInfo g;
        g.vendor = GpuVendor::Nvidia;
        g.name   = fields[0];
        try { g.util_pct     = std::stod(fields[1]); } catch (...) {}
        try { g.mem_used_mb  = std::stod(fields[2]); } catch (...) {}
        try { g.mem_total_mb = std::stod(fields[3]); } catch (...) {}
        try { g.temp_c       = std::stod(fields[4]); } catch (...) {}
        try { g.power_w      = std::stod(fields[5]); } catch (...) {}
        result.push_back(g);
    }
    return result;
}

} // namespace

std::vector<GpuInfo> parse_gpus() {
    std::vector<GpuInfo> result;
    bool have_proprietary_nvidia = false;

    const char* base = "/sys/class/drm";
    DIR* dir = opendir(base);
    bool sysfs_enum_failed = (dir == nullptr);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            // Match "cardN" only (skip "cardN-<connector>", "renderD*", ".", "..").
            if (name.size() < 5 || name.substr(0, 4) != "card") continue;
            if (!std::isdigit(static_cast<unsigned char>(name[4]))) continue;
            if (name.find('-') != std::string::npos) continue;

            const std::string device_dir = std::string(base) + "/" + name + "/device";
            const std::string vendor_id  = read_line(device_dir + "/vendor");

            if (vendor_id == "0x1002") {
                result.push_back(parse_amd(device_dir, name));

            } else if (vendor_id == "0x8086") {
                result.push_back(parse_intel(device_dir, name));

            } else if (vendor_id == "0x10de") {
                // Same hardware, two possible drivers — dispatch on which
                // one is actually bound, since they need entirely different
                // data sources (nouveau: fdinfo; proprietary: nvidia-smi).
                //
                // This fails OPEN: only an explicit "nouveau" match takes
                // the fdinfo path. Anything else — proprietary "nvidia",
                // a differently-named module, or the driver symlink being
                // unreadable/absent on some kernel/distro combo — falls
                // back to trying nvidia-smi rather than silently dropping
                // the card. nvidia-smi is a no-op (empty output) if there's
                // genuinely nothing for it to query, so this is safe.
                std::string driver = read_symlink_basename(device_dir + "/driver");
                if (driver == "nouveau") {
                    result.push_back(parse_nouveau(device_dir, name));
                } else {
                    have_proprietary_nvidia = true;
                }
            }
        }
        closedir(dir);
    }

    // Only spawn nvidia-smi if a proprietary-driven card was actually seen,
    // or sysfs enumeration itself failed (unusual, but better to try than
    // to silently show nothing) — avoids a pointless subprocess on
    // nouveau-only / non-NVIDIA machines.
    if (have_proprietary_nvidia || sysfs_enum_failed) {
        auto nv = parse_nvidia_proprietary();
        result.insert(result.end(), nv.begin(), nv.end());
    }

    return result;
}
