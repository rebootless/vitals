#include "theme.h"
#include "state.h"
#include "paths.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace {

namespace fs = std::filesystem;

const Theme& builtin_theme() {
    static const Theme t = {
        "Default",
        0x000000,
        0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff,
        0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff, 0xffffff,
    };
    return t;
}

struct Role {
    const char*    key;
    uint32_t Theme::* field;
};

const Role ROLES[] = {
    { "BG",        &Theme::BG        },
    { "FG",        &Theme::FG        },
    { "FG_DIM",    &Theme::FG_DIM    },
    { "BAR_EMPTY", &Theme::BAR_EMPTY },
    { "BORDER",    &Theme::BORDER    },
    { "MUTED",     &Theme::MUTED     },
    { "LABEL",     &Theme::LABEL     },
    { "LOW",       &Theme::LOW       },
    { "DATA_A",    &Theme::DATA_A    },
    { "HIGH",      &Theme::HIGH      },
    { "TITLE",     &Theme::TITLE     },
    { "MID",       &Theme::MID       },
    { "DATA_B",    &Theme::DATA_B    },
};

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool parse_hex(const std::string& h, uint32_t& out) {
    if (h.empty()) return false;
    for (char c : h)
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    out = static_cast<uint32_t>(std::stoul(h, nullptr, 16));
    return true;
}

// Accepts "#RRGGBB", "#RR" (grey, same as btop) or "R G B" with 0-255 each.
bool parse_color(const std::string& text, uint32_t& out) {
    std::string s = trim(text);
    if (s.empty()) return false;

    if (s[0] == '#') {
        std::string h = s.substr(1);
        uint32_t v = 0;
        if (h.size() == 6 && parse_hex(h, v)) { out = v; return true; }
        if (h.size() == 2 && parse_hex(h, v)) { out = (v << 16) | (v << 8) | v; return true; }
        return false;
    }

    std::istringstream is(s);
    int r, g, b;
    std::string extra;
    if (!(is >> r >> g >> b) || (is >> extra)) return false;
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) return false;
    out = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    return true;
}

bool load_theme_file(const fs::path& path, Theme& out) {
    std::ifstream f(path);
    if (!f) return false;

    Theme t = builtin_theme();
    t.name = path.stem().string();

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.rfind("theme[", 0) != 0) continue;

        size_t close = line.find(']');
        if (close == std::string::npos) continue;
        size_t eq = line.find('=', close);
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(6, close - 6));
        std::string val = trim(line.substr(eq + 1));
        if (!val.empty() && val[0] == '"') {
            size_t q = val.find('"', 1);
            val = (q == std::string::npos) ? val.substr(1) : val.substr(1, q - 1);
        }

        if (key == "NAME") {
            if (!trim(val).empty()) t.name = trim(val);
            continue;
        }
        for (const Role& r : ROLES) {
            if (key != r.key) continue;
            uint32_t c = 0;
            if (parse_color(val, c)) t.*(r.field) = c;
            break;
        }
    }

    out = std::move(t);
    return true;
}

const std::vector<Theme>& registry() {
    static const std::vector<Theme> themes = [] {
        std::map<std::string, Theme> by_file;

        for (const fs::path& dir : theme_dirs()) {
            std::error_code ec;
            fs::directory_iterator it(dir, ec), end;
            if (ec) continue;
            for (; it != end; it.increment(ec)) {
                if (ec) break;
                const fs::path& p = it->path();
                if (p.extension() != ".theme") continue;
                std::error_code fec;
                if (!fs::is_regular_file(p, fec)) continue;

                Theme t;
                if (load_theme_file(p, t)) by_file[lower(p.stem().string())] = std::move(t);
            }
        }

        std::vector<Theme> files;
        files.reserve(by_file.size());
        for (auto& kv : by_file) files.push_back(std::move(kv.second));
        std::sort(files.begin(), files.end(), [](const Theme& a, const Theme& b) {
            return lower(a.name) < lower(b.name);
        });

        std::vector<Theme> all;
        all.reserve(files.size() + 1);
        all.push_back(builtin_theme());
        for (Theme& t : files) all.push_back(std::move(t));
        return all;
    }();
    return themes;
}

}

const std::vector<Theme>& all_themes() {
    return registry();
}

const Theme& current_theme() {
    const auto& themes = registry();
    int idx = G.theme_idx;
    if (idx < 0 || idx >= static_cast<int>(themes.size())) idx = 0;
    return themes[static_cast<size_t>(idx)];
}

void set_theme_index(int idx) {
    const int n = static_cast<int>(registry().size());
    if (n == 0) return;
    G.theme_idx = ((idx % n) + n) % n;
}

int find_theme_index(const std::string& name) {
    const auto& themes = registry();
    for (size_t i = 0; i < themes.size(); ++i)
        if (themes[i].name == name) return static_cast<int>(i);
    return -1;
}
