#pragma once

#include "defaults.h"
#include "panel_ids.h"
#include "tty.h"

// what the Settings overlay edits; it works on a copy and Esc restores the saved one
struct Settings {
    int        theme_idx  = 0;
    bool       solid_bg   = false;   // false = transparent (terminal default), true = theme's own BG color
    TtyForce   tty_force  = TtyForce::Auto;
    int        refresh_ms = DEFAULT_REFRESH_MS;
    int        log_mb     = DEFAULT_LOG_SIZE_MB;
    int        log_keep   = DEFAULT_LOG_KEEP;
    bool       clear_logs     = false;   // one-shot: runs on save, never persisted
    bool       reset_settings = false;   // one-shot, same
    bool       rounded    = false;
    bool       braille    = false;
    bool       mirror     = DEFAULT_SYMBOL_MIRRORING;   // symbol mirroring
    PanelModes panels     = all_panels_auto();
};

// the overlay's option groups
enum Focus { F_THEME, F_BG, F_TERMINAL, F_CORNERS, F_GRAPH, F_MIRROR, F_PANELS, F_REFRESH, F_LOG_SIZE, F_LOG_COUNT, F_CLEAR_LOGS, F_RESET_SETTINGS };

// Tab visits the groups in the order they appear on screen
inline constexpr int SETTINGS_TAB_ORDER[12] = { F_THEME, F_BG, F_CORNERS, F_GRAPH, F_MIRROR, F_TERMINAL, F_REFRESH, F_LOG_SIZE, F_LOG_COUNT, F_CLEAR_LOGS, F_RESET_SETTINGS, F_PANELS };
