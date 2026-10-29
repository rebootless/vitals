#include "log.h"
#include "paths.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>

namespace {

// a long run at a short refresh would otherwise grow the file without bound
constexpr long long MAX_BYTES = 32LL * 1024 * 1024;

std::mutex  g_mutex;
int         g_fd    = -1;
long long   g_bytes = 0;

const char* level_name(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "INFO ";
}

std::string timestamp() {
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    tm lt{};
    localtime_r(&ts.tv_sec, &lt);

    char buf[40];
    const size_t n = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
    std::snprintf(buf + n, sizeof(buf) - n, ".%03ld", ts.tv_nsec / 1000000);
    return buf;
}

// the caller holds g_mutex
void write_line(const std::string& line) {
    if (g_fd < 0) return;

    if (g_bytes + static_cast<long long>(line.size()) > MAX_BYTES) {
        if (ftruncate(g_fd, 0) != 0) { close(g_fd); g_fd = -1; return; }
        g_bytes = 0;
        const std::string note = timestamp() + " INFO  [log] size limit reached, file restarted\n";
        if (write(g_fd, note.data(), note.size()) > 0) g_bytes += static_cast<long long>(note.size());
    }

    const ssize_t n = write(g_fd, line.data(), line.size());
    if (n > 0) g_bytes += n;
}

}

void log_open() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_fd >= 0) return;

    const std::string dir = config_dir();
    if (dir.empty()) return;

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return;

    char name[32];
    const time_t now = time(nullptr);
    tm lt{};
    localtime_r(&now, &lt);
    std::strftime(name, sizeof(name), "%m-%d-%Y.log", &lt);

    // lock before truncating: a second instance must not wipe the first one's log
    const int fd = open((dir + "/" + name).c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (fd < 0) return;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0 || ftruncate(fd, 0) != 0) { close(fd); return; }

    g_fd    = fd;
    g_bytes = 0;
}

void log_close() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_fd >= 0) close(g_fd);
    g_fd = -1;
}

void log_write(LogLevel level, const char* tag, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_fd < 0) return;

    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    const int len = std::vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    std::string msg(len > 0 ? static_cast<size_t>(len) : 0, '\0');
    if (len > 0) std::vsnprintf(msg.data(), msg.size() + 1, fmt, ap2);
    va_end(ap2);

    for (char& c : msg)
        if (c == '\n' || c == '\r') c = ' '; // one line per event

    write_line(timestamp() + " " + level_name(level) + " [" + tag + "] " + msg + "\n");
}

void log_source(const char* name, bool ok, const char* error) {
    static std::map<std::string, bool> last;
    static std::mutex                  m;

    {
        std::lock_guard<std::mutex> lock(m);
        auto it = last.find(name);
        if (it != last.end() && it->second == ok) return;
        const bool first = (it == last.end());
        last[name] = ok;
        if (first && ok) {
            log_write(LogLevel::Info, "source", "%s: ok", name);
            return;
        }
    }

    if (ok) log_write(LogLevel::Info, "source", "%s: recovered", name);
    else    log_write(LogLevel::Warn, "source", "%s: failed (%s)", name, error ? error : "unknown error");
}
