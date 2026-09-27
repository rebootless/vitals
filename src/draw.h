#pragma once

#include "colors.h"
#include "format.h"
#include "draw_tty.h"

// Text utilities
// Truncate s to at most maxcols terminal columns (UTF-8 aware, 1 col per codepoint).
std::string str_trunc(const std::string& s, int maxcols);

/*
Box drawing
Draws a rounded box with a title in the top border.
Optional bl_label is embedded in the bottom-left border (e.g. "q:Quit").
Returns {inner_y, inner_x, inner_h, inner_w} — the usable area inside the box.
Returns {y,x,0,0} if the box is too small to have an interior.
*/

std::tuple<int,int,int,int>
draw_box(ncplane* n, int y, int x, int h, int w,
         const std::string& title, const std::string& bl_label = "");

// Separator line
void draw_sep(ncplane* n, int y, int x, int w);

// Bracket helpers
void lbr(ncplane* n, int y, int x);
void rbr(ncplane* n, int y, int x);

// Gradient bar
// fill ∈ [0, 1]. Each filled cell is colored with the true-color gradient for
// its position in the bar. Empty cells use theme().SURFACE1 + BAR_BG ("░").
void draw_bar_grad(ncplane* n, int y, int x, int w, double fill, GradType gt);

// Sparkline (CPU history)
// Each column uses the GRAD_HIST gradient at that sample's intensity.
// Glyph table follows G.graph_style_idx (0 = block spark, 1 = braille).
void draw_spark(ncplane* n, int y, int x, int w, const std::deque<double>& hist);

// Bidirectional sparkline (Network RX/TX, Storage Read/Write).
// Draws two rows: top_hist at (y, x), bot_hist at (y+1, x), each in its own
// solid color (not gradient — direction matters more than intensity here).
// Both rows share one auto-scaled max (the larger of the two histories'
// visible-window peaks, floored at 1.0) so they're visually comparable.
// Glyph table follows G.graph_style_idx, same as draw_spark.
void draw_spark_bidir(ncplane* n, int y, int x, int w,
                      const std::deque<double>& top_hist, uint32_t top_color,
                      const std::deque<double>& bot_hist, uint32_t bot_color);

// Title bar (row 0, full width)
void draw_titlebar(ncplane* n, int cols);
