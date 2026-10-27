#pragma once

#include "state.h"

std::vector<cpustat> parse_percpu();
double               cpu_delta(const cpustat& prev, const cpustat& cur);

struct MemUsed { ull used_kib; double pct; };
MemUsed mem_used(const meminfo& mi);

// bytes/sec between two samples dt seconds apart; 0 on counter reset or dt <= 0
double net_rate_bytes(ull prev_bytes, ull cur_bytes, double dt);

struct DiskRate { double read_bytes_per_sec; double write_bytes_per_sec; };
DiskRate disk_rate(ull prev_sectors_read,  ull cur_sectors_read,
                   ull prev_sectors_written, ull cur_sectors_written, double dt);

std::string get_local_ip();
// loopback, VLANs and virtual plumbing (bridges, bonds, tap/veth/ifb, docker, libvirt, Proxmox fw*); VPN links stay visible
bool is_hidden_iface(const std::string& name);
// dm-/loop/sr/ram/zram/md/nbd/zd: their I/O is already counted on the underlying devices
bool is_virtual_disk(const std::string& dev);

std::string           thermal_zone_type  (int zone);
std::pair<double,double> thermal_trip_limits(int zone);

std::vector<HwmonChip> parse_hwmon();

std::string parent_of(const std::string& dev);
