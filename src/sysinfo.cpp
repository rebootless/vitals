#include "sysinfo.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <ifaddrs.h>
#include <map>
#include <net/if.h>
#include <netinet/in.h>
#include <optional>
#include <set>
#include <sstream>

namespace {

// hwmon10 sorts after hwmon9
bool by_len(const std::string& a, const std::string& b) {
    return a.size() != b.size() ? a.size() < b.size() : a < b;
}

std::string read_first_line(const std::string& path) {
    std::ifstream f(path);
    std::string s;
    std::getline(f, s);
    return s;
}

// a sysfs value in thousandths, as a whole unit
std::optional<double> read_milli(const std::string& path) {
    std::ifstream f(path);
    long long v = 0;
    if (!(f >> v)) return std::nullopt;
    return v / 1000.0;
}

std::vector<std::string> list_dir(const std::string& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        names.push_back(it->path().filename().string());
    return names;
}

// "hwmonN" directories under root, in numeric order
std::vector<std::string> hwmon_dirs(const std::string& root) {
    std::vector<std::string> dirs;
    for (std::string& n : list_dir(root))
        if (n.size() > 5 && n.compare(0, 5, "hwmon") == 0) dirs.push_back(std::move(n));
    std::sort(dirs.begin(), dirs.end(), by_len);
    return dirs;
}

// "temp3" for every temp3_input in dir, in numeric order (kind is "temp" or "fan")
std::vector<std::string> hwmon_channels(const std::string& dir, const std::string& kind) {
    std::vector<std::string> channels;
    for (const std::string& fn : list_dir(dir)) {
        const size_t pos = fn.find("_input");
        if (fn.compare(0, kind.size(), kind) == 0 && pos != std::string::npos && pos > kind.size())
            channels.push_back(fn.substr(0, pos));
    }
    std::sort(channels.begin(), channels.end(), by_len);
    return channels;
}

}

double cpu_delta(const cpustat& prev, const cpustat& cur) {
    auto total = [](const cpustat& s) -> ull {
        return s.user + s.nice + s.system + s.idle + s.iowait
             + s.irq  + s.softirq + s.steal;
    };
    auto idle = [](const cpustat& s) -> ull { return s.idle + s.iowait; };

    // iowait can go backwards
    const ull t0 = total(prev), t1 = total(cur);
    const ull i0 = idle(prev),  i1 = idle(cur);
    const ull dt = t1 > t0 ? t1 - t0 : 0;
    ull di = i1 > i0 ? i1 - i0 : 0;
    if (!dt) return 0.0;
    if (di > dt) di = dt;
    return 100.0 * (1.0 - static_cast<double>(di) / static_cast<double>(dt));
}

// Match htop 3.2.1+ "Used" memory calculation.
// (https://github.com/htop-dev/htop/issues/1051)
MemUsed mem_used(const meminfo& mi) {
    const ull reclaimable = mi.Buffers + mi.Cached + mi.SReclaimable;
    const ull used_kib = (mi.MemTotal > mi.MemFree + reclaimable)
                        ? mi.MemTotal - mi.MemFree - reclaimable : 0;
    const double pct = mi.MemTotal > 0
                       ? 100.0 * static_cast<double>(used_kib) / mi.MemTotal : 0.0;
    return { used_kib, pct };
}

double net_rate_bytes(ull prev_bytes, ull cur_bytes, double dt) {
    if (dt <= 0.0 || cur_bytes < prev_bytes) return 0.0;
    return static_cast<double>(cur_bytes - prev_bytes) / dt;
}

DiskRate disk_rate(ull prev_sectors_read,  ull cur_sectors_read,
                   ull prev_sectors_written, ull cur_sectors_written, double dt) {
    DiskRate r{};
    if (dt <= 0.0) return r;
    if (cur_sectors_read    >= prev_sectors_read)
        r.read_bytes_per_sec  = static_cast<double>(cur_sectors_read    - prev_sectors_read)    * 512.0 / dt;
    if (cur_sectors_written >= prev_sectors_written)
        r.write_bytes_per_sec = static_cast<double>(cur_sectors_written - prev_sectors_written) * 512.0 / dt;
    return r;
}

std::string default_route_iface(std::istream& in) {
    std::string line, best;
    unsigned long best_metric = 0;
    std::getline(in, line); // header
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string iface, dest, gw, mask;
        unsigned flags = 0;
        unsigned long refcnt = 0, use = 0, metric = 0;
        ss >> iface >> dest >> gw >> std::hex >> flags >> std::dec >> refcnt >> use >> metric >> mask;
        if (!ss || dest != "00000000" || mask != "00000000" || !(flags & 0x1)) continue; // RTF_UP
        if (best.empty() || metric < best_metric) { best = iface; best_metric = metric; }
    }
    return best;
}

// the interface with the default route wins even if it is a bridge (Proxmox keeps the host IP on vmbr0)
std::string get_local_ip() {
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return "N/A";

    std::string via;
    { std::ifstream f("/proc/net/route"); if (f) via = default_route_iface(f); }

    std::string preferred, fallback;
    for (struct ifaddrs* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        if (!(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK)) continue;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &reinterpret_cast<struct sockaddr_in*>(p->ifa_addr)->sin_addr,
                  buf, sizeof(buf));
        const std::string name = p->ifa_name;
        if (name == via) { preferred = buf; break; }
        if (fallback.empty() && !is_hidden_iface(name)) fallback = buf;
    }
    freeifaddrs(ifa);

    if (!preferred.empty()) return preferred;
    return fallback.empty() ? "N/A" : fallback;
}

bool is_hidden_iface(const std::string& name) {
    static const char* const prefixes[] = {"ifb", "veth", "docker", "br", "virbr", "vmbr", "vnet",
                                           "bond", "tap", "fwbr", "fwln", "fwpr"};
    if (name == "lo") return true;
    if (name.find('.') != std::string::npos) return true; // VLAN: counted in its parent
    for (const char* p : prefixes)
        if (name.rfind(p, 0) == 0) return true;
    return false;
}

bool is_virtual_disk(const std::string& dev) {
    static const char* const prefixes[] = {"dm-", "loop", "sr", "ram", "zram", "md", "nbd", "zd"};
    for (const char* p : prefixes)
        if (dev.rfind(p, 0) == 0) return true;
    return false;
}

std::string thermal_zone_type(int zone) {
    const std::string t = read_first_line("/sys/class/thermal/thermal_zone" + std::to_string(zone) + "/type");
    return t.empty() ? "Zone " + std::to_string(zone) : t;
}

std::pair<double,double> thermal_trip_limits(int zone) {
    const std::string base = "/sys/class/thermal/thermal_zone" + std::to_string(zone);
    double hi = 0, crit = 0;
    for (int i = 0; i <= 6; i++) {
        const std::string trip = base + "/trip_point_" + std::to_string(i);
        const std::string type = read_first_line(trip + "_type");
        if (type != "hot" && type != "critical") continue;
        (type == "hot" ? hi : crit) = read_milli(trip + "_temp").value_or(0.0);
    }
    if (hi == 0 && crit == 0) { hi = 74; crit = 94; }
    else if (hi   == 0) hi   = crit - 20;
    else if (crit == 0) crit = hi   + 20;
    return {hi, crit};
}

std::pair<double,double> hwmon_limits(const HwmonSensor& s) {
    if (s.has_crit && s.temp_crit > 0) {
        const bool max_ok = s.has_max && s.temp_max > 0 && s.temp_max < s.temp_crit;
        return {max_ok ? s.temp_max : s.temp_crit - 20.0, s.temp_crit};
    }
    if (s.has_max && s.temp_max > 0) return {s.temp_max, s.temp_max + 20.0};
    return {-1.0, -1.0};
}

// amdgpu/nouveau/nvidia are excluded: parse_gpus() already reports their temperature
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
        {"nct6683",   {HwmonCategory::Motherboard, "Motherboard"}},
        {"nct6687",   {HwmonCategory::Motherboard, "Motherboard"}}, // the in-kernel nct6683 driver names an NCT6687D this way
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

std::vector<HwmonChip> parse_hwmon(const std::string& root) {
    // one chip per hwmonN directory; same-named chips (two NVMe drives, two sockets) stay separate
    std::vector<std::pair<std::string, std::string>> found; // {dir name, chip name}
    for (const auto& d : hwmon_dirs(root)) {
        const std::string name = read_first_line(root + "/" + d + "/name");
        // GPUs come from parse_gpus(); skip here to avoid a duplicate entry
        if (name.empty() || name == "amdgpu" || name == "nouveau" || name == "nvidia") continue;
        found.emplace_back(d, name);
    }

    std::map<std::string, int> total_of, seen_of;
    for (const auto& f : found) ++total_of[f.second];

    std::vector<HwmonChip> chips;
    for (const auto& [dname, chip_name] : found) {
        const std::string dir = root + "/" + dname;

        chips.emplace_back();
        HwmonChip& chip = chips.back();
        chip.name = chip_name;
        classify_hwmon_chip(chip_name, chip.category, chip.display);
        if (chip.display.empty()) chip.display = chip_name;

        int unlabeled_seen = 0;
        for (const auto& pfx : hwmon_channels(dir, "temp")) {
            const auto val = read_milli(dir + "/" + pfx + "_input");
            if (!val) continue;

            HwmonSensor s;
            s.temp_celsius = *val;
            s.label = read_first_line(dir + "/" + pfx + "_label");
            if (s.label.empty()) {
                if (chip.category == HwmonCategory::Unknown) continue;
                unlabeled_seen++;
                s.label = chip.display;
            }

            if (const auto mx = read_milli(dir + "/" + pfx + "_max"))  { s.temp_max  = *mx; s.has_max  = true; }
            if (const auto cr = read_milli(dir + "/" + pfx + "_crit")) { s.temp_crit = *cr; s.has_crit = true; }

            s.is_package = (s.label.rfind("Package", 0) == 0);
            chip.sensors.push_back(s);
        }

        if (unlabeled_seen > 1) {
            int n = 0;
            for (auto& s : chip.sensors)
                if (s.label == chip.display)
                    s.label = chip.display + " " + std::to_string(++n);
        }

        // tell identical chips apart by number; CPU labels ("Core N") are parsed by the panel
        const int ordinal = ++seen_of[chip_name];
        if (total_of[chip_name] > 1 && chip.category != HwmonCategory::CPU)
            for (auto& s : chip.sensors) s.label += " " + std::to_string(ordinal);
    }

    // Result order: coretemp -> k10temp -> rest (by name)
    std::vector<HwmonChip> result;
    for (const char* pref : {"coretemp", "k10temp"})
        for (auto& chip : chips)
            if (chip.name == pref && !chip.sensors.empty()) result.push_back(std::move(chip));

    std::stable_sort(chips.begin(), chips.end(),
                     [](const HwmonChip& a, const HwmonChip& b) { return a.name < b.name; });
    for (auto& chip : chips)
        if (chip.name != "coretemp" && chip.name != "k10temp" && !chip.sensors.empty())
            result.push_back(std::move(chip));
    return result;
}

std::string fan_short_name(const std::string& label) {
    std::string u;
    for (unsigned char c : label) u += static_cast<char>(std::toupper(c));
    for (const char* key : {"CPU", "GPU", "PSU", "PUMP", "AIO"})
        if (u.find(key) != std::string::npos) return key;
    for (const char* key : {"SYS", "CHA", "CASE"})
        if (u.find(key) != std::string::npos) return "SYS";
    return "";
}

std::vector<FanInfo> collect_fans(const std::vector<GpuInfo>& gpus, const std::string& hwmon_root) {
    // "fan3" -> 3; 0 when the channel is not 1..9 (FAN10+ would break the name width and the naming scheme)
    auto channel_of = [](const std::string& pfx) {
        const std::string num = pfx.substr(3);
        return num.size() == 1 && num[0] >= '1' && num[0] <= '9' ? num[0] - '0' : 0;
    };

    struct Entry { FanInfo fan; std::string fallback; };
    std::vector<Entry> all;

    for (const auto& g : gpus)
        if (g.fan_pct >= 0) {
            Entry en;
            en.fan.name = "GPU";
            en.fan.pct  = static_cast<int>(std::clamp(g.fan_pct, 0.0, 100.0) + 0.5);
            all.push_back(en);
        }

    for (const auto& d : hwmon_dirs(hwmon_root)) {
        const std::string dir  = hwmon_root + "/" + d;
        const std::string chip = read_first_line(dir + "/name");

        for (const auto& pfx : hwmon_channels(dir, "fan")) {
            const int chan = channel_of(pfx);
            if (chan == 0) continue;
            long long rpm = 0;
            { std::ifstream f(dir + "/" + pfx + "_input"); if (!(f >> rpm)) continue; }

            Entry en;
            en.fan.rpm  = static_cast<int>(std::clamp(rpm, 0LL, 9999LL));
            en.fallback = "FAN" + std::to_string(chan);
            en.fan.name = chip == "amdgpu" ? "GPU" : fan_short_name(read_first_line(dir + "/" + pfx + "_label"));
            if (en.fan.name.empty()) en.fan.name = en.fallback;
            all.push_back(en);
        }
    }

    // GPU first (NVIDIA, or amdgpu's hwmon fan), then the board's fans in channel order
    std::stable_partition(all.begin(), all.end(), [](const Entry& e) { return e.fan.name == "GPU"; });
    if (all.size() > static_cast<size_t>(FAN_MAX)) all.resize(FAN_MAX);

    // a repeated name (two "CPU" labels, two chips with a fan1) falls back to the first free FAN<1..9>
    std::set<std::string> taken;
    std::vector<FanInfo> fans;
    for (Entry& en : all) {
        if (taken.count(en.fan.name)) {
            en.fan.name = en.fallback;
            for (int k = 1; taken.count(en.fan.name) && k <= 9; ++k) en.fan.name = "FAN" + std::to_string(k);
        }
        taken.insert(en.fan.name);
        fans.push_back(en.fan);
    }
    return fans;
}

std::string parent_of(const std::string& dev) {
    if (dev.size() >= 3 &&
        (dev[0]=='s' || dev[0]=='h' || dev[0]=='v') && dev[1]=='d') {
        size_t i = dev.size() - 1;
        while (i > 2 && std::isdigit(static_cast<unsigned char>(dev[i]))) i--;
        if (i < dev.size() - 1) return dev.substr(0, i + 1);
    }
    auto p = dev.rfind('p');
    if (p != std::string::npos && p > 2 &&
        std::isdigit(static_cast<unsigned char>(dev.back()))) {
        std::string c = dev.substr(0, p);
        if (!c.empty()) return c;
    }
    if (dev.size() > 6 && dev.substr(0, 6) == "mmcblk") {
        auto q = dev.rfind('p');
        if (q != std::string::npos && q > 6) return dev.substr(0, q);
    }
    return "";
}

PanelFlags panel_availability(const meminfo& mem, const std::vector<GpuInfo>& gpus,
                              const filesystemstat& root_fs, const std::vector<cpustat>& cores,
                              const std::vector<netdev>& net, const std::vector<diskstats>& disks,
                              const std::vector<thermal>& zones, const std::vector<HwmonChip>& hwmon,
                              const std::vector<FanInfo>& fans) {
    PanelFlags a{};
    a[P_CPU]     = !cores.empty();
    a[P_GPU]     = !gpus.empty();
    a[P_MEMORY]  = mem.MemTotal > 0;
    a[P_NETWORK] = std::any_of(net.begin(), net.end(),
                               [](const netdev& nd) { return !is_hidden_iface(nd.interface); });
    a[P_STORAGE] = root_fs.total_bytes > 0 ||
                   std::any_of(disks.begin(), disks.end(),
                               [](const diskstats& d) { return !is_virtual_disk(d.device); });
    a[P_FAN]     = !fans.empty();
    a[P_THERMAL] = !hwmon.empty() || !zones.empty() || (!gpus.empty() && gpus.front().temp_c >= 0);
    return a;
}
