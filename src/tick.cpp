#include "tick.h"
#include "sysinfo.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace {

void push_hist(std::deque<double>& h, double v) {
    h.push_back(v);
    if (static_cast<int>(h.size()) > HIST_CAP) h.pop_front();
}

}

void take_baseline() {
    sample("cpustat",   [] { G.prev_cpu  = parse_cpustat();   });
    sample("percpu",    [] { G.prev_core = parse_percpu();    });
    sample("netdev",    [] { G.prev_net  = parse_netdev();    });
    sample("diskstats", [] { G.prev_disk = parse_diskstats(); });
    G.t_prev = Clock::now();
}

void tick(Clock::time_point t_now, unsigned long long tick_count) {
    const auto tick_start = Clock::now();
    G.dt = std::chrono::duration<double>(t_now - G.t_prev).count();
    if (G.dt < 0.001) G.dt = 1.0;

    cpustat                cur_cpu{};
    std::vector<cpustat>   cur_core;
    std::vector<netdev>    cur_net;
    std::vector<diskstats> cur_disk;
    std::vector<thermal>   therm;
    std::vector<cpufreq>   freqs;
    std::vector<HwmonChip> hwmon;

    if (!sample("cpustat", [&] { cur_cpu = parse_cpustat(); })) cur_cpu = G.prev_cpu;
    sample("percpu",    [&] { cur_core = parse_percpu();    });
    sample("netdev",    [&] { cur_net  = parse_netdev();    });
    sample("diskstats", [&] { cur_disk = parse_diskstats(); });
    sample("thermal",   [&] { therm    = parse_thermal();   });
    sample("cpufreq",   [&] { freqs    = parse_cpufreq();   });
    sample("hwmon",     [&] { hwmon    = parse_hwmon();     });
    sample("gpu",       [&] { G.gpus   = parse_gpus();      });
    sample("fans",      [&] { G.fans   = collect_fans(G.gpus); });
    {   // one line whenever the list changes: what the Fans panel is given
        static std::string last_fans;
        std::string d;
        for (const auto& f : G.fans)
            d += (d.empty() ? "" : ", ") + f.name + "=" +
                 (f.rpm >= 0 ? std::to_string(f.rpm) + " RPM" : std::to_string(f.pct) + " %");
        if (d != last_fans) { LOG_INFO("fans", "%zu fans: %s", G.fans.size(), d.empty() ? "(none)" : d.c_str()); last_fans = d; }
    }
    sample("meminfo",   [&] { G.mem      = parse_meminfo();   });
    sample("snmp",      [&] { G.net_snmp = parse_snmp();      });
    sample("uptime",    [&] { G.up       = parse_uptime();    });
    sample("loadavg",   [&] { G.load     = parse_loadavg();   });
    if (!sample("statvfs", [&] { G.root_fs = parse_filesystemstat("/"); })) G.root_fs = {};

    G.panel_avail = panel_availability(G.mem, G.gpus, G.root_fs, cur_core, cur_net, cur_disk, therm, hwmon, G.fans);

    const std::string ip = get_local_ip();
    if (ip != G.local_ip) LOG_INFO("network", "local address: %s -> %s", G.local_ip.c_str(), ip.c_str());
    G.local_ip = ip;

    const double pct = cpu_delta(G.prev_cpu, cur_cpu);
    push_hist(G.cpu_hist, pct);

    // match cores by CPU number: hotplug shifts positions
    std::map<int, const cpustat*> prev_by_cpu;
    for (const auto& p : G.prev_core) prev_by_cpu[p.cpu] = &p;

    std::vector<double> core_pcts;
    std::vector<int>    core_ids;
    core_pcts.reserve(cur_core.size());
    core_ids.reserve(cur_core.size());
    for (const auto& c : cur_core) {
        auto it = prev_by_cpu.find(c.cpu);
        core_pcts.push_back(it != prev_by_cpu.end() ? cpu_delta(*it->second, c) : 0.0);
        core_ids.push_back(c.cpu);
    }
    G.core_ids = std::move(core_ids);

    // per-interface/device rates must use G.prev_* before the rollover below overwrites them
    std::map<std::string, double> net_rx_rate, net_tx_rate;
    double rx_now = 0.0, tx_now = 0.0;
    for (const auto& nd : cur_net) {
        if (is_hidden_iface(nd.interface)) continue;
        double rx = 0.0, tx = 0.0;
        for (const auto& p : G.prev_net) {
            if (p.interface != nd.interface) continue;
            rx = net_rate_bytes(p.rx_bytes, nd.rx_bytes, G.dt);
            tx = net_rate_bytes(p.tx_bytes, nd.tx_bytes, G.dt);
            break;
        }
        net_rx_rate[nd.interface] = rx;
        net_tx_rate[nd.interface] = tx;
        rx_now += rx;
        tx_now += tx;
    }
    G.peak_rx = std::max(G.peak_rx, rx_now);
    G.peak_tx = std::max(G.peak_tx, tx_now);

    push_hist(G.net_rx_hist, rx_now);
    push_hist(G.net_tx_hist, tx_now);

    double disk_rd_now = 0.0, disk_wr_now = 0.0;
    std::map<std::string, double> disk_rd_rate, disk_wr_rate;
    for (const auto& ds : cur_disk) {
        double rd = 0.0, wr = 0.0;
        for (const auto& p : G.prev_disk) {
            if (p.device != ds.device) continue;
            DiskRate r = disk_rate(p.sectors_read,    ds.sectors_read,
                                   p.sectors_written, ds.sectors_written, G.dt);
            rd = r.read_bytes_per_sec;
            wr = r.write_bytes_per_sec;
            break;
        }
        disk_rd_rate[ds.device] = rd;
        disk_wr_rate[ds.device] = wr;
        // whole disks only: partitions count in their parent
        const std::string& d = ds.device;
        if (is_virtual_disk(d) || !parent_of(d).empty()) continue;
        disk_rd_now += rd;
        disk_wr_now += wr;
    }

    push_hist(G.disk_rd_hist, disk_rd_now);
    push_hist(G.disk_wr_hist, disk_wr_now);

    G.last.cpu_pct   = pct;
    G.last.net       = cur_net;
    G.last.disk      = cur_disk;
    G.last.therm     = therm;
    G.last.freqs     = freqs;
    G.last.core_pcts = core_pcts;
    G.last.hwmon     = hwmon;
    G.last.net_rx    = std::move(net_rx_rate);
    G.last.net_tx    = std::move(net_tx_rate);
    G.last.disk_rd   = std::move(disk_rd_rate);
    G.last.disk_wr   = std::move(disk_wr_rate);

    G.prev_cpu  = cur_cpu;
    G.prev_core = std::move(cur_core);
    G.prev_net  = std::move(cur_net);
    G.prev_disk = std::move(cur_disk);
    G.t_prev    = t_now;

    const double took_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - tick_start).count();
    LOG_DEBUG("tick", "#%llu dt=%.3f took=%.1f ms cpu=%.1f%% mem=%.1f%% net rx=%.0f tx=%.0f B/s "
                      "disk rd=%.0f wr=%.0f B/s zones=%zu hwmon=%zu gpus=%zu",
              tick_count, G.dt, took_ms, pct, mem_used(G.mem).pct, rx_now, tx_now,
              disk_rd_now, disk_wr_now, G.last.therm.size(), G.last.hwmon.size(), G.gpus.size());
    if (took_ms > 250.0 || took_ms > G.opt.refresh_ms / 2.0)
        LOG_WARN("tick", "slow tick #%llu: %.1f ms (refresh %d ms)", tick_count, took_ms, G.opt.refresh_ms);
}
