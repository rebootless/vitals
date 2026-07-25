#pragma once

#include "colors.h"
#include <deque>
#include <string>
#include <tuple>

std::tuple<int,int,int,int>
draw_box_tty(ncplane* n, int y, int x, int h, int w,
            const std::string& title, const std::string& bl_label);

void draw_bar_tty  (ncplane* n, int y, int x, int w, double fill, GradType gt);
void draw_spark_tty(ncplane* n, int y, int x, int w, const std::deque<double>& hist);

const char* glyph_down();  // "\xe2\x96\xbc" (▼) or "v"
const char* glyph_up();    // "\xe2\x96\xb2" (▲) or "^"
const char* glyph_dash();  // "\xe2\x80\x94" (—) or "-"
const char* deg_suffix();  // "\xc2\xb0" "C" (°C) or "C"
