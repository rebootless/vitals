#pragma once

#include <string>

enum class TtyForce { Auto, ForceOn, ForceOff };

bool detect_linux_console();

bool resolve_tty_active(TtyForce force);

TtyForce    tty_force_from_string(const std::string& s);
std::string tty_force_to_string(TtyForce f);
