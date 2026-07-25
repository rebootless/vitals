#pragma once

#include "common.h"
#include "state.h"
#include "theme.h"

// Convenience accessor used throughout draw.cpp / panel_*.cpp: `theme().GREEN`
// instead of spelling out current_theme() everywhere.
inline const Theme& theme() { return current_theme(); }

// Color / style helpers

// Applies the currently configured background: either the terminal's own
// default (transparent) or the active theme's own solid BASE color —
// controlled by G.bg_idx, toggled from the Settings overlay (Esc).
//
// In TTY mode this is skipped entirely (always terminal-default) — see
// nc_fg() below for why.
inline void nc_bg_apply(ncplane* n) {
    uint64_t channels = 0;

    if (!G.tty_active && G.bg_idx == 1) {
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

// Set foreground from 0xRRGGBB, apply the currently configured background.
//
// In TTY mode, theme colors are ignored entirely and the terminal's own
// default foreground/background is used instead (NCSTYLE_BOLD etc. still
// apply for emphasis). Raw Linux consoles are frequently limited to a
// handful of colors and don't reliably support arbitrary RGB — a color
// that looks fine over SSH can render as black-on-black (or otherwise
// illegible) on the physical console, so this is the one safe default.
inline void nc_fg(ncplane* n, uint32_t rgb) {
    if (G.tty_active) {
        ncplane_set_fg_default(n);
        nc_bg_apply(n);
        return;
    }

    ncplane_set_fg_rgb8(n,
        static_cast<unsigned>((rgb >> 16) & 0xFF),
        static_cast<unsigned>((rgb >>  8) & 0xFF),
        static_cast<unsigned>( rgb        & 0xFF));
    nc_bg_apply(n);
}

// Set fg + style in one call — the main draw helper used throughout panels.
inline void nc_set(ncplane* n, uint32_t rgb, unsigned style = NCSTYLE_NONE) {
    nc_fg(n, rgb);
    ncplane_set_styles(n, style);
}

/*
True-color gradient interpolation
Returns a 0xRRGGBB value for position pos ∈ [0, 1], matching the HTML gradients:
   CPU / HIST / GPU: green  -> yellow → red (--green  -> --yellow -> --red)
   MEM / VRAM:       blue   -> mauve        (--blue   -> --mauve)
   TEMP:             yellow -> peach → red  (--yellow -> --peach  -> --red)
   STORAGE:          teal   -> blue         (--teal   -> --blue)

All colors come from the active Theme, so the gradient re-colors itself
automatically when the theme changes — no per-panel changes needed.

Because notcurses supports true 24-bit color, this is a simple linear
interpolation — no LUT, no xterm-256 cube approximation needed.
*/

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

// Discrete color for textual percentage display (the " 73%" label).
inline uint32_t pct_color(double pct) {
    const Theme& t = theme();
    if (pct >= 85.0) return t.RED;
    if (pct >= 60.0) return t.YELLOW;
    return t.GREEN;
}
