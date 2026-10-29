#include "gpu.h"
#include "log.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <unistd.h>
#include <vector>

extern char** environ;

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
    std::set<std::string> seen_clients;

    DIR* proc = opendir("/proc");
    if (!proc) return -1.0;

    struct dirent* pe;
    while ((pe = readdir(proc)) != nullptr) {
        std::string pidname = pe->d_name;
        if (pidname.empty() || !std::isdigit(static_cast<unsigned char>(pidname[0])))
            continue;

        const std::string base = "/proc/" + pidname;
        const std::string fdinfo_dir = base + "/fdinfo";
        DIR* fdd = opendir(fdinfo_dir.c_str());
        if (!fdd) continue;

        struct dirent* fe;
        while ((fe = readdir(fdd)) != nullptr) {
            std::string fname = fe->d_name;
            if (fname == "." || fname == "..") continue;

            // only DRM file descriptors: a readlink is far cheaper than reading every fdinfo
            char target[64];
            ssize_t n = readlink((base + "/fd/" + fname).c_str(), target, sizeof(target) - 1);
            if (n <= 0) continue;
            target[n] = '\0';
            if (std::strncmp(target, "/dev/dri/", 9) != 0) continue;

            std::ifstream f(fdinfo_dir + "/" + fname);
            if (!f) continue;

            std::string line, pdev, client;
            unsigned long long busy = 0;
            bool has_engine = false;

            while (std::getline(f, line)) {
                if (line.rfind("drm-pdev:", 0) == 0) {
                    pdev = trim(line.substr(9));
                } else if (line.rfind("drm-client-id:", 0) == 0) {
                    client = trim(line.substr(14));
                } else if (line.rfind("drm-engine-gr:", 0) == 0) {
                    std::istringstream ls(line.substr(14));
                    unsigned long long v = 0;
                    ls >> v; // value is "<ns> ns" — trailing unit is ignored
                    busy = v;
                    has_engine = true;
                }
            }

            if (!has_engine || pdev != pci_addr) continue;
            // dup()/fork() share one DRM client across several fds and processes: count it once
            if (!client.empty() && !seen_clients.insert(client).second) continue;

            total_busy += busy;
            found = true;
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

// "nvidia-smi -l 1" runs once and is read without blocking: spawning it every tick costs a full NVML init, and a hung driver would freeze the UI. Some old cards (e.g. Kepler) report [N/A] utilization in NVML itself.
bool parse_smi_line(const std::string& line, GpuInfo& g) {
    std::istringstream ls(line);
    std::string field;
    std::vector<std::string> fields;
    while (std::getline(ls, field, ',')) fields.push_back(trim(field));
    if (fields.size() < 6) {
        static int reported = 0;
        if (reported++ < 5) LOG_WARN("gpu", "nvidia-smi: unparsable line: %s", line.c_str());
        return false;
    }

    g = GpuInfo{};
    g.vendor = GpuVendor::Nvidia;
    g.name   = fields[0];
    try { g.util_pct     = std::stod(fields[1]); } catch (...) {}
    try { g.mem_used_mb  = std::stod(fields[2]); } catch (...) {}
    try { g.mem_total_mb = std::stod(fields[3]); } catch (...) {}
    try { g.temp_c       = std::stod(fields[4]); } catch (...) {}
    try { g.power_w      = std::stod(fields[5]); } catch (...) {}
    return true;
}

class NvidiaSmi {
    using Tp = std::chrono::steady_clock::time_point;
    static constexpr std::chrono::seconds STALE{5};  // no new line for this long: values become n/a
    static constexpr std::chrono::seconds RETRY{5};  // pause before respawning
    static constexpr int FIRST_SAMPLE_MS = 2000;     // the only blocking wait, once

public:
    ~NvidiaSmi() { kill_child(); }

    std::vector<GpuInfo> latest() {
        reap();
        Tp now = std::chrono::steady_clock::now();
        if (pid_ < 0 && now >= next_start_) start(now);

        if (pid_ >= 0 && !have_ && !waited_) {
            waited_ = true;
            pollfd p{fd_, POLLIN, 0};
            if (::poll(&p, 1, FIRST_SAMPLE_MS) == 0)
                LOG_WARN("gpu", "nvidia-smi: no output within %d ms, continuing without it", FIRST_SAMPLE_MS);
            now = std::chrono::steady_clock::now();
        }
        if (pid_ >= 0) drain(now);

        if (!have_) return {};
        GpuInfo g = last_;
        const bool stale = now - last_line_ > STALE;
        if (stale != stale_) {
            stale_ = stale;
            if (stale) LOG_WARN("gpu", "nvidia-smi: no new data for %lld s, values shown as n/a", static_cast<long long>(STALE.count()));
            else       LOG_INFO("gpu", "nvidia-smi: data is fresh again");
        }
        if (stale) g.util_pct = g.mem_used_mb = g.mem_total_mb = g.temp_c = g.power_w = -1.0;
        return {g};
    }

private:
    pid_t              pid_ = -1;
    int                fd_  = -1;
    std::string        buf_;
    std::vector<pid_t> dying_;
    GpuInfo            last_;
    bool               have_ = false, waited_ = false, stale_ = false;
    int                last_spawn_rc_ = 0;
    Tp                 started_{}, last_line_{}, next_start_{};

    void start(Tp now) {
        int fds[2];
        if (pipe2(fds, O_CLOEXEC) != 0) {
            LOG_ERROR("gpu", "nvidia-smi: pipe2 failed: %s", std::strerror(errno));
            next_start_ = now + RETRY;
            return;
        }
        fcntl(fds[0], F_SETFL, O_NONBLOCK);

        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_adddup2(&fa, fds[1], 1);
        posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);

        const char* const args[] = {
            "nvidia-smi", "-i", "0",
            "--query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu,power.draw",
            "--format=csv,noheader,nounits", "-l", "1", nullptr};
        pid_t pid = -1;
        const int rc = posix_spawnp(&pid, "nvidia-smi", &fa, nullptr,
                                    const_cast<char* const*>(args), environ);
        posix_spawn_file_actions_destroy(&fa);
        ::close(fds[1]); // once we exit, the child's next write gets SIGPIPE
        if (rc != 0) {
            if (rc != last_spawn_rc_) // a missing binary would repeat every RETRY
                LOG_WARN("gpu", "nvidia-smi: cannot start: %s (retrying every %lld s)", std::strerror(rc),
                         static_cast<long long>(RETRY.count()));
            last_spawn_rc_ = rc;
            ::close(fds[0]);
            next_start_ = now + RETRY;
            return;
        }

        last_spawn_rc_ = 0;
        pid_ = pid; fd_ = fds[0]; started_ = now; buf_.clear();
        LOG_INFO("gpu", "nvidia-smi started, pid %d", static_cast<int>(pid));
    }

    void drain(Tp now) {
        char tmp[512];
        for (;;) {
            const ssize_t n = ::read(fd_, tmp, sizeof(tmp));
            if (n > 0) { buf_.append(tmp, static_cast<size_t>(n)); continue; }
            if (n < 0 && errno == EINTR) continue;
            if (n == 0) {
                LOG_WARN("gpu", "nvidia-smi exited (pipe closed), restart in %lld s", static_cast<long long>(RETRY.count()));
                kill_child();
                next_start_ = now + RETRY;
            }
            break; // EOF, or EAGAIN: nothing more for now
        }

        size_t nl;
        while ((nl = buf_.find('\n')) != std::string::npos) {
            GpuInfo g;
            if (parse_smi_line(buf_.substr(0, nl), g)) {
                if (!have_) LOG_INFO("gpu", "nvidia-smi: first sample: %s", buf_.substr(0, nl).c_str());
                last_ = g; have_ = true; last_line_ = now;
            }
            buf_.erase(0, nl + 1);
        }
        if (buf_.size() > 4096) buf_.clear();

        // alive but silent (hung driver): restart it
        if (pid_ >= 0 && now - std::max(last_line_, started_) > 2 * STALE) {
            LOG_WARN("gpu", "nvidia-smi: silent for %lld s, restarting it", static_cast<long long>((2 * STALE).count()));
            kill_child();
            next_start_ = now;
        }
    }

    // SIGKILL can't take effect on a process stuck in the driver, so never wait for it here
    void kill_child() {
        if (pid_ >= 0) {
            LOG_INFO("gpu", "nvidia-smi: killing pid %d", static_cast<int>(pid_));
            ::kill(pid_, SIGKILL);
            dying_.push_back(pid_);
            pid_ = -1;
        }
        if (fd_ >= 0)  { ::close(fd_); fd_ = -1; }
        buf_.clear();
    }

    void reap() {
        dying_.erase(std::remove_if(dying_.begin(), dying_.end(),
                                    [](pid_t p) {
                                        const bool gone = ::waitpid(p, nullptr, WNOHANG) != 0;
                                        if (gone) LOG_DEBUG("gpu", "nvidia-smi: reaped pid %d", static_cast<int>(p));
                                        return gone;
                                    }),
                     dying_.end());
    }
};

std::vector<GpuInfo> parse_nvidia_proprietary() {
    static NvidiaSmi smi;
    return smi.latest();
}

}

GpuVendor gpu_vendor_from_id(const std::string& vendor_id) {
    if (vendor_id == "0x1002") return GpuVendor::AMD;
    if (vendor_id == "0x8086") return GpuVendor::Intel;
    if (vendor_id == "0x10de") return GpuVendor::Nvidia;
    return GpuVendor::Unknown;
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
        const GpuVendor vendor = gpu_vendor_from_id(c.vendor_id);
        if (vendor == GpuVendor::AMD) {
            result.push_back(parse_amd(c.device_dir, c.name));
            break;

        } else if (vendor == GpuVendor::Intel) {
            result.push_back(parse_intel(c.device_dir, c.name));
            break;

        } else if (vendor == GpuVendor::Nvidia) {
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

    // logged when it changes, not on every tick
    std::string summary = sysfs_enum_failed ? "/sys/class/drm unreadable; " : "";
    for (const auto& c : candidates) summary += c.name + " vendor=" + (c.vendor_id.empty() ? "?" : c.vendor_id) + "; ";
    summary += result.empty() ? "no GPU used" : "using " + result.front().name;
    static std::string last_summary;
    if (summary != last_summary) {
        LOG_INFO("gpu", "detection: %s", summary.c_str());
        last_summary = summary;
    }

    return result;
}
