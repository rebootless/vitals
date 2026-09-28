#include "procfs.h"

#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <sys/statvfs.h>
#include <sys/utsname.h>

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

cpustat parse_cpustat_from(std::istream& in) {
    std::string line;
    while (std::getline(in, line)) {
        if (line.substr(0, 4) != "cpu ") continue;
        std::istringstream ss(line.substr(4));
        cpustat s{};
        ss >> s.user   >> s.nice >> s.system  >> s.idle
           >> s.iowait >> s.irq  >> s.softirq >> s.steal
           >> s.guest  >> s.guest_nice;
        return s;
    }
    throw std::runtime_error("aggregate cpu line not found");
}

cpustat parse_cpustat() {
    std::ifstream f("/proc/stat");
    if (!f) throw std::runtime_error("cannot open /proc/stat");
    return parse_cpustat_from(f);
}

meminfo parse_meminfo_from(std::istream& in) {
    meminfo m{};
    bool has_available = false;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string key;
        unsigned long long value;
        ss >> key >> value;
        if      (key == "MemTotal:")     m.MemTotal     = value;
        else if (key == "MemFree:")      m.MemFree      = value;
        else if (key == "MemAvailable:") { m.MemAvailable = value; has_available = true; }
        else if (key == "Buffers:")      m.Buffers      = value;
        else if (key == "Cached:")       m.Cached       = value;
        else if (key == "SwapTotal:")    m.SwapTotal    = value;
        else if (key == "SwapFree:")     m.SwapFree     = value;
        else if (key == "Active:")       m.Active       = value;
        else if (key == "Inactive:")     m.Inactive     = value;
        else if (key == "Dirty:")        m.Dirty        = value;
        else if (key == "Writeback:")    m.Writeback    = value;
        else if (key == "Slab:")         m.Slab         = value;
        else if (key == "Shmem:")        m.Shmem        = value;
        else if (key == "SReclaimable:") m.SReclaimable = value;
    }

    // fallback without MemAvailable (< 3.14): reclaimable memory minus Shmem, as old procps did
    if (!has_available) {
        unsigned long long reclaimable = m.Buffers + m.Cached + m.SReclaimable;
        unsigned long long shmem_adj   = std::min(reclaimable, m.Shmem);
        m.MemAvailable = m.MemFree + reclaimable - shmem_adj;
    }
    return m;
}

meminfo parse_meminfo() {
    std::ifstream f("/proc/meminfo");
    if (!f) throw std::runtime_error("cannot open /proc/meminfo");
    return parse_meminfo_from(f);
}

uptime parse_uptime_from(std::istream& in) {
    uptime u{};
    in >> u.uptime_seconds >> u.idle_seconds;
    return u;
}

uptime parse_uptime() {
    std::ifstream f("/proc/uptime");
    if (!f) throw std::runtime_error("cannot open /proc/uptime");
    return parse_uptime_from(f);
}

loadavg parse_loadavg_from(std::istream& in) {
    loadavg l{};
    std::string tasks;
    in >> l.load1 >> l.load5 >> l.load15 >> tasks >> l.last_pid;

    auto slash = tasks.find('/');
    if (slash != std::string::npos) {
        l.running_tasks = static_cast<unsigned int>(std::stoul(tasks.substr(0, slash)));
        l.total_tasks   = static_cast<unsigned int>(std::stoul(tasks.substr(slash + 1)));
    }
    return l;
}

loadavg parse_loadavg() {
    std::ifstream f("/proc/loadavg");
    if (!f) throw std::runtime_error("cannot open /proc/loadavg");
    return parse_loadavg_from(f);
}

cpuinfo parse_cpuinfo() {
    std::ifstream f("/proc/cpuinfo");
    if (!f) throw std::runtime_error("cannot open /proc/cpuinfo");

    cpuinfo c{};
    bool got_model    = false;
    bool got_cores    = false;
    bool got_siblings = false;

    std::string line;
    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = trim(line.substr(0, colon));
        std::string val = trim(line.substr(colon + 1));

        if      (!got_model    && key == "model name") { c.model_name  = val;            got_model    = true; }
        else if (!got_cores    && key == "cpu cores")  { c.cpu_cores   = std::stoi(val); got_cores    = true; }
        else if (!got_siblings && key == "siblings")   { c.cpu_siblings = std::stoi(val); got_siblings = true; }

        if (got_model && got_cores && got_siblings) break;
    }
    return c;
}

std::vector<netdev> parse_netdev_from(std::istream& in) {
    std::vector<netdev> result;
    std::string line;
    std::getline(in, line);
    std::getline(in, line);

    while (std::getline(in, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        netdev nd{};
        nd.interface = trim(line.substr(0, colon));

        std::istringstream ss(line.substr(colon + 1));
        unsigned long long skip;
        ss >> nd.rx_bytes >> nd.rx_packets >> nd.rx_errors >> nd.rx_dropped
           >> skip >> skip >> skip >> skip;
        ss >> nd.tx_bytes >> nd.tx_packets >> nd.tx_errors >> nd.tx_dropped;

        result.push_back(std::move(nd));
    }
    return result;
}

std::vector<netdev> parse_netdev() {
    std::ifstream f("/proc/net/dev");
    if (!f) throw std::runtime_error("cannot open /proc/net/dev");
    return parse_netdev_from(f);
}

snmp parse_snmp() {
    std::ifstream f("/proc/net/snmp");
    if (!f) throw std::runtime_error("cannot open /proc/net/snmp");

    using Fields = std::vector<std::string>;
    std::map<std::string, std::pair<Fields, Fields>> table;

    std::string line;
    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string proto = line.substr(0, colon);
        std::istringstream ss(line.substr(colon + 1));
        Fields fields;
        std::string token;
        while (ss >> token) fields.push_back(token);

        auto& entry = table[proto];
        if (entry.first.empty()) entry.first  = std::move(fields);
        else                     entry.second = std::move(fields);
    }

    auto get = [&](const std::string& proto, const std::string& field) -> unsigned long long {
        auto it = table.find(proto);
        if (it == table.end()) return 0;
        const auto& headers = it->second.first;
        const auto& values  = it->second.second;
        for (size_t i = 0; i < headers.size(); ++i) {
            if (headers[i] == field && i < values.size())
                return std::stoull(values[i]);
        }
        return 0;
    };

    snmp s{};
    s.tcp_in_segments   = get("Tcp", "InSegs");
    s.tcp_out_segments  = get("Tcp", "OutSegs");
    s.udp_in_datagrams  = get("Udp", "InDatagrams");
    s.udp_out_datagrams = get("Udp", "OutDatagrams");
    s.ip_in_receives    = get("Ip",  "InReceives");
    s.ip_out_requests   = get("Ip",  "OutRequests");
    return s;
}

std::vector<diskstats> parse_diskstats_from(std::istream& in) {
    std::vector<diskstats> result;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        unsigned int major, minor;
        diskstats d{};
        ss >> major >> minor >> d.device
           >> d.reads_completed  >> d.reads_merged   >> d.sectors_read    >> d.read_time_ms
           >> d.writes_completed >> d.writes_merged  >> d.sectors_written >> d.write_time_ms
           >> d.io_in_progress   >> d.io_time_ms;
        if (!d.device.empty()) result.push_back(std::move(d));
    }
    return result;
}

std::vector<diskstats> parse_diskstats() {
    std::ifstream f("/proc/diskstats");
    if (!f) throw std::runtime_error("cannot open /proc/diskstats");
    return parse_diskstats_from(f);
}

double millideg_to_celsius(unsigned long long raw_millidegrees) {
    return raw_millidegrees / 1000.0;
}

std::vector<thermal> parse_thermal() {
    const char* base = "/sys/class/thermal";
    std::vector<thermal> result;

    DIR* dir = opendir(base);
    if (!dir) return result;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name.size() <= 12 || name.substr(0, 12) != "thermal_zone") continue;

        int zone = std::stoi(name.substr(12));
        std::ifstream f(std::string(base) + "/" + name + "/temp");
        if (!f) continue;

        unsigned long long raw = 0;
        f >> raw;
        result.push_back({zone, millideg_to_celsius(raw)});
    }
    closedir(dir);

    std::sort(result.begin(), result.end(),
              [](const thermal& a, const thermal& b) { return a.zone < b.zone; });
    return result;
}

static std::vector<cpufreq> parse_cpufreq_from_cpuinfo() {
    std::vector<cpufreq> result;
    std::ifstream f("/proc/cpuinfo");
    if (!f) return result;

    std::string line;
    int cpu = -1;
    while (std::getline(f, line)) {
        if (line.rfind("processor", 0) == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos)
                cpu = std::stoi(line.substr(pos + 1));
        } else if (line.rfind("cpu MHz", 0) == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos && cpu >= 0) {
                cpufreq cf{};
                cf.cpu         = cpu;
                cf.current_khz = std::stod(line.substr(pos + 1)) * 1000.0;
                cf.min_khz     = 0.0;
                cf.max_khz     = 0.0;
                result.push_back(cf);
            }
        }
    }

    std::sort(result.begin(), result.end(),
              [](const cpufreq& a, const cpufreq& b) { return a.cpu < b.cpu; });
    return result;
}

std::vector<cpufreq> parse_cpufreq() {
    const char* base = "/sys/devices/system/cpu";
    std::vector<cpufreq> result;

    DIR* dir = opendir(base);
    if (!dir) return parse_cpufreq_from_cpuinfo();

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name.size() <= 3 || name.substr(0, 3) != "cpu") continue;
        if (!std::isdigit(static_cast<unsigned char>(name[3]))) continue;

        int cpu = std::stoi(name.substr(3));
        std::string freq_dir = std::string(base) + "/" + name + "/cpufreq";

        auto read_khz = [](const std::string& path) -> double {
            std::ifstream f(path);
            if (!f) return 0.0;
            unsigned long long v = 0;
            f >> v;
            return static_cast<double>(v);
        };

        cpufreq cf{};
        cf.cpu         = cpu;
        cf.current_khz = read_khz(freq_dir + "/scaling_cur_freq");
        cf.min_khz     = read_khz(freq_dir + "/scaling_min_freq");
        cf.max_khz     = read_khz(freq_dir + "/scaling_max_freq");

        if (cf.max_khz == 0.0) continue;
        result.push_back(cf);
    }
    closedir(dir);

    std::sort(result.begin(), result.end(),
              [](const cpufreq& a, const cpufreq& b) { return a.cpu < b.cpu; });

    if (result.empty()) return parse_cpufreq_from_cpuinfo();
    return result;
}

filesystemstat parse_filesystemstat(const std::string& path) {
    struct statvfs sv{};
    if (::statvfs(path.c_str(), &sv) != 0)
        throw std::runtime_error("statvfs failed for " + path);

    unsigned long long block = sv.f_frsize ? sv.f_frsize : sv.f_bsize;
    filesystemstat stat{};
    stat.total_bytes  = sv.f_blocks * block;
    stat.free_bytes   = sv.f_bfree  * block;
    stat.used_bytes   = stat.total_bytes - stat.free_bytes;
    stat.total_inodes = sv.f_files;
    stat.free_inodes  = sv.f_ffree;
    return stat;
}

systemuname systemuname_from(const struct utsname& u) {
    systemuname s{};
    s.kernel_name    = u.sysname;
    s.hostname       = u.nodename;
    s.kernel_release = u.release;
    s.kernel_version = u.version;
    s.architecture   = u.machine;
    return s;
}

systemuname parse_systemuname() {
    struct utsname u{};
    if (::uname(&u) != 0)
        throw std::runtime_error("uname() failed");
    return systemuname_from(u);
}
