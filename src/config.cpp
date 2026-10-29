#include "config.h"
#include "paths.h"
#include "log.h"

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
    if (path.empty()) {
        LOG_WARN("config", "no settings path (HOME is not set), using defaults");
        return cfg;
    }

    std::ifstream f(path);
    if (!f) {
        LOG_INFO("config", "%s not found, using defaults", path.c_str());
        return cfg;
    }

    std::string line;
    int lineno = 0;
    while (std::getline(f, line)) {
        ++lineno;
        if (trim(line).empty() || trim(line)[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            LOG_WARN("config", "line %d ignored (no '='): %s", lineno, line.c_str());
            continue;
        }

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
                if (cfg.refresh_ms != v) LOG_WARN("config", "refresh_ms=%d is out of range, using %d", v, cfg.refresh_ms);
            } catch (...) {
                LOG_WARN("config", "line %d: invalid refresh_ms \"%s\"", lineno, val.c_str());
            }
        } else {
            LOG_WARN("config", "line %d: unknown key \"%s\"", lineno, key.c_str());
        }
    }
    LOG_INFO("config", "read %s: theme=%s background=%s terminal=%s refresh=%d corners=%s graph=%s mirroring=%d",
             path.c_str(), cfg.theme_name.c_str(), cfg.bg_mode.c_str(), cfg.tty_mode.c_str(), cfg.refresh_ms,
             cfg.corners.c_str(), cfg.graph_style.c_str(), cfg.symbol_mirroring ? 1 : 0);
    return cfg;
}

void save_config(const Config& cfg) {
    std::string dir = config_dir();
    if (dir.empty()) {
        LOG_WARN("config", "settings not saved: no config directory");
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        LOG_ERROR("config", "settings not saved: cannot create %s: %s", dir.c_str(), ec.message().c_str());
        return;
    }

    // write beside the target and rename, so a crash never leaves a partial config
    const std::string path = config_path();
    const std::string tmp  = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f) {
            LOG_ERROR("config", "settings not saved: cannot write %s", tmp.c_str());
            return;
        }

        f << "theme=" << cfg.theme_name << "\n";
        f << "background=" << cfg.bg_mode << "\n";
        f << "terminal=" << cfg.tty_mode << "\n";
        f << "refresh_ms=" << cfg.refresh_ms << "\n";
        f << "corners=" << cfg.corners << "\n";
        f << "graph_style=" << cfg.graph_style << "\n";
        f << "symbol_mirroring=" << (cfg.symbol_mirroring ? "true" : "false") << "\n";

        f.close();
        if (f.fail()) {
            LOG_ERROR("config", "settings not saved: write to %s failed", tmp.c_str());
            std::filesystem::remove(tmp, ec);
            return;
        }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        LOG_ERROR("config", "settings not saved: rename to %s failed: %s", path.c_str(), ec.message().c_str());
        std::filesystem::remove(tmp, ec);
        return;
    }
    LOG_DEBUG("config", "settings saved to %s", path.c_str());
}
