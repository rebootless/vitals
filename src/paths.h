#pragma once

#include <filesystem>
#include <string>
#include <vector>

// $XDG_CONFIG_HOME/vitals, else ~/.config/vitals; "" when neither is usable
std::string config_dir();
std::string config_path();

// <prefix>/share/vitals/themes, the prefix is fixed at build time
std::string system_theme_dir();

// lowest priority first: a same-named theme in a later directory wins
std::vector<std::filesystem::path> theme_dirs();
