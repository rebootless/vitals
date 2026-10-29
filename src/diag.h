#pragma once

#include "common.h"

// the startup records of the debug log: build, environment, terminal, data sources
void log_startup(int argc, char** argv);
void log_terminal(notcurses* nc);
void log_data_sources();
