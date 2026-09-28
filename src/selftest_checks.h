#pragma once

#include "selftest_internal.h"

// Live checks — read the same sources the panels do, against whatever this
// system actually reports. A parser that throws counts as FAIL (caught by
// the caller); optional hardware (GPU, thermal sensors) reports SKIP when
// absent. One per METRICS row (see selftest.cpp) — contrast with
// selftest_fixtures.h, which asserts an exact known result instead.

Result check_cpu();
Result check_memory();
Result check_gpu();
Result check_storage();
Result check_network();
Result check_thermal();
Result check_hostname();
Result check_kernel();
Result check_loadavg();
Result check_uptime();
