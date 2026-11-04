#include "panels.h"

// nct6683 doesn't load on boot, so I have to run this manually to display fan speeds
// sudo modprobe nct6683

// autostart:
// echo nct6683 | sudo tee /etc/modules-load.d/nct6683.conf && sudo systemctl restart systemd-modules-load && lsmod | grep nct6683

namespace {
// "NAME:  [XXXX RPM]": name padded to FAN_NAME_W + 2, then a 10-column value box
constexpr int ITEM_W = FAN_NAME_W + 2 + 10;
constexpr int GAP    = 1;
constexpr int ROWS   = 3;
}

void panel_fan(ncplane* n, int y, int x, int h, int w) {
    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Fans");
    if (ih <= 0 || iw <= 0) return;

    const std::vector<FanInfo>& fans = G.fans;

    if (fans.empty()) {
        nc_set(n, theme().LABEL);
        ncplane_printf_yx(n, iy, ix, "%-9s", "Fans:");
        nc_set(n, theme().BORDER);
        ncplane_putstr_yx(n, iy, ix + 9, str_trunc("N/A", iw - 9).c_str());
        return;
    }

    // as many columns as fit (1 below 33 inner columns, 2 up to 49, 3 up to 66, ...), same item width
    const int ncol = std::max(1, (iw + GAP) / (ITEM_W + GAP));
    const int slot = ITEM_W + GAP; // compact: extra width stays empty on the right
    const int cap  = std::min({ static_cast<int>(fans.size()), ncol * ROWS, ncol * ih });

    for (int i = 0; i < cap; ++i) {
        const FanInfo& f = fans[static_cast<size_t>(i)];
        const int r  = iy + i / ncol;
        const int cx = ix + (i % ncol) * slot;
        if (cx + ITEM_W > ix + iw) continue;

        nc_set(n, theme().LABEL);
        ncplane_printf_yx(n, r, cx, "%-*s", FAN_NAME_W + 2, (f.name + ":").c_str());

        const int bx = cx + FAN_NAME_W + 2;
        lbr(n, r, bx);
        nc_set(n, theme().TITLE);
        if (f.rpm >= 0)      ncplane_printf_yx(n, r, bx + 1, "%4d RPM", f.rpm);
        else if (f.pct >= 0) ncplane_printf_yx(n, r, bx + 1, "%4d %%  ", f.pct);
        else { nc_set(n, theme().BORDER); ncplane_printf_yx(n, r, bx + 1, "%8s", "N/A"); }
        rbr(n, r, bx + 9);
    }
}
