#pragma once

#include <string>

// Persisted at ~/.config/vitals/config as plain "key=value" lines:
//   theme=Catppuccin Mocha
//   background=transparent      (or "solid")
//   terminal=auto                (or "tty" / "graphics")
//   refresh_ms=1000              (100-60000)
//   corners=square                (or "rounded")
struct Config {
    std::string theme_name  = "Catppuccin Mocha";
    std::string bg_mode     = "transparent"; // "transparent" | "solid"
    std::string tty_mode    = "auto";        // "auto" | "tty" | "graphics"
    int         refresh_ms  = 1000;          // 100-60000
    std::string corners     = "square";      // "square" | "rounded"
};

// Reads the config file. Returns defaults (Catppuccin Mocha, transparent) if
// the file or the config directory doesn't exist yet, or on any parse error.
Config load_config();

// Writes the config file, creating ~/.config/vitals if needed.
// Best-effort: failures (e.g. read-only $HOME) are silently ignored.
void save_config(const Config& cfg);
