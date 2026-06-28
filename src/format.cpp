#include "format.h"

#include <cstdio>

std::string fmt_bytes(ull bytes) {
    char b[32];
    if      (bytes < 1024ULL)                          snprintf(b, sizeof(b), "%llu B",    bytes);
    else if (bytes < 1024ULL * 1024ULL)                snprintf(b, sizeof(b), "%.1f KiB",  bytes / 1024.0);
    else if (bytes < 1024ULL * 1024ULL * 1024ULL)      snprintf(b, sizeof(b), "%.2f MiB",  bytes / (1024.0 * 1024.0));
    else                                               snprintf(b, sizeof(b), "%.2f GiB",  bytes / (1024.0 * 1024.0 * 1024.0));
    return b;
}

std::string fmt_mem_kib(ull kib) {
    return fmt_bytes(kib * 1024ULL);
}

std::string fmt_io_rate(double bps) {
    char b[32];
    if (bps < 0) bps = 0;
    if      (bps < 1024.0)                  snprintf(b, sizeof(b), "%.0f B/s",    bps);
    else if (bps < 1024.0 * 1024.0)         snprintf(b, sizeof(b), "%.1f KiB/s",  bps / 1024.0);
    else if (bps < 1024.0 * 1024.0 * 1024.0)
                                             snprintf(b, sizeof(b), "%.2f MiB/s",  bps / (1024.0 * 1024.0));
    else                                     snprintf(b, sizeof(b), "%.2f GiB/s",  bps / (1024.0 * 1024.0 * 1024.0));
    return b;
}

std::string fmt_net_rate(double bps) {
    char b[32];
    if (bps < 0) bps = 0;
    double mbps = (bps * 8.0) / 1'000'000.0;
    if (mbps < 1000.0) snprintf(b, sizeof(b), "%.1f Mbps", mbps);
    else               snprintf(b, sizeof(b), "%.1f Gbps", mbps / 1000.0);
    return b;
}

std::string fmt_uptime(double secs) {
    int s = static_cast<int>(secs);
    int d = s / 86400; s %= 86400;
    int h = s / 3600;  s %= 3600;
    int m = s / 60;    s %= 60;
    char b[32];
    if (d > 0) snprintf(b, sizeof(b), "%dd %02dh %02dm", d, h, m);
    else       snprintf(b, sizeof(b), "%02d:%02d:%02d",  h, m, s);
    return b;
}

std::string fmt_time() {
    char b[16]; time_t t = time(nullptr);
    strftime(b, sizeof(b), "%H:%M:%S", localtime(&t));
    return b;
}
