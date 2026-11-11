#pragma once

#include "colors.h"
#include "format.h"
#include "draw_tty.h"

std::string str_trunc(const std::string& s, int maxcols);

// terminal columns of a UTF-8 string, one per code point
int str_cols(std::string_view s);

std::tuple<int,int,int,int>
draw_box(ncplane* n, int y, int x, int h, int w,
         const std::string& title, const std::string& bl_label = "");

void draw_sep(ncplane* n, int y, int x, int w);

void lbr(ncplane* n, int y, int x);
void rbr(ncplane* n, int y, int x);

void draw_bar_grad(ncplane* n, int y, int x, int w, double fill, GradType gt);

// "Label: [ 45%][bar]": the label padded to label_w, a percent box (N/A when !valid), a bar over the rest of the row
void draw_pct_row(ncplane* n, int y, int x, int w, int label_w, const char* label,
                  double pct, bool valid, uint32_t color, GradType gt);

void draw_spark(ncplane* n, int y, int x, int w, const std::deque<double>& hist);

void draw_spark_bidir(ncplane* n, int y, int x, int w,
                      const std::deque<double>& top_hist, uint32_t top_color,
                      const std::deque<double>& bot_hist, uint32_t bot_color);

// Network and Storage tables: a name, then two value columns led by the down and up arrows
struct PairCols {
    int  name_w;
    int  c2, c3;           // offsets of the value columns from the panel's left edge
    bool down_bold, up_bold;
};

void draw_pair_header(ncplane* n, int y, int x, int w, const PairCols& c,
                      const char* name, const char* down, const char* up);

void draw_pair_row(ncplane* n, int y, int x, int w, const PairCols& c,
                   const std::string& name, const std::string& down, const std::string& up);

// two rows: "<down arrow> label [graph]" over "<up arrow> label [graph]"
void draw_history_pair(ncplane* n, int y, int x, int w, int label_w,
                       const char* down_label, const char* up_label,
                       const std::deque<double>& down_hist, const std::deque<double>& up_hist);

void draw_titlebar(ncplane* n, int cols);

void draw_footer(ncplane* n, int rows, int cols);
