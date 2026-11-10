#pragma once

#include "defaults.h"
#include "settings.h"

#include <string>

struct Config {
    std::string theme_name  = DEFAULT_THEME;
    std::string bg_mode     = DEFAULT_BACKGROUND;
    std::string tty_mode    = DEFAULT_TERMINAL;
    int         refresh_ms  = DEFAULT_REFRESH_MS;
    int         log_size_mb = DEFAULT_LOG_SIZE_MB;
    int         log_keep    = DEFAULT_LOG_KEEP;
    std::string corners     = DEFAULT_CORNERS;
    std::string graph_style = DEFAULT_GRAPH_STYLE;
    bool        symbol_mirroring = DEFAULT_SYMBOL_MIRRORING;
    PanelModes  panels      = all_panels_auto();
};

Config load_config();

void save_config(const Config& cfg);

Settings settings_from_config(const Config& cfg);
Config   config_from_settings(const Settings& s);
