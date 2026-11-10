#include "log.h"
#include "console.h"
#include "paths.h"

#include <algorithm>
#include <cstdarg>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

// a long run at a short refresh would otherwise grow the file without bound
long long g_max_bytes = 32LL * 1024 * 1024;

std::mutex  g_mutex;
int         g_fd    = -1;
long long   g_bytes = 0;
bool        g_full  = false;
std::string g_path;

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
    if (g_fd < 0 || g_full) return;

    if (g_bytes + static_cast<long long>(line.size()) > g_max_bytes) {
        g_full = true;
        const std::string note = timestamp() + " WARN  [log] size limit reached, logging stopped\n";
        if (write(g_fd, note.data(), note.size()) > 0) g_bytes += static_cast<long long>(note.size());
        return;
    }

    const ssize_t n = write(g_fd, line.data(), line.size());
    if (n > 0) g_bytes += n;
}

// the caller holds g_mutex; returns how many files were removed
int prune_logs(int keep) {
    namespace fs = std::filesystem;

    const std::string dir = config_dir();
    if (dir.empty()) return 0;

    std::error_code ec;
    std::vector<std::pair<fs::file_time_type, fs::path>> files;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        std::error_code fec;
        if (!e.is_regular_file(fec) || e.path().extension() != ".log" || e.path() == g_path) continue;
        const auto t = e.last_write_time(fec);
        if (!fec) files.emplace_back(t, e.path());
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

    const size_t slots = static_cast<size_t>(std::max(0, keep - (g_path.empty() ? 0 : 1)));
    int removed = 0;
    for (size_t i = slots; i < files.size(); ++i) {
        const int fd = open(files[i].second.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        if (flock(fd, LOCK_EX | LOCK_NB) == 0 && fs::remove(files[i].second, ec)) ++removed;
        close(fd);
    }
    return removed;
}

}

void log_open() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_fd >= 0) return;

    const std::string dir = config_dir();
    if (dir.empty()) { con_say("warning: log disabled: HOME is not set"); return; }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) { con_say("warning: log disabled: cannot create %s: %s", dir.c_str(), ec.message().c_str()); return; }

    char name[32];
    const time_t now = time(nullptr);
    tm lt{};
    localtime_r(&now, &lt);
    std::strftime(name, sizeof(name), "%m-%d-%Y_%H-%M-%S.log", &lt);
    const std::string path = dir + "/" + name;

    // lock before truncating: a second instance must not wipe the first one's log
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (fd < 0) { con_say("warning: log disabled: cannot open %s: %s", path.c_str(), std::strerror(errno)); return; }

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const int err = errno;
        if (err == EWOULDBLOCK) con_say("warning: log disabled: another instance is using %s", path.c_str());
        else                    con_say("warning: log disabled: cannot lock %s: %s", path.c_str(), std::strerror(err));
        close(fd);
        return;
    }
    if (ftruncate(fd, 0) != 0) {
        const int err = errno;
        con_say("warning: log disabled: cannot truncate %s: %s", path.c_str(), std::strerror(err));
        close(fd);
        return;
    }

    g_fd    = fd;
    g_bytes = 0;
    g_full  = false;
    g_path  = path;
    con_set_log_path(path.c_str());
}

const std::string& log_path() { return g_path; }

void log_set_max_mb(int mb) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_max_bytes = static_cast<long long>(mb) * 1024 * 1024;
    if (g_bytes < g_max_bytes) g_full = false;
}

void log_set_keep(int keep) {
    int removed = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        removed = prune_logs(keep);
    }
    if (removed > 0) LOG_INFO("log", "removed %d old log file(s), keeping %d", removed, keep);
}

void log_clear() {
    int removed = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        removed = prune_logs(g_path.empty() ? 0 : 1);
    }
    LOG_INFO("log", "cleared %d old log file(s) on request", removed);
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
