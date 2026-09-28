#include "selftest.h"

#include "selftest_checks.h"
#include "selftest_fixtures.h"

#include <cstdio>
#include <exception>
#include <string>

namespace {

struct Metric {
    const char* name;
    const char* source;
    const char* raw_summary;
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

}

int run_self_test() {
    int failures = run_live();
    std::printf("\n");
    run_raw();
    std::printf("\n");
    failures += run_fixtures();
    return failures ? 1 : 0;
}
