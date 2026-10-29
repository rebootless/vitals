#include "diag.h"
#include "log.h"
#include "gpu.h"
#include "paths.h"
#include "procfs.h"
#include "sysinfo.h"

#include <clocale>
#include <cstdlib>
#include <functional>
#include <unistd.h>

namespace {

void log_env(const char* name) {
    const char* v = std::getenv(name);
    LOG_INFO("env", "%s=%s", name, v ? v : "(unset)");
}

void probe(const char* name, const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::exception& e) {
        LOG_WARN("probe", "%s failed: %s", name, e.what());
    } catch (...) {
        LOG_WARN("probe", "%s failed: unknown error", name);
    }
}

const char* vendor_name(GpuVendor v) {
    switch (v) {
        case GpuVendor::AMD:    return "AMD";
        case GpuVendor::Nvidia: return "NVIDIA";
        case GpuVendor::Intel:  return "Intel";
        default:                return "unknown";
    }
}

}

void log_startup(int argc, char** argv) {
    LOG_INFO("start", "vitals %s", VITALS_VERSION);
    LOG_INFO("start", "built with %s, C++ standard %ld", __VERSION__, static_cast<long>(__cplusplus));
    LOG_INFO("start", "pid=%d uid=%d", static_cast<int>(getpid()), static_cast<int>(getuid()));

    std::string args;
    for (int i = 1; i < argc; ++i) { if (i > 1) args += ' '; args += argv[i]; }
    LOG_INFO("start", "arguments: %s", args.empty() ? "(none)" : args.c_str());

    for (const char* name : {"TERM", "COLORTERM", "LANG", "LC_ALL", "LC_CTYPE", "HOME",
                             "XDG_CONFIG_HOME", "TERM_PROGRAM", "TMUX", "STY", "SSH_TTY"})
        log_env(name);

    const char* loc = std::setlocale(LC_ALL, nullptr);
    LOG_INFO("start", "locale: %s", loc ? loc : "(unknown)");

    LOG_INFO("paths", "config dir: %s", config_dir().empty() ? "(unavailable)" : config_dir().c_str());
    LOG_INFO("paths", "settings file: %s", config_path().empty() ? "(unavailable)" : config_path().c_str());
    for (const auto& d : theme_dirs()) LOG_INFO("paths", "theme dir: %s", d.string().c_str());
}

void log_terminal(notcurses* nc) {
    LOG_INFO("terminal", "notcurses %s", notcurses_version());

    char* name = notcurses_detected_terminal(nc);
    LOG_INFO("terminal", "detected: %s", name ? name : "(unknown)");
    std::free(name);

    LOG_INFO("terminal", "utf8=%d truecolor=%d palette=%u changecolor=%d",
             notcurses_canutf8(nc), notcurses_cantruecolor(nc),
             notcurses_palette_size(nc), notcurses_canchangecolor(nc));
    LOG_INFO("terminal", "halfblock=%d quadrant=%d sextant=%d braille=%d",
             notcurses_canhalfblock(nc), notcurses_canquadrant(nc),
             notcurses_cansextant(nc), notcurses_canbraille(nc));

    unsigned rows = 0, cols = 0;
    notcurses_stddim_yx(nc, &rows, &cols);
    LOG_INFO("terminal", "size: %ux%u", cols, rows);
}

void log_data_sources() {
    probe("uname", [] {
        const systemuname u = parse_systemuname();
        LOG_INFO("system", "host=%s kernel=%s %s arch=%s", u.hostname.c_str(),
                 u.kernel_name.c_str(), u.kernel_release.c_str(), u.architecture.c_str());
    });

    probe("cpuinfo", [] {
        const cpuinfo c = parse_cpuinfo();
        LOG_INFO("cpu", "model=\"%s\" cores=%d threads=%d", c.model_name.c_str(), c.cpu_cores, c.cpu_siblings);
        LOG_INFO("cpu", "per-core lines in /proc/stat: %zu", parse_percpu().size());
    });

    probe("cpufreq", [] {
        const auto f = parse_cpufreq();
        if (f.empty()) { LOG_INFO("cpu", "no frequency data"); return; }
        double lo = f.front().current_khz, hi = lo;
        for (const auto& e : f) { lo = std::min(lo, e.current_khz); hi = std::max(hi, e.current_khz); }
        LOG_INFO("cpu", "frequency data for %zu cpus, now %.0f..%.0f MHz", f.size(), lo / 1000.0, hi / 1000.0);
    });

    probe("meminfo", [] {
        const meminfo m = parse_meminfo();
        LOG_INFO("memory", "total=%llu kiB available=%llu kiB swap=%llu kiB",
                 m.MemTotal, m.MemAvailable, m.SwapTotal);
    });

    probe("loadavg", [] {
        const loadavg l = parse_loadavg();
        LOG_INFO("system", "load %.2f %.2f %.2f, tasks %d/%d", l.load1, l.load5, l.load15,
                 l.running_tasks, l.total_tasks);
    });

    probe("uptime", [] { LOG_INFO("system", "uptime %.0f s", parse_uptime().uptime_seconds); });

    probe("network", [] {
        for (const auto& nd : parse_netdev())
            LOG_INFO("network", "interface %s%s", nd.interface.c_str(),
                     is_hidden_iface(nd.interface) ? " (hidden)" : "");
        const std::string ip = get_local_ip();
        LOG_INFO("network", "local address: %s", ip.c_str());
    });

    probe("snmp", [] { (void)parse_snmp(); LOG_INFO("network", "/proc/net/snmp readable"); });

    probe("storage", [] {
        for (const auto& ds : parse_diskstats()) {
            const std::string parent = parent_of(ds.device);
            LOG_INFO("storage", "device %s%s%s%s", ds.device.c_str(),
                     is_virtual_disk(ds.device) ? " (virtual)" : "",
                     parent.empty() ? "" : " partition of ", parent.c_str());
        }
        const filesystemstat fs = parse_filesystemstat("/");
        LOG_INFO("storage", "/ total=%llu used=%llu avail=%llu bytes", fs.total_bytes, fs.used_bytes, fs.avail_bytes);
    });

    probe("thermal zones", [] {
        const auto zones = parse_thermal();
        LOG_INFO("thermal", "%zu thermal zones", zones.size());
        for (const auto& z : zones) LOG_INFO("thermal", "zone%d %.1f C", z.zone, z.temperature_celsius);
    });

    probe("hwmon", [] {
        const auto chips = parse_hwmon();
        LOG_INFO("thermal", "%zu hwmon chips", chips.size());
        for (const auto& c : chips) {
            LOG_INFO("thermal", "chip %s (%s), %zu sensors", c.name.c_str(), c.display.c_str(), c.sensors.size());
            for (const auto& s : c.sensors)
                LOG_INFO("thermal", "  %s %.1f C max=%s crit=%s", s.label.c_str(), s.temp_celsius,
                         s.has_max ? std::to_string(s.temp_max).c_str() : "-",
                         s.has_crit ? std::to_string(s.temp_crit).c_str() : "-");
        }
    });

    probe("gpu", [] {
        const auto gpus = parse_gpus();
        if (gpus.empty()) LOG_INFO("gpu", "no GPU detected");
        for (const auto& g : gpus)
            LOG_INFO("gpu", "%s (%s) util=%.0f mem=%.0f/%.0f MB temp=%.0f power=%.1f",
                     g.name.c_str(), vendor_name(g.vendor), g.util_pct, g.mem_used_mb, g.mem_total_mb,
                     g.temp_c, g.power_w);
    });
}
