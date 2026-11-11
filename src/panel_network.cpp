#include "panels.h"

void panel_network(ncplane* n, int y, int x, int h, int w,
                   const std::vector<netdev>& cur_net,
                   const std::map<std::string, double>& rx_rates,
                   const std::map<std::string, double>& tx_rates) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Network");
    if (ih <= 0 || iw <= 0) return;

    const PairCols cols{14, 16, 32, false, true};
    int row = iy;

    auto header = [&](const char* name) {
        if (row >= iy + ih) return;
        draw_pair_header(n, row, ix, iw, cols, name, "RX", "TX");
        row++;
    };
    auto line = [&](const std::string& name, const std::string& rx, const std::string& tx) {
        if (row >= iy + ih) return;
        draw_pair_row(n, row, ix, iw, cols, name, rx, tx);
        row++;
    };
    auto sep = [&](int spare) {
        if (row + spare >= iy + ih) return;
        draw_sep(n, row, ix, iw);
        row++;
    };

    ull total_rx = 0, total_tx = 0;
    bool has_iface = false;
    for (const auto& nd : cur_net) {
        if (is_hidden_iface(nd.interface)) continue;
        has_iface = true;
        total_rx += nd.rx_bytes;
        total_tx += nd.tx_bytes;
    }

    header("Interfaces");
    if (!has_iface) line("N/A", "N/A", "N/A");

    for (const auto& nd : cur_net) {
        if (is_hidden_iface(nd.interface)) continue;
        line(nd.interface, fmt_net_rate(rate_of(rx_rates, nd.interface)),
                           fmt_net_rate(rate_of(tx_rates, nd.interface)));
    }

    sep(0);

    const auto& sn = G.net_snmp;
    header("Packets");
    line("TCP", std::to_string(sn.tcp_in_segments),  std::to_string(sn.tcp_out_segments));
    line("UDP", std::to_string(sn.udp_in_datagrams), std::to_string(sn.udp_out_datagrams));

    sep(0);

    header("Throughput");
    line("Total", fmt_bytes(total_rx), fmt_bytes(total_tx));
    line("Peak",  fmt_net_rate(G.peak_rx), fmt_net_rate(G.peak_tx));

    sep(1);

    if (row + 2 < iy + ih) {
        nc_set(n, theme().LABEL);
        ncplane_putstr_yx(n, row, ix, "History:");
        draw_history_pair(n, row + 1, ix, iw, 2, "RX", "TX", G.net_rx_hist, G.net_tx_hist);
    }
}
