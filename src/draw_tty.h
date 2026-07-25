#pragma once

#include "colors.h"
#include <deque>
#include <string>
#include <tuple>

/*
draw_tty
ASCII-safe counterparts to the Unicode drawing primitives in draw.cpp — the
Linux kernel virtual console's built-in font frequently lacks box-drawing
(┌─┐│└┘), block-element (▏▎▍...█, ▁▂▃...█), and arrow (▲▼) glyphs, so a
raw TTY session needs a rendering path that only ever emits 7-bit ASCII.

These are called internally by draw_box() / draw_bar_grad() / draw_spark()
in draw.cpp when G.tty_active is set — panels never call draw_*_tty()
directly, they keep calling the normal draw_box() / draw_bar_grad() /
draw_spark() and the dispatch happens transparently. Panels DO call the
small glyph_*() helpers directly, for the handful of spots (RX/TX arrows,
degree sign) that don't go through a shared primitive.
*/

std::tuple<int,int,int,int>
draw_box_tty(ncplane* n, int y, int x, int h, int w,
            const std::string& title, const std::string& bl_label);

void draw_bar_tty  (ncplane* n, int y, int x, int w, double fill, GradType gt);
void draw_spark_tty(ncplane* n, int y, int x, int w, const std::deque<double>& hist);

// Small glyph swaps, resolved against G.tty_active — used directly by
// panel_network.cpp, panel_storage.cpp, panel_thermal.cpp, panel_settings.cpp,
// and draw.cpp's title bar.
const char* glyph_down();  // "\xe2\x96\xbc" (▼) or "v"
const char* glyph_up();    // "\xe2\x96\xb2" (▲) or "^"
const char* glyph_dash();  // "\xe2\x80\x94" (—) or "-"
const char* deg_suffix();  // "\xc2\xb0" "C" (°C) or "C"
