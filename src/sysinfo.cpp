#include "sysinfo.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cctype>

// Per-core CPU stats from /proc/stat
std::vector<cpustat> parse_percpu() {
    std::ifstream f("/proc/stat");
    if (!f) return {};
    std::vector<cpustat> result;
    std::string line;
    while (std::getline(f, line)) {
        if (line.size() < 4) continue;
        if (line[0]!='c' || line[1]!='p' || line[2]!='u') continue;
        if (!std::isdigit(static_cast<unsigned char>(line[3]))) continue;
        size_t sp = line.find(' ');
        if (sp == std::string::npos) continue;
        std::istringstream ss(line.substr(sp + 1));
        cpustat s{};
        ss >> s.user >> s.nice >> s.system >> s.idle
           >> s.iowait >> s.irq >> s.softirq >> s.steal
           >> s.guest  >> s.guest_nice;
        result.push_back(s);
    }
    return result;
}

// CPU utilisation delta (0..100)
double cpu_delta(const cpustat& prev, const cpustat& cur) {
    auto total = [](const cpustat& s) -> ull {
        return s.user + s.nice + s.system + s.idle + s.iowait
             + s.irq  + s.softirq + s.steal;
    };
    auto idle = [](const cpustat& s) -> ull { return s.idle + s.iowait; };

    ull dt = total(cur) - total(prev);
    ull di = idle(cur)  - idle(prev);
    if (!dt) return 0.0;
    return 100.0 * (1.0 - static_cast<double>(di) / static_cast<double>(dt));
}

// Local IP (first non-loopback IPv4 interface)
std::string get_local_ip() {
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return "N/A";
    std::string result = "N/A";
    for (struct ifaddrs* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        if (p->ifa_name == std::string("lo")) continue;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &reinterpret_cast<struct sockaddr_in*>(p->ifa_addr)->sin_addr,
                  buf, sizeof(buf));
        result = buf;
        break;
    }
    freeifaddrs(ifa);
    return result;
}

// Thermal zone type string
std::string thermal_zone_type(int zone) {
    std::ifstream f("/sys/class/thermal/thermal_zone" + std::to_string(zone) + "/type");
    if (!f) return "Zone " + std::to_string(zone);
    std::string t; std::getline(f, t);
    return t.empty() ? "Zone " + std::to_string(zone) : t;
}

// Thermal trip-point limits
std::pair<double,double> thermal_trip_limits(int zone) {
    auto read_mc = [](const std::string& p) -> double {
        std::ifstream f(p); if (!f) return 0.0;
        unsigned long long v = 0; f >> v; return v / 1000.0;
    };
    std::string base = "/sys/class/thermal/thermal_zone" + std::to_string(zone);
    double hi = 0, crit = 0;
    for (int i = 0; i <= 6; i++) {
        std::ifstream tf(base + "/trip_point_" + std::to_string(i) + "_type");
        if (!tf) continue;
        std::string tp; tf >> tp;
        double temp = read_mc(base + "/trip_point_" + std::to_string(i) + "_temp");
        if (tp == "hot")      hi   = temp;
        if (tp == "critical") crit = temp;
    }
    if (hi == 0 && crit == 0) { hi = 74; crit = 94; }
    else if (hi   == 0) hi   = crit - 20;
    else if (crit == 0) crit = hi   + 20;
    return {hi, crit};
}

// Identifies the real-world device behind a hwmon chip name, driving both
// the friendly group header shown in the Thermal panel and whether an
// unlabeled sensor (bare "tempN", no temp*_label file) is shown at all —
// unidentifiable unlabeled sensors are dropped rather than shown as
// meaningless "temp1" rows. amdgpu/nouveau/nvidia are intentionally
// excluded here: those chips are already surfaced through parse_gpus()
// (gpu.cpp reads their hwmon node directly for GPU temperature), so
// picking them up again in this generic scan would duplicate the GPU
// panel's own reading under a second, unlabeled "temp1" entry.
static bool classify_hwmon_chip(const std::string& name,
                                HwmonCategory& cat, std::string& display) {
    static const std::map<std::string, std::pair<HwmonCategory, const char*>> table = {
        {"coretemp",  {HwmonCategory::CPU,         "CPU"}},
        {"k10temp",   {HwmonCategory::CPU,         "CPU"}},
        {"zenpower",  {HwmonCategory::CPU,         "CPU"}},
        {"acpitz",    {HwmonCategory::Motherboard, "Motherboard"}},
        {"nct6775",   {HwmonCategory::Motherboard, "Motherboard"}},
        {"nct6776",   {HwmonCategory::Motherboard, "Motherboard"}},
        {"nct6779",   {HwmonCategory::Motherboard, "Motherboard"}},
        {"nct6791",   {HwmonCategory::Motherboard, "Motherboard"}},
        {"nct6792",   {HwmonCategory::Motherboard, "Motherboard"}},
        {"nct6793",   {HwmonCategory::Motherboard, "Motherboard"}},
        {"nct6795",   {HwmonCategory::Motherboard, "Motherboard"}},
        {"nct6796",   {HwmonCategory::Motherboard, "Motherboard"}},
        {"it8792",    {HwmonCategory::Motherboard, "Motherboard"}},
        {"it8686",    {HwmonCategory::Motherboard, "Motherboard"}},
        {"w83627ehf", {HwmonCategory::Motherboard, "Motherboard"}},
        {"nvme",      {HwmonCategory::Storage,     "SSD"}},
        {"drivetemp", {HwmonCategory::Storage,     "Storage"}},
        {"iwlwifi",   {HwmonCategory::Network,     "Wi-Fi"}},
        {"thinkpad",  {HwmonCategory::Laptop,      "Laptop"}},
    };
    auto it = table.find(name);
    if (it == table.end()) return false;
    cat     = it->second.first;
    display = it->second.second;
    return true;
}

// Hwmon reader (/sys/class/hwmon/hwmonN/)
std::vector<HwmonChip> parse_hwmon() {
    std::map<std::string, HwmonChip> chips_map;

    DIR* base = opendir("/sys/class/hwmon");
    if (!base) return {};

    auto read_mc = [](const std::string& p) -> std::pair<double, bool> {
        std::ifstream f(p);
        if (!f) return {0.0, false};
        long long v = 0; f >> v;
        if (!f) return {0.0, false};
        return {v / 1000.0, true};
    };

    struct dirent* de;
    while ((de = readdir(base)) != nullptr) {
        std::string dname = de->d_name;
        if (dname == "." || dname == "..") continue;
        if (dname.size() < 5 || dname.substr(0, 5) != "hwmon") continue;

        std::string dir = "/sys/class/hwmon/" + dname;

        std::string chip_name;
        { std::ifstream nf(dir + "/name"); if (!nf) continue; std::getline(nf, chip_name); }
        if (chip_name.empty()) continue;

        // GPUs are read separately (gpu.cpp / parse_gpus()) and shown in
        // their own "GPU" section — skip here to avoid a duplicate entry.
        if (chip_name == "amdgpu" || chip_name == "nouveau" || chip_name == "nvidia")
            continue;

        HwmonChip& chip = chips_map[chip_name];
        chip.name = chip_name;
        classify_hwmon_chip(chip_name, chip.category, chip.display);
        if (chip.display.empty()) chip.display = chip_name;

        // Collect temp*_input entries
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        std::vector<std::string> prefixes;
        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            std::string fn = e->d_name;
            if (fn.substr(0, 4) == "temp") {
                size_t pos = fn.find("_input");
                if (pos != std::string::npos) prefixes.push_back(fn.substr(0, pos));
            }
        }
        closedir(d);
        std::sort(prefixes.begin(), prefixes.end());

        int unlabeled_seen = 0;
        for (const auto& pfx : prefixes) {
            auto [val, ok] = read_mc(dir + "/" + pfx + "_input");
            if (!ok) continue;

            std::string real_label;
            { std::ifstream lf(dir + "/" + pfx + "_label");
              if (lf) std::getline(lf, real_label); }

            HwmonSensor s;
            s.temp_celsius = val;

            if (!real_label.empty()) {
                s.label = real_label;
            } else if (chip.category != HwmonCategory::Unknown) {
                // No temp*_label, but the chip itself is identified (e.g.
                // acpitz's lone sensor) — use the chip's friendly name.
                // Numbered only if the chip exposes more than one such
                // sensor (e.g. a Super-I/O chip with several unlabeled
                // probes).
                unlabeled_seen++;
                s.label = chip.display;
            } else {
                // Neither the sensor nor its chip could be identified —
                // a bare "temp1" tells the person nothing useful, so
                // skip it rather than show a meaningless row.
                continue;
            }

            auto [mx, hmx] = read_mc(dir + "/" + pfx + "_max");
            s.temp_max = mx; s.has_max = hmx;
            auto [cr, hcr] = read_mc(dir + "/" + pfx + "_crit");
            s.temp_crit = cr; s.has_crit = hcr;

            s.is_package = (s.label.rfind("Package", 0) == 0);
            chip.sensors.push_back(s);
        }

        // Retroactively number unlabeled sensors sharing one chip's
        // friendly name ("Motherboard" -> "Motherboard 1"/"Motherboard 2")
        // so they're distinguishable — only when there's more than one.
        if (unlabeled_seen > 1) {
            int n = 0;
            for (auto& s : chip.sensors)
                if (s.label == chip.display)
                    s.label = chip.display + " " + std::to_string(++n);
        }
    }
    closedir(base);

    // Result order: coretemp -> k10temp -> rest
    std::vector<HwmonChip> result;
    for (const char* pref : {"coretemp", "k10temp"}) {
        auto it = chips_map.find(pref);
        if (it != chips_map.end() && !it->second.sensors.empty()) {
            result.push_back(std::move(it->second));
            chips_map.erase(it);
        }
    }
    for (auto& [nm, chip] : chips_map)
        if (!chip.sensors.empty()) result.push_back(std::move(chip));
    return result;
}

// Block device parent detection
std::string parent_of(const std::string& dev) {
    // Traditional block: sda1 -> sda, hdb2 -> hdb
    if (dev.size() >= 3 &&
        (dev[0]=='s' || dev[0]=='h' || dev[0]=='v') && dev[1]=='d') {
        size_t i = dev.size() - 1;
        while (i > 2 && std::isdigit(static_cast<unsigned char>(dev[i]))) i--;
        if (i < dev.size() - 1) return dev.substr(0, i + 1);
    }
    // NVMe: nvme0n1p1 -> nvme0n1
    auto p = dev.rfind('p');
    if (p != std::string::npos && p > 2 &&
        std::isdigit(static_cast<unsigned char>(dev.back()))) {
        std::string c = dev.substr(0, p);
        if (!c.empty()) return c;
    }
    // mmcblk: mmcblk0p1 -> mmcblk0
    if (dev.size() > 6 && dev.substr(0, 6) == "mmcblk") {
        auto q = dev.rfind('p');
        if (q != std::string::npos && q > 6) return dev.substr(0, q);
    }
    return "";
}
