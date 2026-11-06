#pragma once

struct notcurses;

// One-line messages for stderr. While notcurses owns the screen they are held back and printed
// once it is released: anything written to stderr meanwhile would be lost with the alternate screen.
void con_say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void con_hold(notcurses* nc);   // the terminal now belongs to notcurses
void con_release();             // notcurses is stopped: print what was held

void con_stage(const char* name);                  // where the program is; named in a crash message
void con_set_log_path(const char* path);           // for the "see ... for details" line
void con_install_crash_handlers();                 // call before notcurses_init: it passes fatal signals on to us
