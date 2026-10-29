#include "tty.h"
#include "log.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unistd.h>

bool detect_linux_console() {
    // the kernel VT sets TERM=linux (getty, console, rescue shells)
    const char* term = std::getenv("TERM");
    if (term && std::string(term) == "linux") return true;

    // TERM overridden: /dev/ttyN (digits) is a VT; tty0 and ttyS* are excluded, /dev/console included
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
    bool active = false;
    switch (force) {
        case TtyForce::ForceOn:  active = true;  break;
        case TtyForce::ForceOff: active = false; break;
        case TtyForce::Auto:
        default:                 active = detect_linux_console(); break;
    }
    LOG_DEBUG("tty", "mode %s -> %s", tty_force_to_string(force).c_str(), active ? "text-only console" : "graphics");
    return active;
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
