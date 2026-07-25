#include "panels.h"

void panel_network(ncplane* n, int y, int x, int h, int w,
                   const std::vector<netdev>& cur_net) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Network", "q:Quit Esc:Settings");
    if (ih <= 0 || iw <= 0) return;

    int row = iy;
    const int C1 = ix;
    const int C2 = ix + 16;
    const int C3 = ix + 32;

    // Section 1: Interfaces
    if (row < iy + ih) {
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, row, C1, "%-14s", "Interfaces");
        if (C2 < ix + iw) {
            nc_set(n, theme().PEACH);
            ncplane_printf_yx(n, row, C2, "%-14s", "RX");
        }
        if (C3 < ix + iw) {
            nc_set(n, theme().TEAL, NCSTYLE_BOLD);
            ncplane_printf_yx(n, row, C3, "%-14s", "TX");
        }
        row++;
    }

    ull total_rx = 0, total_tx = 0;

    for (const auto& nd : cur_net) {
        if (nd.interface == "lo") continue;
        double rx = 0, tx = 0;
        for (const auto& p : G.prev_net) {
            if (p.interface != nd.interface) continue;
            if (nd.rx_bytes >= p.rx_bytes)
                rx = static_cast<double>(nd.rx_bytes - p.rx_bytes) / G.dt;
            if (nd.tx_bytes >= p.tx_bytes)
                tx = static_cast<double>(nd.tx_bytes - p.tx_bytes) / G.dt;
            break;
        }
        total_rx += nd.rx_bytes;
        total_tx += nd.tx_bytes;

        if (row >= iy + ih) break;
        nc_set(n, theme().TEXT);
        ncplane_printf_yx(n, row, C1, "%-14s",
                          str_trunc(nd.interface, 14).c_str());
        if (C2 < ix + iw) {
            nc_set(n, theme().PEACH);
            ncplane_putstr_yx(n, row, C2, (std::string(glyph_down()) + " ").c_str());
            nc_set(n, theme().TEXT);
            ncplane_printf_yx(n, row, C2 + 2, "%-12s",
                              str_trunc(fmt_net_rate(rx), 12).c_str());
        }
        if (C3 < ix + iw) {
            nc_set(n, theme().TEAL, NCSTYLE_BOLD);
            ncplane_putstr_yx(n, row, C3, (std::string(glyph_up()) + " ").c_str());
            nc_set(n, theme().TEXT);
            ncplane_printf_yx(n, row, C3 + 2, "%-12s",
                              str_trunc(fmt_net_rate(tx), 12).c_str());
        }
        row++;
    }

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    // Section 2: Packets
    auto sn = parse_snmp();

    if (row < iy + ih) {
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, row, C1, "%-14s", "Packets");
        if (C2 < ix + iw) { nc_set(n, theme().PEACH);              ncplane_printf_yx(n, row, C2, "%-14s", "RX"); }
        if (C3 < ix + iw) { nc_set(n, theme().TEAL, NCSTYLE_BOLD); ncplane_printf_yx(n, row, C3, "%-14s", "TX"); }
        row++;
    }

    auto pkt_row = [&](const char* name, ull rx_v, ull tx_v) {
        if (row >= iy + ih) return;
        nc_set(n, theme().TEXT); ncplane_printf_yx(n, row, C1, "%-14s", name);
        if (C2 < ix + iw) {
            nc_set(n, theme().PEACH);              ncplane_putstr_yx(n,  row, C2,     (std::string(glyph_down()) + " ").c_str());
            nc_set(n, theme().TEXT);               ncplane_printf_yx(n,  row, C2 + 2, "%-12llu", rx_v);
        }
        if (C3 < ix + iw) {
            nc_set(n, theme().TEAL, NCSTYLE_BOLD); ncplane_putstr_yx(n, row, C3,     (std::string(glyph_up()) + " ").c_str());
            nc_set(n, theme().TEXT);               ncplane_printf_yx(n,  row, C3 + 2, "%-12llu", tx_v);
        }
        row++;
    };
    pkt_row("TCP", sn.tcp_in_segments,  sn.tcp_out_segments);
    pkt_row("UDP", sn.udp_in_datagrams, sn.udp_out_datagrams);

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    // Section 3: Throughput
    if (row < iy + ih) {
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, row, C1, "%-14s", "Throughput");
        if (C2 < ix + iw) { nc_set(n, theme().PEACH);              ncplane_printf_yx(n, row, C2, "%-14s", "RX"); }
        if (C3 < ix + iw) { nc_set(n, theme().TEAL, NCSTYLE_BOLD); ncplane_printf_yx(n, row, C3, "%-14s", "TX"); }
        row++;
    }

    // Total cumulative bytes
    if (row < iy + ih) {
        nc_set(n, theme().TEXT); ncplane_printf_yx(n, row, C1, "%-14s", "Total");
        if (C2 < ix + iw) {
            nc_set(n, theme().PEACH);              ncplane_putstr_yx(n, row, C2,     (std::string(glyph_down()) + " ").c_str());
            nc_set(n, theme().TEXT);               ncplane_printf_yx(n, row, C2 + 2, "%-12s", fmt_bytes(total_rx).c_str());
        }
        if (C3 < ix + iw) {
            nc_set(n, theme().TEAL, NCSTYLE_BOLD); ncplane_putstr_yx(n, row, C3,     (std::string(glyph_up()) + " ").c_str());
            nc_set(n, theme().TEXT);               ncplane_printf_yx(n, row, C3 + 2, "%-12s", fmt_bytes(total_tx).c_str());
        }
        row++;
    }

    // Peak rates
    if (row < iy + ih) {
        nc_set(n, theme().TEXT); ncplane_printf_yx(n, row, C1, "%-14s", "Peak");
        if (C2 < ix + iw) {
            nc_set(n, theme().PEACH);              ncplane_putstr_yx(n, row, C2,     (std::string(glyph_down()) + " ").c_str());
            nc_set(n, theme().TEXT);               ncplane_printf_yx(n, row, C2 + 2, "%-12s", fmt_net_rate(G.peak_rx).c_str());
        }
        if (C3 < ix + iw) {
            nc_set(n, theme().TEAL, NCSTYLE_BOLD); ncplane_putstr_yx(n, row, C3,     (std::string(glyph_up()) + " ").c_str());
            nc_set(n, theme().TEXT);               ncplane_printf_yx(n, row, C3 + 2, "%-12s", fmt_net_rate(G.peak_tx).c_str());
        }
        row++;
    }
}
