#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Theme {
    std::string name;

    uint32_t BG;

    uint32_t FG;
    uint32_t FG_DIM;
    uint32_t BAR_EMPTY;
    uint32_t BORDER;
    uint32_t MUTED;
    uint32_t LABEL;
    uint32_t LOW;
    uint32_t DATA_A;     // RX / write / warm (temperature mid)
    uint32_t HIGH;
    uint32_t TITLE;      // titles / CPU freq
    uint32_t MID;
    uint32_t DATA_B;     // TX / read / cached
};

const std::vector<Theme>& all_themes();

const Theme& current_theme();

void set_theme_index(int idx);

int find_theme_index(const std::string& name);
