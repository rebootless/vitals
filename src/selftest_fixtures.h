#pragma once

#include "selftest_internal.h"

// Fixture checks — feed known raw /proc-format text (or, for GPU/Thermal/
// Hostname/Kernel, the narrowest real pure function each already factors
// out to) through the exact parse_*_from()/sysinfo.cpp/gpu.cpp code the
// panels use, and assert the exact processed result. Unlike
// selftest_checks.h, these never depend on what this particular machine
// happens to report, so they catch a regression in the parsing or math
// itself. One per METRICS row (see selftest.cpp).

Result check_fixture_cpu();
Result check_fixture_memory();
Result check_fixture_gpu();
Result check_fixture_storage();
Result check_fixture_network();
Result check_fixture_thermal();
Result check_fixture_hostname();
Result check_fixture_kernel();
Result check_fixture_loadavg();
Result check_fixture_uptime();
