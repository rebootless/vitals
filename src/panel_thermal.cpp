#include "panels.h"

void panel_thermal(ncplane* n, int y, int x, int h, int w,
                   const std::vector<thermal>&   zones,
                   const std::vector<HwmonChip>& hwmon,
                   const std::vector<GpuInfo>&   gpus) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Thermal");
    if (ih <= 0 || iw <= 0) return;

    int row = iy;
    const int LBL_W  = 14;  // label column (incl. ": ")
    const int TEMP_W =  9;  // "[+99.0°C]" = 9 visible columns
    int bar_w = iw - LBL_W - TEMP_W - 2;

    // Determine trip limits
    double hi_limit   = 74.0;
    double crit_limit = 94.0;
    bool   found      = false;

    for (const auto& chip : hwmon) {
        for (const auto& s : chip.sensors) {
            if (s.has_crit && s.temp_crit > 0) {
                crit_limit = s.temp_crit;
                hi_limit   = s.has_max ? s.temp_max : crit_limit - 20.0;
                found = true; break;
            }
        }
        if (found) break;
    }
    if (!found && !zones.empty()) {
        auto [hi, crit] = thermal_trip_limits(zones[0].zone);
        hi_limit = hi; crit_limit = crit;
    }

    // Single-row renderer
    auto draw_temp_row = [&](int r, const std::string& label,
                             double temp, bool is_pkg,
                             double row_hi = -1.0, double row_crit = -1.0) {
        if (r >= iy + ih - 1) return;

        double use_hi   = row_hi   >= 0 ? row_hi   : hi_limit;
        double use_crit = row_crit >= 0 ? row_crit : crit_limit;

        double shown = std::min(99.0, std::max(0.0, temp));
        uint32_t tc  = (temp >= use_crit) ? theme().RED
                     : (temp >= use_hi)   ? theme().YELLOW
                                            : theme().GREEN;

        // Label: "%-12s: " -> always LBL_W columns
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "%-*s: ", LBL_W - 2,
                 str_trunc(label, LBL_W - 2).c_str());
        nc_set(n, is_pkg ? theme().MAUVE : theme().BLUE,
                  is_pkg ? NCSTYLE_BOLD : NCSTYLE_NONE);
        ncplane_putstr_yx(n, r, ix, lbl);

        int tx = ix + LBL_W;

        // [+99.0°C] — 7 visible columns of content inside brackets
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "+%.1f" DEG_C, shown);
        lbr(n, r, tx);
        nc_set(n, tc, NCSTYLE_BOLD);
        ncplane_putstr_yx(n, r, tx + 1, tbuf);
        rbr(n, r, tx + 8);

        // [bar]
        if (bar_w > 2) {
            lbr(n, r, tx + TEMP_W);
            draw_bar_grad(n, r, tx + TEMP_W + 1, bar_w,
                          shown / 99.0, GRAD_TEMP);
            rbr(n, r, tx + TEMP_W + 1 + bar_w);
        }
    };

    // Hwmon display
    if (!hwmon.empty()) {
        for (const auto& chip : hwmon) {
            if (row >= iy + ih - 1) break;

            // Package sensors (mauve bold)
            bool had_pkg = false;
            for (const auto& s : chip.sensors) {
                if (!s.is_package) continue;
                draw_temp_row(row++, s.label, s.temp_celsius, true);
                had_pkg = true;
            }
            if (had_pkg && row < iy + ih - 1)
                draw_sep(n, row++, ix, iw);

            // Core sensors — zero-pad numeric suffix ("Core 3" -> "Core 03")
            for (const auto& s : chip.sensors) {
                if (s.is_package || row >= iy + ih - 1) continue;
                std::string lbl = s.label;
                if (lbl.size() > 5 && lbl.substr(0, 5) == "Core ") {
                    std::string num = lbl.substr(5);
                    bool all_dig   = !num.empty();
                    for (char c : num)
                        if (!std::isdigit(static_cast<unsigned char>(c)))
                            { all_dig = false; break; }
                    if (all_dig) {
                        char fmt_lbl[16];
                        snprintf(fmt_lbl, sizeof(fmt_lbl), "Core %02d", std::stoi(num));
                        lbl = fmt_lbl;
                    }
                }
                draw_temp_row(row++, lbl, s.temp_celsius, false);
            }
        }

    // Thermal zone fallback
    } else if (!zones.empty()) {
        bool first = true;
        for (const auto& z : zones) {
            if (row >= iy + ih - 1) break;
            if (!first && row < iy + ih - 1) draw_sep(n, row++, ix, iw);
            first = false;
            if (row >= iy + ih - 1) break;
            draw_temp_row(row++, thermal_zone_type(z.zone), z.temperature_celsius, false);
        }

    } else {
        nc_set(n, theme().SURFACE2);
        ncplane_putstr_yx(n, row, ix, "No thermal data found");
        return;
    }

    // GPU temps — fixed thresholds (no crit/max sysfs node is broadly
    // available across AMD/Intel/NVIDIA the way it is for CPU hwmon).
    {
        bool any_gpu_temp = false;
        for (const auto& g : gpus) if (g.temp_c >= 0) { any_gpu_temp = true; break; }

        if (any_gpu_temp && row < iy + ih - 1) {
            draw_sep(n, row++, ix, iw);
            for (size_t gi = 0; gi < gpus.size(); ++gi) {
                if (gpus[gi].temp_c < 0 || row >= iy + ih - 1) continue;
                char lbl[16];
                snprintf(lbl, sizeof(lbl), "GPU %d", static_cast<int>(gi));
                draw_temp_row(row++, lbl, gpus[gi].temp_c, false, 75.0, 85.0);
            }
        }
    }

    // Footer: trip limits
    int frow = iy + ih - 1;
    if (frow < iy) return;

    char hibuf[32], cbuf[32];
    snprintf(hibuf, sizeof(hibuf), "+%.1f" DEG_C,
             std::min(99.0, std::max(0.0, hi_limit)));
    snprintf(cbuf,  sizeof(cbuf),  "+%.1f" DEG_C,
             std::min(99.0, std::max(0.0, crit_limit)));

    int cx = ix;
    nc_set(n, theme().OVERLAY0); ncplane_putstr_yx(n, frow, cx, "(");   cx++;
    nc_set(n, theme().BLUE);     ncplane_putstr_yx(n, frow, cx, "HIGH = "); cx += 7;
    nc_set(n, theme().PEACH, NCSTYLE_BOLD);
                              ncplane_putstr_yx(n, frow, cx, hibuf); cx += 7;
    nc_set(n, theme().OVERLAY0); ncplane_putstr_yx(n, frow, cx, ", ");  cx += 2;
    nc_set(n, theme().BLUE);     ncplane_putstr_yx(n, frow, cx, "CRIT = "); cx += 7;
    nc_set(n, theme().RED, NCSTYLE_BOLD);
                              ncplane_putstr_yx(n, frow, cx, cbuf);  cx += 7;
    nc_set(n, theme().OVERLAY0); ncplane_putstr_yx(n, frow, cx, ")");
}
