#pragma once

#include "common.h"

std::string fmt_bytes    (ull bytes);
std::string fmt_mem_kib  (ull kib);
std::string fmt_io_rate  (double bytes_per_sec);
std::string fmt_net_rate (double bytes_per_sec);
std::string fmt_uptime   (double secs);
std::string fmt_time     ();
