#include "selftest_fixtures.h"

#include "gpu.h"
#include "procfs.h"
#include "sysinfo.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sstream>
#include <string>
#include <sys/utsname.h>

namespace {

struct utsname fake_utsname() {
    struct utsname u{};
    std::snprintf(u.sysname,  sizeof(u.sysname),  "Linux");
    std::snprintf(u.nodename, sizeof(u.nodename), "myhost");
    std::snprintf(u.release,  sizeof(u.release),  "6.1.0-generic");
    std::snprintf(u.version,  sizeof(u.version),  "#1 SMP");
    std::snprintf(u.machine,  sizeof(u.machine),  "x86_64");
    return u;
}

}

Result check_fixture_cpu() {
    std::istringstream before("cpu  0 0 0 0 0 0 0 0 0 0\n");
    std::istringstream after ("cpu  50 0 0 50 0 0 0 0 0 0\n");
    const cpustat c0 = parse_cpustat_from(before);
    const cpustat c1 = parse_cpustat_from(after);

    const double pct = cpu_delta(c0, c1);
    // total 0->100 (dt=100), idle 0->50 (di=50) => 100*(1-50/100) = 50%
    if (std::abs(pct - 50.0) > 0.01)
        return fail("cpu_delta: expected 50.0, got " + std::to_string(pct));
    return ok();
}

Result check_fixture_memory() {
    std::istringstream in(
        "MemTotal:       10000000 kB\n"
        "MemFree:         2000000 kB\n"
        "MemAvailable:    5000000 kB\n"
        "Buffers:          100000 kB\n"
        "Cached:          3000000 kB\n"
        "SwapTotal:       2000000 kB\n"
        "SwapFree:        2000000 kB\n"
        "Slab:             200000 kB\n"
        "Shmem:            100000 kB\n"
        "SReclaimable:     100000 kB\n");
    const meminfo mi = parse_meminfo_from(in);

    if (mi.MemTotal != 10000000) return fail("MemTotal parsed wrong");
    if (mi.MemFree  != 2000000)  return fail("MemFree parsed wrong");

    const MemUsed mu = mem_used(mi);
    // reclaimable = 100000+3000000+100000 = 3200000
    // used = 10000000 - 2000000 - 3200000 = 4800000 kiB => 48%
    if (mu.used_kib != 4800000)
        return fail("mem_used: expected 4800000 KiB, got " + std::to_string(mu.used_kib));
    if (std::abs(mu.pct - 48.0) > 0.01)
        return fail("mem_used: expected 48.0%, got " + std::to_string(mu.pct));
    return ok();
}

Result check_fixture_gpu() {
    // 0x1002 is AMD's PCI vendor ID
    const GpuVendor v = gpu_vendor_from_id("0x1002");
    if (v != GpuVendor::AMD)
        return fail("gpu_vendor_from_id(\"0x1002\"): expected AMD");
    return ok();
}

Result check_fixture_storage() {
    auto sample = [](unsigned long long rd_sectors, unsigned long long wr_sectors) {
        std::ostringstream ss;
        ss << "   8       0 sda 100 0 " << rd_sectors << " 10 200 0 " << wr_sectors << " 20 0 30\n";
        return ss.str();
    };
    std::istringstream before(sample(2000, 4000));
    std::istringstream after (sample(2200, 4500));
    const auto disks0 = parse_diskstats_from(before);
    const auto disks1 = parse_diskstats_from(after);
    if (disks0.size() != 1 || disks0[0].device != "sda")
        return fail("parse_diskstats_from: expected one \"sda\" entry");

    const DiskRate r = disk_rate(disks0[0].sectors_read,    disks1[0].sectors_read,
                                 disks0[0].sectors_written, disks1[0].sectors_written, 2.0);
    // (2200-2000)*512/2 = 51200 B/s ; (4500-4000)*512/2 = 128000 B/s
    if (std::abs(r.read_bytes_per_sec - 51200.0) > 0.01)
        return fail("disk_rate(read): expected 51200 B/s, got " + std::to_string(r.read_bytes_per_sec));
    if (std::abs(r.write_bytes_per_sec - 128000.0) > 0.01)
        return fail("disk_rate(write): expected 128000 B/s, got " + std::to_string(r.write_bytes_per_sec));
    return ok();
}

Result check_fixture_network() {
    auto sample = [](unsigned long long rx, unsigned long long tx) {
        std::ostringstream ss;
        ss << "Inter-|   Receive                                                |  Transmit\n"
           << " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
           << "  eth0: " << rx << " 10 0 0 0 0 0 0 " << tx << " 20 0 0 0 0 0 0\n";
        return ss.str();
    };
    std::istringstream before(sample(1000, 2000));
    std::istringstream after (sample(1500, 2800));
    const auto nets0 = parse_netdev_from(before);
    const auto nets1 = parse_netdev_from(after);
    if (nets0.size() != 1 || nets0[0].interface != "eth0")
        return fail("parse_netdev_from: expected one \"eth0\" entry");

    const double rx_rate = net_rate_bytes(nets0[0].rx_bytes, nets1[0].rx_bytes, 2.0);
    const double tx_rate = net_rate_bytes(nets0[0].tx_bytes, nets1[0].tx_bytes, 2.0);
    if (std::abs(rx_rate - 250.0) > 0.01)
        return fail("net_rate_bytes(rx): expected 250 B/s, got " + std::to_string(rx_rate));
    if (std::abs(tx_rate - 400.0) > 0.01)
        return fail("net_rate_bytes(tx): expected 400 B/s, got " + std::to_string(tx_rate));
    return ok();
}

Result check_fixture_thermal() {
    const double c = millideg_to_celsius(45000);
    if (std::abs(c - 45.0) > 0.001)
        return fail("millideg_to_celsius(45000): expected 45.0, got " + std::to_string(c));
    return ok();
}

Result check_fixture_fans() {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = fs::temp_directory_path(ec) / ("vitals-selftest-fans-" + std::to_string(::getpid()));
    if (ec) return skip("no temporary directory");
    fs::remove_all(root, ec);

    auto put = [&](const char* dir, const char* file, const char* text) {
        fs::create_directories(root / dir);
        std::ofstream(root / dir / file) << text << "\n";
    };
    put("hwmon0", "name", "nct6687");
    put("hwmon0", "fan1_input", "698");
    put("hwmon0", "fan2_input", "0");
    put("hwmon0", "fan10_input", "5");          // fan10 and above are never listed
    put("hwmon1", "name", "it8688");
    put("hwmon1", "fan1_input", "1200");
    put("hwmon1", "fan1_label", "CPU_FAN");
    put("hwmon1", "fan2_input", "900");
    put("hwmon1", "fan2_label", "CPU_OPT");      // a repeated name falls back to the first free FAN<n>

    GpuInfo g;
    g.fan_pct = 45.0;
    const std::vector<FanInfo> fans = collect_fans({g}, root.string());
    fs::remove_all(root, ec);

    // GPU first, then the board's channels in order
    const char* names[] = {"GPU", "FAN1", "FAN2", "CPU", "FAN3"};
    const int   rpms[]  = {-1, 698, 0, 1200, 900};
    if (fans.size() != 5)
        return fail("collect_fans: expected 5 fans, got " + std::to_string(fans.size()));
    for (size_t i = 0; i < 5; ++i) {
        if (fans[i].name != names[i] || fans[i].rpm != rpms[i])
            return fail("collect_fans: entry " + std::to_string(i) + " expected " + names[i] + ", got " + fans[i].name);
    }
    if (fans[0].pct != 45) return fail("collect_fans: GPU fan expected 45 %");
    return ok();
}

Result check_fixture_hostname() {
    const systemuname s = systemuname_from(fake_utsname());
    if (s.hostname != "myhost")
        return fail("systemuname_from: expected hostname \"myhost\", got \"" + s.hostname + "\"");
    return ok();
}

Result check_fixture_kernel() {
    const systemuname s = systemuname_from(fake_utsname());
    if (s.kernel_release != "6.1.0-generic")
        return fail("systemuname_from: expected kernel_release \"6.1.0-generic\", got \"" + s.kernel_release + "\"");
    return ok();
}

Result check_fixture_loadavg() {
    std::istringstream in("0.50 0.30 0.10 2/300 1234\n");
    const loadavg l = parse_loadavg_from(in);
    if (std::abs(l.load1 - 0.50) > 0.001 || std::abs(l.load5 - 0.30) > 0.001 ||
        std::abs(l.load15 - 0.10) > 0.001)
        return fail("parse_loadavg_from: load1/5/15 parsed wrong");
    if (l.running_tasks != 2 || l.total_tasks != 300)
        return fail("parse_loadavg_from: running/total tasks parsed wrong");
    if (l.last_pid != 1234)
        return fail("parse_loadavg_from: last_pid parsed wrong");
    return ok();
}

Result check_fixture_uptime() {
    std::istringstream in("12345.67 8000.00\n");
    const uptime u = parse_uptime_from(in);
    if (std::abs(u.uptime_seconds - 12345.67) > 0.001)
        return fail("parse_uptime_from: uptime_seconds parsed wrong");
    if (std::abs(u.idle_seconds - 8000.00) > 0.001)
        return fail("parse_uptime_from: idle_seconds parsed wrong");
    return ok();
}
