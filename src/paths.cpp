#include "paths.h"

#include <cstdlib>
#include <system_error>

namespace fs = std::filesystem;

std::string config_dir() {
    const char* home = std::getenv("HOME");
    if (!home || !*home) return "";
    return std::string(home) + "/.config/vitals";
}

std::string config_path() {
    std::string dir = config_dir();
    return dir.empty() ? "" : dir + "/config";
}

std::vector<fs::path> theme_dirs() {
    std::vector<fs::path> dirs;
    dirs.emplace_back("/usr/share/vitals/themes");
    dirs.emplace_back("/usr/local/share/vitals/themes");

    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        dirs.push_back(exe.parent_path().parent_path() / "share/vitals/themes");
        dirs.push_back(exe.parent_path() / "themes");
    }

    const std::string cfg = config_dir();
    if (!cfg.empty()) dirs.push_back(fs::path(cfg) / "themes");
    return dirs;
}
