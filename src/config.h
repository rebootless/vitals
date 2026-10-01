#pragma once

#include <string>

struct Config {
    std::string theme_name  = "Catppuccin Mocha";
    std::string bg_mode     = "transparent";
    std::string tty_mode    = "auto";
    int         refresh_ms  = 1000;
    std::string corners     = "square";
    std::string graph_style = "sparkline";
    bool        symbol_mirroring = false;
};

Config load_config();

void save_config(const Config& cfg);
