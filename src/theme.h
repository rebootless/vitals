#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Theme {
    std::string name;

    uint32_t BASE;

    uint32_t TEXT;
    uint32_t SUBTEXT0;
    uint32_t SURFACE1;
    uint32_t SURFACE2;
    uint32_t OVERLAY0;
    uint32_t BLUE;
    uint32_t GREEN;
    uint32_t PEACH;
    uint32_t RED;
    uint32_t MAUVE;     // titles / CPU freq
    uint32_t YELLOW;
    uint32_t TEAL;
};

const std::vector<Theme>& all_themes();

const Theme& current_theme();

void set_theme_index(int idx);

int find_theme_index(const std::string& name);
