#pragma once

#include <string>
#include <vector>

// /proc/stat
struct cpustat { // stat will conflict with POSIX API
    unsigned long long user;       // time spent running user processes
    unsigned long long nice;       // time spent running niced user processes
    unsigned long long system;     // time spent running kernel code
    unsigned long long idle;       // time spent idle
    unsigned long long iowait;     // time waiting for I/O operations
    unsigned long long irq;        // time servicing hardware interrupts
    unsigned long long softirq;    // time servicing software interrupts
    unsigned long long steal;      // time stolen by the hypervisor
    unsigned long long guest;      // time spent running guest virtual CPUs
    unsigned long long guest_nice; // time spent running niced guest virtual CPUs
};

// /proc/meminfo (values in kB)
struct meminfo {
    unsigned long long MemTotal;
    unsigned long long MemFree;
    unsigned long long MemAvailable;

    unsigned long long Buffers;
    unsigned long long Cached;

    unsigned long long SwapTotal;
    unsigned long long SwapFree;

    unsigned long long Active;
    unsigned long long Inactive;

    unsigned long long Dirty;
    unsigned long long Writeback;

    unsigned long long Slab;
    unsigned long long Shmem;
    unsigned long long SReclaimable; // only used as a MemAvailable fallback input
};

// /proc/uptime
struct uptime {
    double uptime_seconds;
    double idle_seconds;
};

// /proc/loadavg
struct loadavg {
    double load1;
    double load5;
    double load15;

    unsigned int running_tasks;
    unsigned int total_tasks;

    unsigned int last_pid;
};

// /proc/cpuinfo (first physical CPU block only)
struct cpuinfo {
    std::string model_name;

    int cpu_cores;
    int cpu_siblings;
};

// /proc/net/dev (one entry per interface)
struct netdev {
    std::string interface;

    std::vector<netdev> v_netdev;

    unsigned long long rx_bytes;
    unsigned long long rx_packets;
    unsigned long long rx_errors;
    unsigned long long rx_dropped;

    unsigned long long tx_bytes;
    unsigned long long tx_packets;
    unsigned long long tx_errors;
    unsigned long long tx_dropped;
};

// /proc/net/snmp
struct snmp {
    unsigned long long tcp_in_segments;
    unsigned long long tcp_out_segments;

    unsigned long long udp_in_datagrams;
    unsigned long long udp_out_datagrams;

    unsigned long long ip_in_receives;
    unsigned long long ip_out_requests;
};

// /proc/diskstats (one entry per block device)
struct diskstats {
    std::string device;

    std::vector<diskstats> v_diskstats;

    unsigned long long reads_completed;
    unsigned long long reads_merged;
    unsigned long long sectors_read;
    unsigned long long read_time_ms;

    unsigned long long writes_completed;
    unsigned long long writes_merged;
    unsigned long long sectors_written;
    unsigned long long write_time_ms;

    unsigned long long io_in_progress;
    unsigned long long io_time_ms;
};

// /sys/class/thermal/thermal_zone<N>/temp (one entry per zone)
struct thermal {
    int    zone;
    double temperature_celsius; // Raw sysfs value is millidegrees
};

// /sys/devices/system/cpu/cpu<N>/cpufreq/ (one entry per core)
struct cpufreq {
    int    cpu;
    double current_khz;

    double min_khz;
    double max_khz;
};

// statvfs()
struct filesystemstat { // statvfs will conflict with POSIX API
    unsigned long long total_bytes;
    unsigned long long used_bytes;
    unsigned long long free_bytes;

    unsigned long long total_inodes;
    unsigned long long free_inodes;
};

// uname()
struct systemuname { // uname will conflict with POSIX API
    std::string hostname;

    std::string kernel_name;
    std::string kernel_release;
    std::string kernel_version;

    std::string architecture;
};

cpustat                parse_cpustat();
meminfo                parse_meminfo();
uptime                 parse_uptime();
loadavg                parse_loadavg();
cpuinfo                parse_cpuinfo();
std::vector<netdev>    parse_netdev();
snmp                   parse_snmp();
std::vector<diskstats> parse_diskstats();
std::vector<thermal>   parse_thermal();
std::vector<cpufreq>   parse_cpufreq();
filesystemstat         parse_filesystemstat(const std::string& path = "/");
systemuname            parse_systemuname();
