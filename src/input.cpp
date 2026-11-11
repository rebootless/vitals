#include "input.h"
#include "config.h"
#include "log.h"
#include "state.h"
#include "theme.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>

namespace {

constexpr int REFRESH_STEPS[] = {
    100, 200, 300, 500, 750, 1000, 1500, 2000, 3000, 5000,
    7500, 10000, 15000, 20000, 30000, 45000, 60000
};
constexpr int LOG_SIZE_STEPS[]  = { 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };
constexpr int LOG_COUNT_STEPS[] = { 1, 3, 5, 10, 20, 50 };

// the table entry nearest to `value`, moved by `dir` (+1 = one step down the table) and kept inside it
template <size_t N>
int step_by(const int (&steps)[N], int value, int dir) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(N); ++i)
        if (std::abs(steps[i] - value) < std::abs(steps[best] - value)) best = i;
    return steps[std::clamp(best - dir, 0, static_cast<int>(N) - 1)];
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
    const bool clear_logs = G.opt.clear_logs;
    if (G.opt.reset_settings) {
        G.opt        = settings_from_config(Config{});
        G.tty_active = resolve_tty_active(G.opt.tty_force);
        LOG_INFO("ui", "settings reset to defaults");
    }
    G.opt.clear_logs = false;

    const Config cfg = config_from_settings(G.opt);
    save_config(cfg);
    log_set_max_mb(cfg.log_size_mb);
    log_set_keep(cfg.log_keep);
    if (clear_logs) log_clear();
    G.settings_open = false;
    LOG_INFO("ui", "settings saved: %s", config_summary(cfg).c_str());
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
    case F_LOG_SIZE:
        s.log_mb = step_by(LOG_SIZE_STEPS, s.log_mb, dir);
        break;
    case F_CLEAR_LOGS:     s.clear_logs     = !s.clear_logs;     break;
    case F_RESET_SETTINGS: s.reset_settings = !s.reset_settings; break;
    case F_LOG_COUNT:
        s.log_keep = step_by(LOG_COUNT_STEPS, s.log_keep, dir);
        break;
    default: // refresh: up is a longer interval
        s.refresh_ms = step_by(REFRESH_STEPS, s.refresh_ms, dir);
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
    } else if (ch == ' ' && (G.settings_focus == F_CLEAR_LOGS || G.settings_focus == F_RESET_SETTINGS)) {
        change_value(+1);
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
