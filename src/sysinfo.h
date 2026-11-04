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

// IPv4 of the default-route interface, else of the first up, non-virtual one; "N/A" if none
std::string get_local_ip();
// interface of the lowest-metric default route in /proc/net/route format, "" if none
std::string default_route_iface(std::istream& in);
// loopback, VLANs and virtual plumbing (bridges, bonds, tap/veth/ifb, docker, libvirt, Proxmox fw*); VPN links stay visible
bool is_hidden_iface(const std::string& name);
// dm-/loop/sr/ram/zram/md/nbd/zd: their I/O is already counted on the underlying devices
bool is_virtual_disk(const std::string& dev);

// {high, crit} from the sensor's own max/crit; {-1, -1} when it reports none
std::pair<double,double> hwmon_limits(const HwmonSensor& s);

std::string           thermal_zone_type  (int zone);
std::pair<double,double> thermal_trip_limits(int zone);

std::vector<HwmonChip> parse_hwmon();

constexpr int FAN_NAME_W = 4;  // longest short name: CPU, GPU, PSU, FAN1
constexpr int FAN_MAX    = 6;  // the panel shows at most this many

// "CPU_FAN" -> "CPU", "Chassis Fan 2" -> "SYS"; "" when the label says nothing useful
std::string fan_short_name(const std::string& label);
// hwmon fanN_input in sensor order, then the NVIDIA fan; at most FAN_MAX entries with unique names.
// GPU first, then the board's channels fan1..fan9 (0 RPM included; fan10+ ignored); unlabeled ones are named FAN<channel>.
std::vector<FanInfo> collect_fans(const std::vector<GpuInfo>& gpus,
                                  const std::string& hwmon_root = "/sys/class/hwmon");

std::string parent_of(const std::string& dev);

// per panel: is there anything to show (auto panels appear and disappear with it)
PanelFlags panel_availability(const meminfo& mem, const std::vector<GpuInfo>& gpus,
                              const filesystemstat& root_fs, const std::vector<cpustat>& cores,
                              const std::vector<netdev>& net, const std::vector<diskstats>& disks,
                              const std::vector<thermal>& zones, const std::vector<HwmonChip>& hwmon,
                              const std::vector<FanInfo>& fans);
