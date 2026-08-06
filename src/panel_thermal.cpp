#include "panels.h"

namespace {

// One renderable line: a label (already tree-prefixed if nested) plus its
// temperature and whether it should render in the "featured" mauve-bold
// style (currently just the CPU package reading, kept for continuity with
// the previous flat layout).
struct ThermItem {
    std::string label;
    double      temp;
    bool        featured;
    double      row_hi   = -1.0; // -1 = use the panel-wide hi/crit limits
    double      row_crit = -1.0;
};

struct ThermGroup {
    std::string             name; // e.g. "CPU", "Motherboard", "SSD"
    std::vector<ThermItem>  items;
};

void push_group(std::vector<ThermGroup>& groups, const std::string& name,
                std::vector<ThermItem> items) {
    if (!items.empty()) groups.push_back({name, std::move(items)});
}

} // namespace

void panel_thermal(ncplane* n, int y, int x, int h, int w,
                   const std::vector<thermal>&   zones,
                   const std::vector<HwmonChip>& hwmon,
                   const std::vector<GpuInfo>&   gpus) {

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Thermal");
    if (ih <= 0 || iw <= 0) return;

    int row = iy;
    const int LBL_W  = 11;  // label column (tight text + ':', no forced
                             // padding — was 14, moved 2 cols left so the
                             // temperature bracket/bar gain those 2 columns)
    const int TEMP_W =  9;  // "[+99.0°C]" = 9 visible columns
    int bar_w = iw - LBL_W - TEMP_W - 2;

    // Determine trip limits (shared by every group except GPU, which uses
    // its own fixed thresholds since crit/max sysfs nodes aren't reliably
    // exposed across AMD/Intel/NVIDIA the way they are for CPU hwmon).
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

    // Single-row renderer — draws "<label>[+NN.N°C][bar]" at row r.
    auto draw_temp_row = [&](int r, const std::string& label,
                             double temp, bool featured,
                             double row_hi, double row_crit) {
        if (r >= iy + ih - 1) return;

        double use_hi   = row_hi   >= 0 ? row_hi   : hi_limit;
        double use_crit = row_crit >= 0 ? row_crit : crit_limit;

        double shown = std::min(99.0, std::max(0.0, temp));
        uint32_t tc  = (temp >= use_crit) ? theme().RED
                     : (temp >= use_hi)   ? theme().YELLOW
                                            : theme().GREEN;

        char lbl[64];
        // Tight "<label>:" with no padding before the colon — the
        // bracket below sits at a fixed column regardless (ix + LBL_W),
        // so a short label like "GPU" just leaves blank space before the
        // bracket instead of padding filling that space with spaces
        // between the label text and its own colon.
        snprintf(lbl, sizeof(lbl), "%s:",
                 str_trunc(label, LBL_W - 1).c_str());
        // "Package" no longer gets a distinct color from the rest of the
        // CPU tree (Core 01, Core 02, ...) — same blue as every other row.
        (void)featured;
        nc_set(n, theme().BLUE, NCSTYLE_NONE);
        ncplane_putstr_yx(n, r, ix, lbl);

        int tx = ix + LBL_W;

        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "+%.1f%s", shown, deg_suffix());
        lbr(n, r, tx);
        nc_set(n, tc, NCSTYLE_BOLD);
        ncplane_putstr_yx(n, r, tx + 1, tbuf);
        rbr(n, r, tx + 8);

        if (bar_w > 2) {
            lbr(n, r, tx + TEMP_W);
            draw_bar_grad(n, r, tx + TEMP_W + 1, bar_w,
                          shown / 99.0, GRAD_TEMP);
            rbr(n, r, tx + TEMP_W + 1 + bar_w);
        }
    };

    // ---- Build groups -----------------------------------------------
    // Buckets by category so e.g. two Super-I/O motherboard chips, or a
    // coretemp + a discrete Tccd reading, land under one shared header
    // rather than two adjacent boxes with the same name.
    std::vector<ThermItem> cpu_items, mobo_items, storage_items,
                            network_items, laptop_items;
    std::map<std::string, std::vector<ThermItem>> other_items; // by chip name

    for (const auto& chip : hwmon) {
        std::vector<ThermItem>* bucket = nullptr;
        switch (chip.category) {
            case HwmonCategory::CPU:         bucket = &cpu_items;     break;
            case HwmonCategory::Motherboard: bucket = &mobo_items;    break;
            case HwmonCategory::Storage:     bucket = &storage_items; break;
            case HwmonCategory::Network:     bucket = &network_items; break;
            case HwmonCategory::Laptop:      bucket = &laptop_items;  break;
            default:                         bucket = &other_items[chip.name]; break;
        }
        for (const auto& s : chip.sensors) {
            // Zero-pad numeric core labels: "Core 3" -> "Core 03", so a
            // 10+ core CPU still lines up in the tree.
            std::string lbl = s.label;
            if (lbl.size() > 5 && lbl.substr(0, 5) == "Core ") {
                std::string num = lbl.substr(5);
                bool all_dig = !num.empty();
                for (char c : num)
                    if (!std::isdigit(static_cast<unsigned char>(c))) { all_dig = false; break; }
                if (all_dig) {
                    char fmt_lbl[16];
                    snprintf(fmt_lbl, sizeof(fmt_lbl), "Core %02d", std::stoi(num));
                    lbl = fmt_lbl;
                }
            }
            if (s.is_package) lbl = "Package";
            bucket->push_back({lbl, s.temp_celsius, s.is_package, -1.0, -1.0});
        }
    }

    // Thermal-zone fallback (no hwmon chips found at all) — each zone
    // becomes its own single-item "group" under its ACPI type name.
    std::vector<ThermItem> zone_items;
    if (hwmon.empty()) {
        for (const auto& z : zones)
            zone_items.push_back({thermal_zone_type(z.zone), z.temperature_celsius, false, -1.0, -1.0});
    }

    // gpus holds at most one entry (parse_gpus() only reports the primary
    // card — see gpu.h), so this is really "if a GPU was found", not a loop
    // in the multi-card sense.
    std::vector<ThermItem> gpu_items;
    if (!gpus.empty() && gpus.front().temp_c >= 0)
        gpu_items.push_back({"GPU", gpus.front().temp_c, false, 75.0, 85.0});

    std::vector<ThermGroup> groups;
    push_group(groups, "CPU",         cpu_items);
    push_group(groups, "Motherboard", mobo_items);
    push_group(groups, "Storage",     storage_items);
    push_group(groups, "Wi-Fi",       network_items);
    push_group(groups, "Laptop",      laptop_items);
    for (auto& [nm, items] : other_items) push_group(groups, nm, items);
    push_group(groups, "GPU",         gpu_items);
    for (auto& item : zone_items) push_group(groups, item.label, {item});

    if (groups.empty()) {
        nc_set(n, theme().SURFACE2);
        ncplane_putstr_yx(n, row, ix, "No thermal data found");
        return;
    }

    // ---- Render --------------------------------------------------------
    // A group with a single reading prints flat ("Motherboard  +34.0°C"),
    // matching how one number needs no tree around it. A group with
    // several readings gets a bold title row, then each item indented
    // with a tree connector (├─ for all but the last, └─ for the last).
    // CPU always gets its title row even with just one reading, since
    // "CPU" is worth stating outright rather than showing a bare sensor
    // name like "Tctl".
    bool first_group = true;
    for (const auto& g : groups) {
        if (row >= iy + ih - 1) break;
        if (!first_group) { if (row < iy + ih - 1) draw_sep(n, row++, ix, iw); }
        first_group = false;

        bool show_header = (g.items.size() > 1) || (g.name == "CPU");

        if (show_header) {
            if (row >= iy + ih - 1) break;
            nc_set(n, theme().MAUVE, NCSTYLE_BOLD);
            ncplane_putstr_yx(n, row, ix, g.name.c_str());
            row++;

            for (size_t i = 0; i < g.items.size() && row < iy + ih - 1; ++i) {
                bool last = (i + 1 == g.items.size());
                std::string prefix = last ? "\xe2\x94\x94\xe2\x94\x80"   // └─
                                          : "\xe2\x94\x9c\xe2\x94\x80";  // ├─
                const auto& it = g.items[i];
                draw_temp_row(row++, prefix + it.label, it.temp,
                             it.featured, it.row_hi, it.row_crit);
            }
        } else {
            const auto& it = g.items[0];
            draw_temp_row(row++, it.label, it.temp, it.featured,
                         it.row_hi, it.row_crit);
        }
    }

    // Footer: trip limits
    int frow = iy + ih - 1;
    if (frow < iy) return;

    char hibuf[32], cbuf[32];
    snprintf(hibuf, sizeof(hibuf), "+%.1f%s",
             std::min(99.0, std::max(0.0, hi_limit)), deg_suffix());
    snprintf(cbuf,  sizeof(cbuf),  "+%.1f%s",
             std::min(99.0, std::max(0.0, crit_limit)), deg_suffix());

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
