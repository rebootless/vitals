#pragma once

#include <iosfwd>
#include <string>
#include <vector>

struct utsname; // <sys/utsname.h> — forward-declared to keep this header light

struct cpustat { // stat will conflict with POSIX API
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
    unsigned long long guest;
    unsigned long long guest_nice;
};

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

struct uptime {
    double uptime_seconds;
    double idle_seconds;
};

struct loadavg {
    double load1;
    double load5;
    double load15;

    unsigned int running_tasks;
    unsigned int total_tasks;

    unsigned int last_pid;
};

struct cpuinfo {
    std::string model_name;

    int cpu_cores;    // physical cores across all sockets
    int cpu_siblings; // logical CPUs (threads)
};

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

struct snmp {
    unsigned long long tcp_in_segments;
    unsigned long long tcp_out_segments;

    unsigned long long udp_in_datagrams;
    unsigned long long udp_out_datagrams;

    unsigned long long ip_in_receives;
    unsigned long long ip_out_requests;
};

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

struct thermal {
    int    zone;
    double temperature_celsius;
};

struct cpufreq {
    int    cpu;
    double current_khz;

    double min_khz;
    double max_khz;
};

struct filesystemstat { // statvfs will conflict with POSIX API
    unsigned long long total_bytes;
    unsigned long long used_bytes;
    unsigned long long free_bytes;

    unsigned long long total_inodes;
    unsigned long long free_inodes;
};

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

cpuinfo                parse_cpuinfo_from(std::istream& in); // model and thread count only
cpustat                parse_cpustat_from(std::istream& in);
meminfo                parse_meminfo_from(std::istream& in);
std::vector<netdev>    parse_netdev_from(std::istream& in);
std::vector<diskstats> parse_diskstats_from(std::istream& in);
uptime                 parse_uptime_from(std::istream& in);
loadavg                parse_loadavg_from(std::istream& in);
systemuname            systemuname_from(const struct utsname& u);

double millideg_to_celsius(unsigned long long raw_millidegrees);
