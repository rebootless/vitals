#pragma once

#include "state.h"

// CPU helpers
std::vector<cpustat> parse_percpu();
double               cpu_delta(const cpustat& prev, const cpustat& cur);

// Network
std::string get_local_ip();
// True for interfaces the Network panel hides: loopback plus virtual/container
// plumbing (ifb*, veth*, docker*, br-*, virbr*, vnet*). VPN links stay visible.
bool is_hidden_iface(const std::string& name);

// Thermal zones
std::string           thermal_zone_type  (int zone);
std::pair<double,double> thermal_trip_limits(int zone); // returns {hi°C, crit°C}

// Hwmon (coretemp / k10temp / …)
std::vector<HwmonChip> parse_hwmon();

// Disk
// Returns the parent device name (e.g. "sda" for "sda1"), or "" if root device.
std::string parent_of(const std::string& dev);
