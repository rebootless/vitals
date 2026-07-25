#include "tty.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unistd.h>

bool detect_linux_console() {
    // Strongest signal: the kernel VT itself sets TERM=linux. This alone
    // covers the vast majority of real-world cases (getty, systemd's
    // "console", recovery/rescue shells, ...).
    const char* term = std::getenv("TERM");
    if (term && std::string(term) == "linux") return true;

    // Fallback in case TERM was overridden: resolve what stdout actually
    // points at. /dev/ttyN (all-digit suffix) is a kernel VT; /dev/tty0
    // (no digits after "tty") and /dev/ttyS* (serial) are deliberately
    // excluded from this check, /dev/console is included.
    char buf[64];
    ssize_t n = readlink("/proc/self/fd/1", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        std::string path(buf);

        if (path == "/dev/console") return true;

        if (path.rfind("/dev/tty", 0) == 0) {
            std::string rest = path.substr(8);
            bool all_digits = !rest.empty() &&
                std::all_of(rest.begin(), rest.end(),
                            [](unsigned char c) { return std::isdigit(c) != 0; });
            if (all_digits) return true;
        }
    }

    return false;
}

bool resolve_tty_active(TtyForce force) {
    switch (force) {
        case TtyForce::ForceOn:  return true;
        case TtyForce::ForceOff: return false;
        case TtyForce::Auto:
        default:                 return detect_linux_console();
    }
}

TtyForce tty_force_from_string(const std::string& s) {
    if (s == "tty")      return TtyForce::ForceOn;
    if (s == "graphics") return TtyForce::ForceOff;
    return TtyForce::Auto;
}

std::string tty_force_to_string(TtyForce f) {
    switch (f) {
        case TtyForce::ForceOn:  return "tty";
        case TtyForce::ForceOff: return "graphics";
        default:                 return "auto";
    }
}
