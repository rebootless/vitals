#pragma once

#include <array>
#include <string_view>

enum PanelId { P_CPU, P_GPU, P_MEMORY, P_NETWORK, P_STORAGE, P_THERMAL, PANEL_N };

struct PanelInfo {
    const char* key;   // config key suffix: panel_<key>
    const char* label; // name in the settings overlay
};

inline constexpr PanelInfo PANELS[PANEL_N] = {
    { "cpu",     "CPU"     },
    { "gpu",     "GPU"     },
    { "memory",  "Memory"  },
    { "network", "Network" },
    { "storage", "Storage" },
    { "thermal", "Thermal" },
};

using PanelFlags = std::array<bool, PANEL_N>;

inline constexpr PanelFlags all_panels_on() {
    PanelFlags f{};
    for (bool& b : f) b = true;
    return f;
}

inline int panel_index(std::string_view key) {
    for (int i = 0; i < PANEL_N; ++i)
        if (key == PANELS[i].key) return i;
    return -1;
}
