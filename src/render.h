#pragma once

#include "panels.h"

void render(notcurses* nc, ncplane* n);

// the real terminal size differs from the one the plane was last resized to
bool terminal_resized(ncplane* n);
