#include "layout.h"

#include <algorithm>
#include <numeric>
#include <utility>

namespace {

struct Cell { int w; std::vector<PanelId> panels; }; // panels stacked top to bottom
struct Row  {
    int h; std::vector<Cell> cells;
    int fixed_h = 0;               // > 0: exact height, h is ignored
    std::vector<PanelId> fan_over{}; // Fans sit under these panels and span their full width
};

constexpr int FAN_H   = 5; // box + 3 rows of fans
constexpr int MEM_MIN = 5; // Memory keeps at least this much above Fans

using Template = std::vector<Row>;

const Template WIDE = {
    // I caught myself thinking that I just don't know what to add to the planned panel under panel_thermal,
    // so I just made panel_fan bigger
    { 3, { { 2, { P_CPU, P_GPU } }, { 1, { P_MEMORY } }, { 1, { P_THERMAL } } }, 0, { P_MEMORY, P_THERMAL } },
    { 2, { { 1, { P_NETWORK } },    { 1, { P_STORAGE } } } },
};

const Template MEDIUM = {
    { 2, { { 1, { P_CPU, P_GPU } }, { 1, { P_MEMORY, P_FAN } } } },
    { 2, { { 1, { P_NETWORK } },    { 1, { P_STORAGE } } } },
    { 1, { { 1, { P_THERMAL } } } },
};

const Template NARROW = {
    { 1, { { 1, { P_CPU } } } },
    { 1, { { 1, { P_GPU } } } },
    { 1, { { 1, { P_MEMORY } } } },
    { 1, { { 1, { P_FAN } } }, FAN_H },
    { 1, { { 1, { P_NETWORK } } } },
    { 1, { { 1, { P_STORAGE } } } },
    { 1, { { 1, { P_THERMAL } } } },
};

// each share is rounded down, the last one takes the remainder
std::vector<int> split_floor(int total, const std::vector<int>& weights) {
    const int sum = std::accumulate(weights.begin(), weights.end(), 0);
    std::vector<int> out;
    int used = 0;
    for (size_t i = 0; i + 1 < weights.size(); ++i) {
        out.push_back(total * weights[i] / sum);
        used += out.back();
    }
    out.push_back(total - used);
    return out;
}

// edges are rounded down, so shares never drift from the total
std::vector<int> split_edges(int total, const std::vector<int>& weights) {
    const int sum = std::accumulate(weights.begin(), weights.end(), 0);
    std::vector<int> out;
    int acc = 0, prev = 0;
    for (int w : weights) {
        acc += w;
        const int edge = total * acc / sum;
        out.push_back(edge - prev);
        prev = edge;
    }
    return out;
}

void place_stack(Layout& L, const std::vector<PanelId>& stack, int y, int x, int h, int w,
                 const std::vector<GpuInfo>& gpus) {
    if (stack.size() == 1) {
        // Fans never stretch: three rows of fans need no more
        L.panel[stack[0]] = { y, x, stack[0] == P_FAN ? std::min(h, FAN_H) : h, w };
        return;
    }

    if (stack[1] == P_FAN) {
        const int fan_h = std::min(FAN_H, std::max(0, h - MEM_MIN));
        L.panel[stack[0]] = { y,             x, h - fan_h, w };
        L.panel[stack[1]] = { y + h - fan_h, x, fan_h,     w };
        return;
    }

    // GPU's content height is satisfied first, CPU gets the rest (the reverse starved GPU to 0 rows)
    const bool vram    = !gpus.empty() && gpus.front().mem_total_mb > 0;
    const int gpu_want = (vram ? 3 : 2) + 2;
    const int CPU_MIN  = 5;
    const int gpu_h = std::min(gpu_want, std::max(0, h - CPU_MIN));
    const int cpu_h = h - gpu_h;
    L.panel[stack[0]] = { y,         x, cpu_h, w };
    L.panel[stack[1]] = { y + cpu_h, x, gpu_h, w };
}

}

Layout compute_layout(int rows, int cols, const std::vector<GpuInfo>& gpus, const PanelFlags& visible) {
    Layout L;
    L.rows = rows;
    L.cols = cols;

    // title bar above, key hints below
    const int avail = std::max(0, rows - 2);

    Template live;
    for (Row row : (cols >= 130 ? WIDE : cols >= 80 ? MEDIUM : NARROW)) {
        for (Cell& c : row.cells)
            c.panels.erase(std::remove_if(c.panels.begin(), c.panels.end(),
                                          [&](PanelId id) { return !visible[id]; }),
                           c.panels.end());
        row.cells.erase(std::remove_if(row.cells.begin(), row.cells.end(),
                                       [](const Cell& c) { return c.panels.empty(); }),
                        row.cells.end());
        if (!visible[P_FAN]) row.fan_over.clear();
        if (!row.fan_over.empty()) {
            // Fans need a host to sit under; without one they take the right half of the row on their own
            bool host = false;
            for (const Cell& c : row.cells)
                for (PanelId id : c.panels)
                    host = host || std::find(row.fan_over.begin(), row.fan_over.end(), id) != row.fan_over.end();
            if (!host) { row.cells.push_back({ 2, { P_FAN } }); row.fan_over.clear(); }
        }
        if (!row.cells.empty()) live.push_back(std::move(row));
    }
    if (live.empty()) return L;

    // fixed rows take their height first, the weighted rows share the rest
    std::vector<int> heights(live.size(), 0);
    std::vector<int> row_weights;
    int left = avail;
    for (size_t i = 0; i < live.size(); ++i) {
        if (live[i].fixed_h > 0) { heights[i] = std::min(live[i].fixed_h, left); left -= heights[i]; }
        else row_weights.push_back(live[i].h);
    }
    if (!row_weights.empty()) {
        const std::vector<int> shares = split_floor(left, row_weights);
        for (size_t i = 0, k = 0; i < live.size(); ++i)
            if (live[i].fixed_h == 0) heights[i] = shares[k++];
    }

    int y = 1;
    for (size_t ri = 0; ri < live.size(); ++ri) {
        std::vector<int> cell_weights;
        for (const Cell& c : live[ri].cells) cell_weights.push_back(c.w);
        const std::vector<int> widths = split_edges(cols, cell_weights);

        int x = 0;
        for (size_t ci = 0; ci < live[ri].cells.size(); ++ci) {
            place_stack(L, live[ri].cells[ci].panels, y, x, heights[ri], widths[ci], gpus);
            x += widths[ci];
        }

        if (!live[ri].fan_over.empty()) {
            // one wide Fans box under the hosts; the hosts give up its height
            int x0 = cols, x1 = 0;
            for (PanelId id : live[ri].fan_over) {
                const Rect& r = L.panel[id];
                if (r.h <= 0) continue;
                x0 = std::min(x0, r.x);
                x1 = std::max(x1, r.x + r.w);
            }
            const int fan_h = std::min(FAN_H, std::max(0, heights[ri] - MEM_MIN));
            if (fan_h > 0 && x1 > x0) {
                for (PanelId id : live[ri].fan_over)
                    if (L.panel[id].h > 0) L.panel[id].h -= fan_h;
                L.panel[P_FAN] = { y + heights[ri] - fan_h, x0, fan_h, x1 - x0 };
            }
        }
        y += heights[ri];
    }
    return L;
}
