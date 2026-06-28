#pragma once

#include "state.h"

std::vector<cpustat> parse_percpu();
double               cpu_delta(const cpustat& prev, const cpustat& cur);

std::string get_local_ip();

std::string           thermal_zone_type  (int zone);
std::pair<double,double> thermal_trip_limits(int zone);

std::vector<HwmonChip> parse_hwmon();

std::string parent_of(const std::string& dev);
