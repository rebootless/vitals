#include "selftest.h"

#include "gpu.h"
#include "procfs.h"
#include "sysinfo.h"

#include <chrono>
#include <cstdio>
#include <exception>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Each check reads the same sources the panels do. A parser that throws counts
// as FAIL; optional hardware (GPU, thermal sensors) reports SKIP when absent.

namespace {

enum class Status { Ok, Skip, Fail };

struct Result {
    Status      status = Status::Ok;
    std::string reason;
};

Result ok()                  { return {Status::Ok,   ""}; }
Result skip(std::string why) { return {Status::Skip, std::move(why)}; }
Result fail(std::string why) { return {Status::Fail, std::move(why)}; }

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

// Header line: hostname/kernel, uptime, load average.
Result check_system() {
    (void)parse_systemuname();
    (void)parse_loadavg();
    if (parse_uptime().uptime_seconds <= 0.0) return fail("uptime is not positive");
    return ok();
}

struct Check {
    const char* name;
    Result    (*run)();
};

const char* label(Status s) {
    switch (s) {
        case Status::Ok:   return "OK";
        case Status::Skip: return "SKIP";
        case Status::Fail: return "FAIL";
    }
    return "FAIL";
}

} // namespace

int run_self_test() {
    static const Check checks[] = {
        {"CPU",     check_cpu},
        {"Memory",  check_memory},
        {"GPU",     check_gpu},
        {"Storage", check_storage},
        {"Network", check_network},
        {"Thermal", check_thermal},
        {"System",  check_system},
    };

    int failures = 0;
    for (const Check& c : checks) {
        Result r;
        try {
            r = c.run();
        } catch (const std::exception& e) {
            r = fail(e.what());
        } catch (...) {
            r = fail("unknown error");
        }
        if (r.status == Status::Fail) ++failures;

        std::printf("%-8s%s", c.name, label(r.status));
        if (!r.reason.empty()) std::printf("  %s", r.reason.c_str());
        std::printf("\n");
    }
    return failures ? 1 : 0;
}
