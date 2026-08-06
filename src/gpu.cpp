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

std::string read_symlink_basename(const std::string& path) {
    char buf[512];
    ssize_t n = readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    std::string s(buf);
    size_t pos = s.find_last_of('/');
    return pos == std::string::npos ? s : s.substr(pos + 1);
}

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
    if (temp_m >= 0) g.temp_c = temp_m / 1000.0;

    double power_u = read_hwmon_field(device_dir, "power1_average", "power1_input");
    if (power_u >= 0) g.power_w = power_u / 1000000.0;

    return g;
}

GpuInfo parse_intel(const std::string& device_dir, const std::string& card) {
    GpuInfo g;
    g.vendor = GpuVendor::Intel;
    g.name   = "Intel GPU (" + card + ")";

    g.util_pct = read_double(device_dir + "/gpu_busy_percent");

    double temp_m = read_hwmon_field(device_dir, "temp1_input");
    if (temp_m >= 0) g.temp_c = temp_m / 1000.0;

    double power_u = read_hwmon_field(device_dir, "power1_average", "power1_input");
    if (power_u >= 0) g.power_w = power_u / 1000000.0;

    // Intel iGPUs have no sysfs VRAM node: mem_*_mb stay -1
    return g;
}

// nouveau has no gpu_busy_percent: sum drm-engine-gr from /proc/*/fdinfo over two samples (as nvtop/btop do)

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
    double result = -1.0;

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

    // basename of the device_dir symlink is the PCI address that fdinfo reports as "drm-pdev"
    std::string pci_addr = read_symlink_basename(device_dir);
    g.util_pct = read_engine_busy_percent(pci_addr);

    double temp_m = read_hwmon_field(device_dir, "temp1_input");
    if (temp_m >= 0) g.temp_c = temp_m / 1000.0;

    return g;
}

// nvidia-smi parse; some old cards (e.g. Kepler) report [N/A] utilization in NVML itself
std::vector<GpuInfo> parse_nvidia_proprietary() {
    std::vector<GpuInfo> result;

    FILE* pipe = popen(
        "nvidia-smi -i 0 --query-gpu=name,utilization.gpu,memory.used,memory.total,"
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

}

std::vector<GpuInfo> parse_gpus() {
    std::vector<GpuInfo> result;

    const char* base = "/sys/class/drm";
    DIR* dir = opendir(base);
    bool sysfs_enum_failed = (dir == nullptr);

    // sort cardN: readdir order is unspecified, and card0 is normally the primary GPU
    struct Candidate { int index; std::string name, device_dir, vendor_id; };
    std::vector<Candidate> candidates;

    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.size() < 5 || name.substr(0, 4) != "card") continue;
            if (!std::isdigit(static_cast<unsigned char>(name[4]))) continue;
            if (name.find('-') != std::string::npos) continue;

            int idx = 0;
            try { idx = std::stoi(name.substr(4)); } catch (...) { continue; }

            const std::string device_dir = std::string(base) + "/" + name + "/device";
            candidates.push_back({idx, name, device_dir, read_line(device_dir + "/vendor")});
        }
        closedir(dir);
    }
    std::sort(candidates.begin(), candidates.end(),
             [](const Candidate& a, const Candidate& b) { return a.index < b.index; });

    for (const auto& c : candidates) {
        if (c.vendor_id == "0x1002") {
            result.push_back(parse_amd(c.device_dir, c.name));
            break;

        } else if (c.vendor_id == "0x8086") {
            result.push_back(parse_intel(c.device_dir, c.name));
            break;

        } else if (c.vendor_id == "0x10de") {
            // fails open: only an explicit "nouveau" skips nvidia-smi
            std::string driver = read_symlink_basename(c.device_dir + "/driver");
            if (driver == "nouveau") {
                result.push_back(parse_nouveau(c.device_dir, c.name));
            } else {
                auto nv = parse_nvidia_proprietary();
                if (!nv.empty()) result.push_back(std::move(nv.front()));
            }
            break;
        }
    }

    if (result.empty() && (sysfs_enum_failed || candidates.empty())) {
        auto nv = parse_nvidia_proprietary();
        if (!nv.empty()) result.push_back(std::move(nv.front()));
    }

    return result;
}
