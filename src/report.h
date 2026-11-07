#pragma once

#include "common.h"

// the startup lines on stderr (see console.h)
void report_terminal_failure();
void report_terminal(notcurses* nc);
void report_config();
void report_data();
