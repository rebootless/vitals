#include "layout.h"

#include <algorithm>
#include <numeric>
#include <utility>

namespace {

struct Cell { int w; std::vector<PanelId> panels; }; // panels stacked top to bottom
struct Row  { int h; std::vector<Cell>    cells;  };

using Template = std::vector<Row>;

const Template WIDE = {
    { 3, { { 2, { P_CPU, P_GPU } }, { 1, { P_MEMORY } }, { 1, { P_THERMAL } } } },
    { 2, { { 1, { P_NETWORK } },    { 1, { P_STORAGE } } } },
};

const Template MEDIUM = {
    { 2, { { 1, { P_CPU, P_GPU } }, { 1, { P_MEMORY } } } },
    { 2, { { 1, { P_NETWORK } },    { 1, { P_STORAGE } } } },
    { 1, { { 1, { P_THERMAL } } } },
};

const Template NARROW = {
    { 1, { { 1, { P_CPU } } } },
    { 1, { { 1, { P_GPU } } } },
    { 1, { { 1, { P_MEMORY } } } },
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
        L.panel[stack[0]] = { y, x, h, w };
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
        if (!row.cells.empty()) live.push_back(std::move(row));
    }
    if (live.empty()) return L;

    std::vector<int> row_weights;
    for (const Row& r : live) row_weights.push_back(r.h);
    const std::vector<int> heights = split_floor(avail, row_weights);

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
        y += heights[ri];
    }
    return L;
}
