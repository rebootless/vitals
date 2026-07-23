#pragma once

#include <string>

struct Config {
    std::string theme_name = "Catppuccin Mocha";
    std::string bg_mode    = "transparent";
};

Config load_config();

void save_config(const Config& cfg);
