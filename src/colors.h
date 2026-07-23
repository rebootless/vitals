#pragma once

#include "common.h"
#include "state.h"
#include "theme.h"

inline const Theme& theme() { return current_theme(); }

inline void nc_bg_apply(ncplane* n) {
    uint64_t channels = 0;

    if (G.bg_idx == 1) {
        uint32_t rgb = theme().BASE;

        ncchannels_set_fg_default(&channels);

        ncchannels_set_bg_rgb8(
            &channels,
            static_cast<unsigned>((rgb >> 16) & 0xFF),
            static_cast<unsigned>((rgb >>  8) & 0xFF),
            static_cast<unsigned>( rgb        & 0xFF));
    } else {
        ncchannels_set_fg_default(&channels);
        ncchannels_set_bg_default(&channels);
    }

    ncplane_set_base(n, " ", 0, channels);
}

inline void nc_fg(ncplane* n, uint32_t rgb) {
    ncplane_set_fg_rgb8(n,
        static_cast<unsigned>((rgb >> 16) & 0xFF),
        static_cast<unsigned>((rgb >>  8) & 0xFF),
        static_cast<unsigned>( rgb        & 0xFF));
    nc_bg_apply(n);
}

inline void nc_set(ncplane* n, uint32_t rgb, unsigned style = NCSTYLE_NONE) {
    nc_fg(n, rgb);
    ncplane_set_styles(n, style);
}

static inline uint32_t lerp_rgb(uint32_t a, uint32_t b, double t) {
    t = std::max(0.0, std::min(1.0, t));
    auto ch = [&](int shift) -> uint32_t {
        int av = (a >> shift) & 0xFF;
        int bv = (b >> shift) & 0xFF;
        return static_cast<uint32_t>(av + static_cast<int>((bv - av) * t));
    };
    return (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

inline uint32_t grad_color(GradType gt, double pos) {
    pos = std::max(0.0, std::min(1.0, pos));
    const Theme& t = theme();
    switch (gt) {
        case GRAD_CPU:
        case GRAD_HIST:
            if (pos < 0.5) return lerp_rgb(t.GREEN,  t.YELLOW, pos * 2.0);
            else           return lerp_rgb(t.YELLOW, t.RED,    (pos - 0.5) * 2.0);

        case GRAD_MEM:
            return lerp_rgb(t.BLUE, t.MAUVE, pos);

        case GRAD_TEMP:
            if (pos < 0.5) return lerp_rgb(t.YELLOW, t.PEACH, pos * 2.0);
            else           return lerp_rgb(t.PEACH,  t.RED,   (pos - 0.5) * 2.0);

        case GRAD_STORAGE:
            return lerp_rgb(t.TEAL, t.BLUE, pos);
    }
    return t.GREEN;
}

inline uint32_t pct_color(double pct) {
    const Theme& t = theme();
    if (pct >= 85.0) return t.RED;
    if (pct >= 60.0) return t.YELLOW;
    return t.GREEN;
}
