#include "layout.h"

#include <utility>

Layout compute_layout(int rows, int cols, const std::vector<GpuInfo>& gpus) {
    Layout L;
    L.rows = rows;
    L.cols = cols;

    int avail = rows - 1;
    int top_h = avail * 3 / 5;
    int bot_h = avail - top_h;

    // GPU's content height is satisfied first, CPU gets the rest (the reverse starved GPU to 0 rows)
    auto split_cpu_gpu = [&](int col_h) -> std::pair<int, int> {
        if (gpus.empty()) return { col_h, 0 };
        int gpu_rows = (gpus.front().mem_total_mb > 0) ? 3 : 2;
        int gpu_want = gpu_rows + 2;

        const int CPU_MIN = 5;
        int gpu_h = std::min(gpu_want, std::max(0, col_h - CPU_MIN));
        int cpu_h = col_h - gpu_h;
        return { cpu_h, gpu_h };
    };

    if (cols >= 130) {
        int c1 = cols / 2;
        int c2 = (cols - c1) / 2;
        int c3 = cols - c1 - c2;

        int b1 = cols / 2;
        int b2 = cols - b1;

        auto [cpu_h, gpu_h] = split_cpu_gpu(top_h);

        L.cpu     = { 1,        0,     cpu_h, c1 };
        L.gpu     = { 1+cpu_h,  0,     gpu_h, c1 };
        L.memory  = { 1,        c1,    top_h, c2 };
        L.thermal = { 1,        c1+c2, top_h, c3 };
        L.network = { 1+top_h,  0,     bot_h, b1 };
        L.storage = { 1+top_h,  b1,    bot_h, b2 };

    } else if (cols >= 80) {
        int half  = cols / 2;
        int mid_h = avail * 2 / 5;
        top_h     = avail * 2 / 5;
        bot_h     = avail - top_h - mid_h;

        auto [cpu_h, gpu_h] = split_cpu_gpu(top_h);

        L.cpu     = { 1,             0,    cpu_h, half };
        L.gpu     = { 1+cpu_h,       0,    gpu_h, half };
        L.memory  = { 1,             half, top_h, cols-half };
        L.network = { 1+top_h,       0,    mid_h, half };
        L.storage = { 1+top_h,       half, mid_h, cols-half };
        L.thermal = { 1+top_h+mid_h, 0,    bot_h, cols };

    } else {
        int np = gpus.empty() ? 5 : 6;
        int ph = avail / np, rem = avail % np;

        int r = 0;
        L.cpu = { ph*r, 0, ph, cols }; r++;
        if (!gpus.empty()) { L.gpu = { ph*r, 0, ph, cols }; r++; }
        L.memory  = { ph*r, 0, ph, cols }; r++;
        L.network = { ph*r, 0, ph, cols }; r++;
        L.storage = { ph*r, 0, ph, cols }; r++;
        L.thermal = { ph*r, 0, ph+rem, cols };
    }

    return L;
}
