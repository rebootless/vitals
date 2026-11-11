#include "panels.h"

void panel_storage(ncplane* n, int y, int x, int h, int w,
                   const std::vector<diskstats>& cur_disk,
                   const std::map<std::string, double>& rd_rates,
                   const std::map<std::string, double>& wr_rates) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Storage");
    if (ih <= 0 || iw <= 0) return;

    const PairCols cols{13, 15, 31, true, false};
    int row = iy;

    const auto& fs = G.root_fs;
    const bool has_fs = fs.total_bytes > 0;
    if (row < iy + ih) {
        // same basis as df: the root-reserved blocks are neither used nor available
        const double basis = static_cast<double>(fs.used_bytes + fs.avail_bytes);
        double fpct = basis > 0 ? 100.0 * static_cast<double>(fs.used_bytes) / basis : 0.0;

        draw_pct_row(n, row, ix, iw, 6, "Root:", fpct, has_fs, pct_color(fpct), GRAD_STORAGE);
        row++;
    }
    if (row < iy + ih) {
        nc_set(n, has_fs ? theme().FG : theme().BORDER);
        std::string s = has_fs ? fmt_bytes(fs.used_bytes) + " / " + fmt_bytes(fs.total_bytes) : "N/A";
        ncplane_printf_yx(n, row, ix + 6, "%s", str_trunc(s, iw - 6).c_str());
        row++;
    }

    if (row < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    if (row < iy + ih) {
        draw_pair_header(n, row, ix, iw, cols, "Device", "Write/s", "Read/s");
        row++;
    }

    auto disk_line = [&](const std::string& name, const std::string& wr_text, const std::string& rd_text) {
        if (row >= iy + ih) return;
        draw_pair_row(n, row, ix, iw, cols, name, wr_text, rd_text);
        row++;
    };

    auto disk_row = [&](const std::string& prefix, const diskstats& ds) {
        const int prefix_cols = prefix.empty() ? 0 : 2;
        disk_line(prefix + str_trunc(ds.device, 13 - prefix_cols),
                  fmt_io_rate(rate_of(wr_rates, ds.device)), fmt_io_rate(rate_of(rd_rates, ds.device)));
    };

    std::vector<const diskstats*> roots;
    for (const auto& ds : cur_disk) {
        if (is_virtual_disk(ds.device)) continue;
        if (parent_of(ds.device).empty()) roots.push_back(&ds);
    }
    if (roots.empty()) disk_line("N/A", "N/A", "N/A");

    for (const auto* root : roots) {
        if (row >= iy + ih) break;
        disk_row("", *root);

        std::vector<const diskstats*> children;
        for (const auto& ds : cur_disk) {
            if (is_virtual_disk(ds.device)) continue;
            if (parent_of(ds.device) == root->device) children.push_back(&ds);
        }
        for (size_t ci = 0; ci < children.size(); ++ci) {
            if (row >= iy + ih) break;
            bool last = (ci == children.size() - 1);
            disk_row(last ? "\xe2\x94\x94\xe2\x94\x80"
                          : "\xe2\x94\x9c\xe2\x94\x80",
                     *children[ci]);
        }
    }

    if (row + 1 < iy + ih) { draw_sep(n, row, ix, iw); row++; }

    if (row + 2 < iy + ih) {
        nc_set(n, theme().LABEL);
        ncplane_putstr_yx(n, row, ix, "History:");
        draw_history_pair(n, row + 1, ix, iw, 5, "Write", "Read", G.disk_wr_hist, G.disk_rd_hist);
    }
}
