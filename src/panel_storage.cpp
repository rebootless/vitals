#include "panels.h"

void panel_storage(ncplane* n, int y, int x, int h, int w,
                   const std::vector<diskstats>& cur_disk) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Storage");
    if (ih <= 0 || iw <= 0) return;

    int row = iy;
    const int C1 = ix;
    const int C2 = ix + 15;
    const int C3 = ix + 31;

    // Root filesystem: [pct%][bar] / used/total
    try {
        auto fs = parse_filesystemstat("/");
        if (fs.total_bytes > 0 && row < iy + ih) {
            double fpct = 100.0 * static_cast<double>(fs.used_bytes) / fs.total_bytes;
            uint32_t fc = pct_color(fpct);

            nc_set(n, theme().BLUE);
            ncplane_putstr_yx(n, row, ix, "Root: ");
            lbr(n, row, ix + 6);
            nc_set(n, fc, NCSTYLE_BOLD);
            ncplane_printf_yx(n, row, ix + 7, "%3.0f%%", fpct);
            rbr(n, row, ix + 11);

            int bx = ix + 12, bw = iw - 14;
            if (bw > 2) {
                lbr(n, row, bx);
                draw_bar_grad(n, row, bx + 1, bw, fpct / 100.0, GRAD_STORAGE);
                rbr(n, row, bx + 1 + bw);
            }
            row++;
        }
        if (fs.total_bytes > 0 && row < iy + ih) {
            nc_set(n, theme().TEXT);
            std::string s = fmt_bytes(fs.used_bytes) + " / " + fmt_bytes(fs.total_bytes);
            ncplane_printf_yx(n, row, ix + 6, "%s", str_trunc(s, iw - 6).c_str());
            row++;
        }
    } catch (...) {}

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    // Header
    if (row < iy + ih) {
        nc_set(n, theme().BLUE);
        ncplane_printf_yx(n, row, C1, "%-13s", "Device");
        if (C2 < ix + iw) { nc_set(n, theme().PEACH, NCSTYLE_BOLD); ncplane_printf_yx(n, row, C2, "%-14s", "Write/s"); }
        if (C3 < ix + iw) { nc_set(n, theme().TEAL);                 ncplane_printf_yx(n, row, C3, "%-14s", "Read/s");  }
        row++;
    }

    // Skip virtual / optical / ram devices
    auto skip = [](const std::string& d) {
        if (d.size() >= 3 && d.substr(0, 3) == "dm-")   return true;
        if (d.size() >= 4 && d.substr(0, 4) == "loop")  return true;
        if (d.size() >= 2 && d.substr(0, 2) == "sr")    return true;
        if (d.size() >= 3 && d.substr(0, 3) == "ram")   return true;
        return false;
    };

    // Compute read/write rate for one device
    auto rate = [&](const diskstats& ds) -> std::pair<double, double> {
        double rd = 0, wr = 0;
        for (const auto& p : G.prev_disk) {
            if (p.device != ds.device) continue;
            if (ds.sectors_read     >= p.sectors_read)
                rd = static_cast<double>(ds.sectors_read    - p.sectors_read)    * 512.0 / G.dt;
            if (ds.sectors_written  >= p.sectors_written)
                wr = static_cast<double>(ds.sectors_written - p.sectors_written) * 512.0 / G.dt;
            break;
        }
        return {rd, wr};
    };

    // Render one device row
    auto disk_row = [&](const std::string& prefix, const diskstats& ds) {
        if (row >= iy + ih) return;
        auto [rd, wr] = rate(ds);
        std::string name = prefix + str_trunc(ds.device,
                                              13 - static_cast<int>(prefix.size()));
        nc_set(n, theme().TEXT);
        ncplane_printf_yx(n, row, C1, "%-13s", name.c_str());
        if (C2 < ix + iw) {
            nc_set(n, theme().PEACH, NCSTYLE_BOLD);
            ncplane_putstr_yx(n, row, C2, (std::string(glyph_down()) + " ").c_str());
            nc_set(n, theme().TEXT);
            ncplane_printf_yx(n, row, C2 + 2, "%-12s", fmt_io_rate(wr).c_str());
        }
        if (C3 < ix + iw) {
            nc_set(n, theme().TEAL);
            ncplane_putstr_yx(n, row, C3, (std::string(glyph_up()) + " ").c_str());
            nc_set(n, theme().TEXT);
            ncplane_printf_yx(n, row, C3 + 2, "%-12s", fmt_io_rate(rd).c_str());
        }
        row++;
    };

    // Build root device list (no parent)
    std::vector<const diskstats*> roots;
    for (const auto& ds : cur_disk) {
        if (skip(ds.device)) continue;
        if (parent_of(ds.device).empty()) roots.push_back(&ds);
    }

    // Render tree: root → partitions
    for (const auto* root : roots) {
        if (row >= iy + ih) break;
        disk_row("", *root);

        std::vector<const diskstats*> children;
        for (const auto& ds : cur_disk) {
            if (skip(ds.device)) continue;
            if (parent_of(ds.device) == root->device) children.push_back(&ds);
        }
        for (size_t ci = 0; ci < children.size(); ++ci) {
            if (row >= iy + ih) break;
            bool last = (ci == children.size() - 1);
            disk_row(last ? "\xe2\x94\x94\xe2\x94\x80"   // └─
                          : "\xe2\x94\x9c\xe2\x94\x80",  // ├─
                     *children[ci]);
        }
    }
}
