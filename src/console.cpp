#include "console.h"
#include "common.h"
#include "log.h"

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <unistd.h>

namespace {

// fixed buffers: the crash handlers may run with a damaged heap
char        g_held[16384];
size_t      g_held_n = 0;
bool        g_holding = false;
notcurses*  g_nc = nullptr;
const char* g_stage = "startup";
char        g_log_path[1024] = "";

void put(const char* s) {
    size_t left = std::strlen(s);
    while (left > 0) {
        const ssize_t n = write(STDERR_FILENO, s, left);
        if (n <= 0) return;
        s += n;
        left -= static_cast<size_t>(n);
    }
}

void flush_held() {
    g_held[g_held_n] = '\0';
    put(g_held);
    g_held_n = 0;
}

void see_log() {
    if (!g_log_path[0]) return;
    put("see "); put(g_log_path); put(" for details\n");
}

const char* signal_name(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGABRT: return "SIGABRT";
        default:      return "a signal";
    }
}

// notcurses restores the terminal first and then hands the signal on to this handler
void on_signal(int sig) {
    flush_held();
    put("fatal: terminated by "); put(signal_name(sig)); put(" during "); put(g_stage); put("\n");
    see_log();
    signal(sig, SIG_DFL);
    raise(sig);
}

void on_terminate() {
    std::string what = "unknown exception";
    if (std::exception_ptr ep = std::current_exception()) {
        try { std::rethrow_exception(ep); }
        catch (const std::exception& e) { what = e.what(); }
        catch (...) {}
    }

    if (g_nc) { notcurses_stop(g_nc); g_nc = nullptr; } // the terminal first, then the message
    g_holding = false;
    flush_held();
    put("fatal: unhandled exception during "); put(g_stage); put(": "); put(what.c_str()); put("\n");
    see_log();
    LOG_ERROR("crash", "unhandled exception during %s: %s", g_stage, what.c_str());
    log_close();
    std::_Exit(1);
}

}

void con_say(const char* fmt, ...) {
    char line[600];
    va_list ap;
    va_start(ap, fmt);
    const int len = std::vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    if (len <= 0) return;

    const size_t n = std::min(static_cast<size_t>(len), sizeof(line) - 2);
    line[n] = '\n';
    line[n + 1] = '\0';

    if (!g_holding) { put(line); return; }
    if (g_held_n + n + 1 < sizeof(g_held)) {
        std::memcpy(g_held + g_held_n, line, n + 1);
        g_held_n += n + 1;
    }
}

void con_hold(notcurses* nc) { g_nc = nc; g_holding = true; }

void con_release() {
    g_holding = false;
    g_nc = nullptr;
    flush_held();
}

void con_stage(const char* name) { g_stage = name; }

void con_set_log_path(const char* path) {
    std::snprintf(g_log_path, sizeof(g_log_path), "%s", path ? path : "");
}

void con_install_crash_handlers() {
    std::set_terminate(on_terminate);

    struct sigaction sa{};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND | SA_NODEFER;
    for (int sig : {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT}) sigaction(sig, &sa, nullptr);
}
