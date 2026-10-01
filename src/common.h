#pragma once

#include "notcurses/include/notcurses/notcurses.h"

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

// UTF-8 degree symbol — two adjacent literals prevent \xb0C hex sequence bleed
#define DEG_C "\xc2\xb0" "C"
#define DEG   "\xc2\xb0"

using ull   = unsigned long long;
using Clock = std::chrono::steady_clock;

// Rolling-history buffer cap, shared by cpu_hist and the aggregated
// net/disk rate histories (see AppState in state.h).
inline constexpr int HIST_CAP = 200;

// Block / sparkline / bar-background glyphs
// Each glyph is a valid UTF-8 sequence rendered as exactly 1 terminal cell.
static const char* BLOCK[] = { " ","▏","▎","▍","▌","▋","▊","▉","█" };
static const char* SPARK[] = { "▁","▂","▃","▄","▅","▆","▇","█" };
static const char* SPARK_BRAILLE[] = { "⣀","⣀","⣤","⣤","⣶","⣶","⣿","⣿" };
// Mirrored (fill from the top of the cell down) — lower row of bidirectional graphs.
// SPARK_MIRROR uses Unicode "Symbols for Legacy Computing" (U+1FB82..1FB86): font-dependent.
static const char* SPARK_MIRROR[] = { "▔","🮂","🮃","▀","🮄","🮅","🮆","█" };
static const char* SPARK_BRAILLE_MIRROR[] = { "⠉","⠉","⠛","⠛","⠿","⠿","⣿","⣿" };
static const char* BAR_BG  = "•";
