#include "config.h"
#include "paths.h"
#include "log.h"
#include "strutil.h"
#include "theme.h"

#include <algorithm>
#include <fstream>
#include <filesystem>

namespace {

// a bad value leaves `out` as it was
void read_int(int& out, const std::string& key, const std::string& val, int lo, int hi, int lineno) {
    try {
        const int v = std::stoi(val);
        out = std::clamp(v, lo, hi);
        if (out != v) LOG_WARN("config", "%s=%d is out of range, using %d", key.c_str(), v, out);
    } catch (...) {
        LOG_WARN("config", "line %d: invalid %s \"%s\"", lineno, key.c_str(), val.c_str());
    }
}

}

std::string config_summary(const Config& cfg) {
    return "theme=" + cfg.theme_name + " background=" + cfg.bg_mode + " terminal=" + cfg.tty_mode +
           " refresh=" + std::to_string(cfg.refresh_ms) + " log_size=" + std::to_string(cfg.log_size_mb) +
           " log_keep=" + std::to_string(cfg.log_keep) + " corners=" + cfg.corners +
           " graph=" + cfg.graph_style + " mirroring=" + (cfg.symbol_mirroring ? "1" : "0");
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
        const std::string trimmed = str_trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            LOG_WARN("config", "line %d ignored (no '='): %s", lineno, line.c_str());
            continue;
        }

        std::string key = str_trim(line.substr(0, eq));
        std::string val = str_trim(line.substr(eq + 1));
        const int panel = str_starts(key, "panel_") ? panel_index(key.substr(6)) : -1;
        if (key == "theme")           cfg.theme_name = val;
        else if (key == "background") cfg.bg_mode    = val;
        else if (key == "terminal")   cfg.tty_mode    = val;
        else if (key == "corners")    cfg.corners     = val;
        else if (key == "graph_style") cfg.graph_style = val;
        else if (key == "symbol_mirroring") cfg.symbol_mirroring = (val == "true");
        else if (panel >= 0) {
            if (!mode_from_string(val, cfg.panels[static_cast<size_t>(panel)]))
                LOG_WARN("config", "line %d: invalid value \"%s\" for %s", lineno, val.c_str(), key.c_str());
        }
        else if (key == "refresh_ms")  read_int(cfg.refresh_ms,  key, val, 100, 60000, lineno);
        else if (key == "log_size_mb") read_int(cfg.log_size_mb, key, val, 2, 1024, lineno);
        else if (key == "log_keep")    read_int(cfg.log_keep,    key, val, 1, 50, lineno);
        else {
            LOG_WARN("config", "line %d: unknown key \"%s\"", lineno, key.c_str());
        }
    }
    LOG_INFO("config", "read %s: %s", path.c_str(), config_summary(cfg).c_str());
    return cfg;
}

Settings settings_from_config(const Config& cfg) {
    Settings s;
    const int ti = find_theme_index(cfg.theme_name);
    s.theme_idx  = ti >= 0 ? ti : 0;
    s.solid_bg   = (cfg.bg_mode == "solid");
    s.tty_force  = tty_force_from_string(cfg.tty_mode);
    s.refresh_ms = cfg.refresh_ms;
    s.log_mb     = cfg.log_size_mb;
    s.log_keep   = cfg.log_keep;
    s.rounded    = (cfg.corners == "rounded");
    s.braille    = (cfg.graph_style == "braille");
    s.mirror     = cfg.symbol_mirroring;
    s.panels     = cfg.panels;
    return s;
}

Config config_from_settings(const Settings& s) {
    Config cfg;
    cfg.theme_name       = all_themes()[static_cast<size_t>(s.theme_idx)].name;
    cfg.bg_mode          = s.solid_bg ? "solid" : "transparent";
    cfg.tty_mode         = tty_force_to_string(s.tty_force);
    cfg.refresh_ms       = s.refresh_ms;
    cfg.log_size_mb      = s.log_mb;
    cfg.log_keep         = s.log_keep;
    cfg.corners          = s.rounded ? "rounded" : "square";
    cfg.graph_style      = s.braille ? "braille" : "sparkline";
    cfg.symbol_mirroring = s.mirror;
    cfg.panels           = s.panels;
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
        f << "log_size_mb=" << cfg.log_size_mb << "\n";
        f << "log_keep=" << cfg.log_keep << "\n";
        f << "corners=" << cfg.corners << "\n";
        f << "graph_style=" << cfg.graph_style << "\n";
        f << "symbol_mirroring=" << (cfg.symbol_mirroring ? "true" : "false") << "\n";
        for (int i = 0; i < PANEL_N; ++i)
            f << "panel_" << PANELS[i].key << "=" << mode_name(cfg.panels[static_cast<size_t>(i)]) << "\n";

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
