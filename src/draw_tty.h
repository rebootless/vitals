#pragma once

#include "colors.h"

void draw_bar_tty  (ncplane* n, int y, int x, int w, double fill, GradType gt);

const char* glyph_down();  // "\xe2\x96\xbc" (▼) or "v"
const char* glyph_up();    // "\xe2\x96\xb2" (▲) or "^"
const char* deg_suffix();  // "\xc2\xb0" "C" (°C) or "C"
