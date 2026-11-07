#include "input.h"
#include "config.h"
#include "log.h"
#include "state.h"
#include "theme.h"

#include <algorithm>
#include <iterator>

namespace {

const int REFRESH_STEPS[] = {
    100, 200, 300, 500, 750, 1000, 1500, 2000, 3000, 5000,
    7500, 10000, 15000, 20000, 30000, 45000, 60000
};
const int REFRESH_STEPS_N = sizeof(REFRESH_STEPS) / sizeof(REFRESH_STEPS[0]);

int refresh_step_index() {
    int best = 0;
    int best_diff = std::abs(REFRESH_STEPS[0] - G.opt.refresh_ms);
    for (int i = 1; i < REFRESH_STEPS_N; ++i) {
        int diff = std::abs(REFRESH_STEPS[i] - G.opt.refresh_ms);
        if (diff < best_diff) { best = i; best_diff = diff; }
    }
    return best;
}

int wrap(int v, int n) { return ((v % n) + n) % n; }

void open_settings() {
    G.opt_saved = G.opt;
    G.settings_panel_cursor = 0;
    G.settings_focus        = F_THEME;
    G.settings_theme_top    = 0;
    G.settings_right_top    = 0;
    G.settings_open         = true;
    LOG_INFO("ui", "settings opened");
}

void discard_settings() {
    G.opt        = G.opt_saved;
    G.tty_active = resolve_tty_active(G.opt.tty_force);
    G.settings_open = false;
    LOG_INFO("ui", "settings discarded");
}

void save_settings() {
    const Config cfg = config_from_settings(G.opt);
    save_config(cfg);
    G.settings_open = false;
    LOG_INFO("ui", "settings saved: theme=%s background=%s terminal=%s refresh=%d corners=%s graph=%s mirroring=%d",
             cfg.theme_name.c_str(), cfg.bg_mode.c_str(), cfg.tty_mode.c_str(), cfg.refresh_ms,
             cfg.corners.c_str(), cfg.graph_style.c_str(), cfg.symbol_mirroring ? 1 : 0);
}

// up/down changes the value of the focused group; dir is -1 for up, +1 for down
void change_value(int dir) {
    Settings& s = G.opt;
    switch (G.settings_focus) {
    case F_THEME:
        s.theme_idx = wrap(s.theme_idx + dir, static_cast<int>(all_themes().size()));
        break;
    case F_BG:      s.solid_bg = !s.solid_bg; break;
    case F_CORNERS: s.rounded  = !s.rounded;  break;
    case F_GRAPH:   s.braille  = !s.braille;  break;
    case F_MIRROR:  s.mirror   = !s.mirror;   break;
    case F_TERMINAL:
        s.tty_force  = static_cast<TtyForce>(wrap(static_cast<int>(s.tty_force) + dir, 3));
        G.tty_active = resolve_tty_active(s.tty_force);
        break;
    case F_PANELS:
        G.settings_panel_cursor = wrap(G.settings_panel_cursor + dir, PANEL_N);
        break;
    default: // refresh: up is a longer interval
        s.refresh_ms = REFRESH_STEPS[std::clamp(refresh_step_index() - dir, 0, REFRESH_STEPS_N - 1)];
        break;
    }
}

void settings_key(uint32_t ch) {
    if (ch == NCKEY_ESC) {
        discard_settings();
    } else if (ch == '\t') {
        const int* at = std::find(std::begin(SETTINGS_TAB_ORDER), std::end(SETTINGS_TAB_ORDER), G.settings_focus);
        G.settings_focus = SETTINGS_TAB_ORDER[(at - std::begin(SETTINGS_TAB_ORDER) + 1) % std::size(SETTINGS_TAB_ORDER)];
    } else if (ch == ' ' && G.settings_focus == F_PANELS) {
        PanelMode& m = G.opt.panels[static_cast<size_t>(G.settings_panel_cursor)];
        m = (m == PanelMode::On)   ? PanelMode::Auto
          : (m == PanelMode::Auto) ? PanelMode::Off
                                   : PanelMode::On;
    } else if (ch == NCKEY_UP) {
        change_value(-1);
    } else if (ch == NCKEY_DOWN) {
        change_value(+1);
    } else if (ch == NCKEY_ENTER || ch == '\n' || ch == '\r') {
        save_settings();
    }
}

}

bool handle_key(uint32_t ch) {
    if (G.settings_open) {
        settings_key(ch);
        return false;
    }

    if (ch == 'q' || ch == 'Q') return true;

    if (ch == 'f' || ch == 'F') {
        G.frozen = !G.frozen;
        LOG_INFO("ui", "freeze %s", G.frozen ? "on" : "off");
    }

    if (ch == NCKEY_ESC) open_settings();
    return false;
}
