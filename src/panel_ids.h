#pragma once

#include <array>
#include <cstddef>
#include <string_view>

enum PanelId { P_CPU, P_GPU, P_MEMORY, P_NETWORK, P_STORAGE, P_THERMAL, P_FAN, PANEL_N };

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
    { "fan",     "Fans"    },
};

enum class PanelMode { On, Auto, Off }; // config values: true, auto, false

using PanelFlags = std::array<bool, PANEL_N>;
using PanelModes = std::array<PanelMode, PANEL_N>;

inline constexpr PanelModes all_panels_auto() {
    PanelModes m{};
    for (PanelMode& x : m) x = PanelMode::Auto;
    return m;
}

inline const char* mode_name(PanelMode m) {
    switch (m) {
        case PanelMode::On:  return "true";
        case PanelMode::Off: return "false";
        default:             return "auto";
    }
}

inline bool mode_from_string(std::string_view s, PanelMode& out) {
    if (s == "true")  { out = PanelMode::On;   return true; }
    if (s == "auto")  { out = PanelMode::Auto; return true; }
    if (s == "false") { out = PanelMode::Off;  return true; }
    return false;
}

inline int panel_index(std::string_view key) {
    for (int i = 0; i < PANEL_N; ++i)
        if (key == PANELS[i].key) return i;
    return -1;
}

// auto panels follow has_data; the layout only ever sees the result
inline PanelFlags panel_visibility(const PanelModes& modes, const PanelFlags& has_data) {
    PanelFlags v{};
    for (size_t i = 0; i < static_cast<size_t>(PANEL_N); ++i)
        v[i] = modes[i] == PanelMode::On || (modes[i] == PanelMode::Auto && has_data[i]);
    return v;
}
