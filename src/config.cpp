#include "config.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace {

std::string config_dir() {
    const char* home = std::getenv("HOME");
    if (!home || !*home) return "";
    return std::string(home) + "/.config/vitals";
}

std::string config_path() {
    std::string dir = config_dir();
    return dir.empty() ? "" : dir + "/config";
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

} // namespace

Config load_config() {
    Config cfg{}; // defaults

    std::string path = config_path();
    if (path.empty()) return cfg;

    std::ifstream f(path);
    if (!f) return cfg; // no config yet — first run

    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key == "theme")           cfg.theme_name = val;
        else if (key == "background") cfg.bg_mode    = val;
        else if (key == "terminal")   cfg.tty_mode    = val;
        else if (key == "refresh_ms") {
            try {
                int v = std::stoi(val);
                cfg.refresh_ms = std::max(100, std::min(60000, v));
            } catch (...) { /* keep default */ }
        }
    }
    return cfg;
}

void save_config(const Config& cfg) {
    std::string dir = config_dir();
    if (dir.empty()) return;

    // Best-effort recursive-ish mkdir: ~/.config then ~/.config/vitals.
    std::string home_config = dir.substr(0, dir.find_last_of('/'));
    ::mkdir(home_config.c_str(), 0755);
    ::mkdir(dir.c_str(), 0755);

    std::string path = dir + "/config";
    std::ofstream f(path, std::ios::trunc);
    if (!f) return;

    f << "theme=" << cfg.theme_name << "\n";
    f << "background=" << cfg.bg_mode << "\n";
    f << "terminal=" << cfg.tty_mode << "\n";
    f << "refresh_ms=" << cfg.refresh_ms << "\n";
}
