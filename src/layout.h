#pragma once

#include "state.h"

struct Rect { int y = 0, x = 0, h = 0, w = 0; };

struct Layout {
    int  rows = 0, cols = 0;
    Rect cpu, gpu, memory, network, storage, thermal;
};

Layout compute_layout(int rows, int cols, const std::vector<GpuInfo>& gpus);
