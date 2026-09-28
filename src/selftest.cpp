#include "selftest.h"

#include "selftest_checks.h"
#include "selftest_fixtures.h"

#include <cstdio>
#include <exception>
#include <string>

// The 10-row table shared by all three sections:
//   Live:     does check_X() (selftest_checks.cpp) work against whatever
//             this system reports? (OK/SKIP/FAIL; optional hardware SKIPs
//             when absent)
//   Raw:      the literal input fed to the fixture below, so a human
//             reading top-to-bottom sees "here's the input" then "here's
//             what came out" for the exact same row.
//   Fixtures: check_fixture_X() (selftest_fixtures.cpp) — known input run
//             through the exact parse_*_from()/sysinfo.cpp/gpu.cpp
//             functions the panels use, asserting the exact processed
//             result.
//
// Live checks and fixture checks are two different kinds of test — one
// says "does this machine's data look sane", the other says "does our
// parsing/math produce the exact right number" — so they live in separate
// files (selftest_checks.cpp / selftest_fixtures.cpp); this file only
// wires the two together row-by-row and prints the result. One array is
// what keeps all three sections' row count and order identical by
// construction, rather than by hand-keeping three separate lists in sync.
//
// GPU/Thermal/Hostname/Kernel don't have a real /proc-format text file to
// fixture the way CPU/Memory/Network/Storage/Load avg/Uptime do (GPU is a
// multi-file sysfs walk, Thermal reads one bare number, uname() is a
// syscall) — their fixtures instead cover the one real, named, pure
// function each already factors out to (gpu_vendor_from_id,
// millideg_to_celsius, systemuname_from), the narrowest genuine seam
// available rather than an invented one.

namespace {

struct Metric {
    const char* name;
    const char* source;      // shown in Live:
    const char* raw_summary; // shown in Raw: — the literal input the Fixtures
                             // check for this row parses/computes from
    Result    (*live)();
    Result    (*fixture)();
};

const Metric METRICS[] = {
    {"CPU",      "/proc/stat",      "cpu  0 0 0 0 0 0 0 0 0 0  ->  cpu  50 0 0 50 0 0 0 0 0 0",
     check_cpu,      check_fixture_cpu},
    {"Memory",   "/proc/meminfo",   "MemTotal 10000000  MemFree 2000000  Buffers 100000  Cached 3000000  SReclaimable 100000",
     check_memory,   check_fixture_memory},
    {"GPU",      "sysfs vendor ID", "vendor = 0x1002",
     check_gpu,      check_fixture_gpu},
    {"Storage",  "/proc/diskstats", "sda sectors_read 2000->2200  sectors_written 4000->4500",
     check_storage,  check_fixture_storage},
    {"Network",  "/proc/net/dev",   "eth0 rx_bytes 1000->1500  tx_bytes 2000->2800",
     check_network,  check_fixture_network},
    {"Thermal",  "sysfs temp",      "temp = 45000 (millidegrees)",
     check_thermal,  check_fixture_thermal},
    {"Hostname", "uname",           "utsname.nodename = \"host-generic\"",
     check_hostname, check_fixture_hostname},
    {"Kernel",   "uname",           "utsname.release = \"6.1.0-generic\"",
     check_kernel,   check_fixture_kernel},
    {"Load avg", "/proc/loadavg",   "0.50 0.30 0.10 2/300 1234",
     check_loadavg,  check_fixture_loadavg},
    {"Uptime",   "/proc/uptime",    "12345.67 8000.00",
     check_uptime,   check_fixture_uptime},
};

const char* label(Status s) {
    switch (s) {
        case Status::Ok:   return "OK";
        case Status::Skip: return "SKIP";
        case Status::Fail: return "FAIL";
    }
    return "FAIL";
}

// Longest name ("Load avg") and source ("/proc/diskstats") in METRICS,
// +1 each for a trailing space before the next column.
constexpr int NAME_W   = 9;
constexpr int SOURCE_W = 18;

int run_live() {
    std::printf("Live:\n");
    int failures = 0;
    for (const Metric& m : METRICS) {
        Result r;
        try {
            r = m.live();
        } catch (const std::exception& e) {
            r = fail(e.what());
        } catch (...) {
            r = fail("unknown error");
        }
        if (r.status == Status::Fail) ++failures;

        const std::string source = std::string("(") + m.source + ")";
        std::printf("%-*s%-*s%s", NAME_W, m.name, SOURCE_W, source.c_str(), label(r.status));
        if (!r.reason.empty()) std::printf("  %s", r.reason.c_str());
        std::printf("\n");
    }
    return failures;
}

void run_raw() {
    std::printf("Raw:\n");
    for (const Metric& m : METRICS)
        std::printf("%-*s%s\n", NAME_W, m.name, m.raw_summary);
}

int run_fixtures() {
    std::printf("Fixtures:\n");
    int failures = 0;
    for (const Metric& m : METRICS) {
        Result r;
        try {
            r = m.fixture();
        } catch (const std::exception& e) {
            r = fail(e.what());
        } catch (...) {
            r = fail("unknown error");
        }
        if (r.status == Status::Fail) ++failures;

        std::printf("%-*s%s", NAME_W, m.name, label(r.status));
        if (!r.reason.empty()) std::printf("  %s", r.reason.c_str());
        std::printf("\n");
    }
    return failures;
}

} // namespace

int run_self_test() {
    int failures = run_live();
    std::printf("\n");
    run_raw();
    std::printf("\n");
    failures += run_fixtures();
    return failures ? 1 : 0;
}
