#include "paths.h"

#include <cstdlib>
#include <system_error>

namespace fs = std::filesystem;

std::string config_dir() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] == '/') return std::string(xdg) + "/vitals"; // the spec ignores relative paths

    const char* home = std::getenv("HOME");
    if (!home || !*home) return "";
    return std::string(home) + "/.config/vitals";
}

std::string config_path() {
    std::string dir = config_dir();
    return dir.empty() ? "" : dir + "/config.conf";
}

std::string system_theme_dir() {
    return std::string(VITALS_DATA_DIR) + "/themes";
}

std::vector<fs::path> theme_dirs() {
    std::vector<fs::path> dirs;
    dirs.emplace_back(system_theme_dir());

    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) dirs.push_back(exe.parent_path() / "themes"); // running from the build tree

    const std::string cfg = config_dir();
    if (!cfg.empty()) dirs.push_back(fs::path(cfg) / "themes");
    return dirs;
}
