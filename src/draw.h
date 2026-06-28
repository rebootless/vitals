#pragma once

#include "colors.h"
#include "format.h"

std::string str_trunc(const std::string& s, int maxcols);

std::tuple<int,int,int,int>
draw_box(ncplane* n, int y, int x, int h, int w,
         const std::string& title, const std::string& bl_label = "");

void draw_sep(ncplane* n, int y, int x, int w);

void lbr(ncplane* n, int y, int x);
void rbr(ncplane* n, int y, int x);

void draw_bar_grad(ncplane* n, int y, int x, int w, double fill, GradType gt);

void draw_spark(ncplane* n, int y, int x, int w, const std::deque<double>& hist);

void draw_titlebar(ncplane* n, int cols);
