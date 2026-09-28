#pragma once

#include "selftest_internal.h"

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
