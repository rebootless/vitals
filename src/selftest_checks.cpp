#include "selftest_checks.h"

#include "gpu.h"
#include "procfs.h"
#include "sysinfo.h"

#include <chrono>
#include <thread>
#include <vector>

Result check_cpu() {
    const cpustat before = parse_cpustat();
    if (parse_percpu().empty()) return fail("no per-core lines in /proc/stat");
    (void)parse_cpuinfo(); // the model name may legitimately be empty (e.g. ARM)

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const double pct = cpu_delta(before, parse_cpustat());
    if (!(pct >= 0.0 && pct <= 100.0)) return fail("CPU usage out of range");
    return ok();
}

Result check_memory() {
    const meminfo mi = parse_meminfo();
    if (mi.MemTotal == 0)           return fail("MemTotal is 0");
    if (mi.MemFree > mi.MemTotal)   return fail("MemFree exceeds MemTotal");
    if (mi.SwapFree > mi.SwapTotal) return fail("SwapFree exceeds SwapTotal");
    return ok();
}

Result check_gpu() {
    const std::vector<GpuInfo> gpus = parse_gpus();
    if (gpus.empty())              return skip("no GPU detected");
    if (gpus.front().name.empty()) return fail("GPU detected without a name");
    return ok();
}

Result check_storage() {
    if (parse_diskstats().empty()) return fail("no devices in /proc/diskstats");
    if (parse_filesystemstat("/").total_bytes == 0)
        return fail("root filesystem reports zero size");
    return ok();
}

Result check_network() {
    const std::vector<netdev> nets = parse_netdev();
    if (nets.empty()) return fail("no interfaces in /proc/net/dev");
    (void)parse_snmp();
    for (const netdev& nd : nets)
        if (!is_hidden_iface(nd.interface)) return ok();
    return skip("no non-virtual network interfaces");
}

Result check_thermal() {
    const bool zones = !parse_thermal().empty();
    const bool hwmon = !parse_hwmon().empty();
    if (!zones && !hwmon) return skip("no thermal zones or hwmon sensors");
    return ok();
}

Result check_hostname() {
    (void)parse_systemuname();
    return ok();
}

Result check_kernel() {
    if (parse_systemuname().kernel_release.empty())
        return fail("kernel_release is empty");
    return ok();
}

Result check_loadavg() {
    const loadavg l = parse_loadavg();
    if (l.total_tasks > 0 && l.running_tasks > l.total_tasks)
        return fail("running_tasks exceeds total_tasks");
    return ok();
}

Result check_uptime() {
    if (parse_uptime().uptime_seconds <= 0.0) return fail("uptime is not positive");
    return ok();
}
