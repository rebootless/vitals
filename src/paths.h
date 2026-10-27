#pragma once

#include <filesystem>
#include <string>
#include <vector>

// "" when HOME is unset
std::string config_dir();
std::string config_path();

// lowest priority first: a same-named theme in a later directory wins
std::vector<std::filesystem::path> theme_dirs();
