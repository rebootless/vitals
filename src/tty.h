#pragma once

#include <string>

// How the person wants terminal-mode decided (Settings overlay -> Terminal).
enum class TtyForce { Auto, ForceOn, ForceOff };

// True if stdout appears to be a raw Linux virtual console (kernel VT,
// /dev/ttyN) or /dev/console, as opposed to a real terminal emulator/pty
// (xterm, alacritty, tmux, an SSH session, ...). Used by the Auto setting.
//
// The Linux console's built-in font frequently lacks Unicode box-drawing,
// block-element, and arrow glyphs — vitals falls back to a plain-ASCII
// rendering path (see draw_tty.h) when this is (effectively) true.
bool detect_linux_console();

// Resolves the effective tty-mode flag from the person's forced choice.
bool resolve_tty_active(TtyForce force);

// Parses/serializes TtyForce <-> the strings persisted in the config file.
TtyForce    tty_force_from_string(const std::string& s);
std::string tty_force_to_string(TtyForce f);
