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

// two adjacent literals keep \xb0 from absorbing the following C
#define DEG_C "\xc2\xb0" "C"
#define DEG   "\xc2\xb0"

using ull   = unsigned long long;
using Clock = std::chrono::steady_clock;

inline constexpr int HIST_CAP = 200;

static const char* BLOCK[] = { " ","▏","▎","▍","▌","▋","▊","▉","█" };
static const char* SPARK[] = { "▁","▂","▃","▄","▅","▆","▇","█" };
static const char* SPARK_BRAILLE[] = { "⣀","⣀","⣤","⣤","⣶","⣶","⣿","⣿" };
// SPARK_MIRROR uses U+1FB82..1FB86 (Symbols for Legacy Computing): font-dependent
static const char* SPARK_MIRROR[] = { "▔","🮂","🮃","▀","🮄","🮅","🮆","█" };
static const char* SPARK_BRAILLE_MIRROR[] = { "⠉","⠉","⠛","⠛","⠿","⠿","⣿","⣿" };
static const char* BAR_BG  = "•";
