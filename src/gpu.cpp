#include "gpu.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <sstream>

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

std::vector<GpuInfo> parse_nvidia() {
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

}

std::vector<GpuInfo> parse_gpus() {
    std::vector<GpuInfo> result;

    const char* base = "/sys/class/drm";
    DIR* dir = opendir(base);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.size() < 5 || name.substr(0, 4) != "card") continue;
            if (!std::isdigit(static_cast<unsigned char>(name[4]))) continue;
            if (name.find('-') != std::string::npos) continue;

            const std::string device_dir = std::string(base) + "/" + name + "/device";
            const std::string vendor_id  = read_line(device_dir + "/vendor");

            if (vendor_id == "0x1002") {
                result.push_back(parse_amd(device_dir, name));
            } else if (vendor_id == "0x8086") {
                result.push_back(parse_intel(device_dir, name));
            }
        }
        closedir(dir);
    }

    auto nv = parse_nvidia();
    result.insert(result.end(), nv.begin(), nv.end());

    return result;
}
