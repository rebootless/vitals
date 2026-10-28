#include "config.h"
#include "paths.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

}

Config load_config() {
    Config cfg{};

    std::string path = config_path();
    if (path.empty()) return cfg;

    std::ifstream f(path);
    if (!f) return cfg;

    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key == "theme")           cfg.theme_name = val;
        else if (key == "background") cfg.bg_mode    = val;
        else if (key == "terminal")   cfg.tty_mode    = val;
        else if (key == "corners")    cfg.corners     = val;
        else if (key == "graph_style") cfg.graph_style = val;
        else if (key == "symbol_mirroring") cfg.symbol_mirroring = (val == "true");
        else if (key == "refresh_ms") {
            try {
                int v = std::stoi(val);
                cfg.refresh_ms = std::max(100, std::min(60000, v));
            } catch (...) { }
        }
    }
    return cfg;
}

void save_config(const Config& cfg) {
    std::string dir = config_dir();
    if (dir.empty()) return;

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return;

    std::string path = dir + "/config";
    std::ofstream f(path, std::ios::trunc);
    if (!f) return;

    f << "theme=" << cfg.theme_name << "\n";
    f << "background=" << cfg.bg_mode << "\n";
    f << "terminal=" << cfg.tty_mode << "\n";
    f << "refresh_ms=" << cfg.refresh_ms << "\n";
    f << "corners=" << cfg.corners << "\n";
    f << "graph_style=" << cfg.graph_style << "\n";
    f << "symbol_mirroring=" << (cfg.symbol_mirroring ? "true" : "false") << "\n";
}
