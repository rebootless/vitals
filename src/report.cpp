#include "report.h"
#include "console.h"
#include "log.h"
#include "paths.h"
#include "state.h"
#include "strutil.h"

#include <cerrno>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

void report_terminal_failure() {
    con_say("error: cannot start the terminal UI (notcurses_init failed)");
    const char* term = getenv("TERM");
    if (!isatty(STDOUT_FILENO))
        con_say("hint: stdout is not a terminal; run vitals directly in a terminal, not through a pipe or redirect");
    else if (!term || !*term)
        con_say("hint: TERM is not set; set it to a real terminal type, e.g. xterm-256color");
    else if (std::strcmp(term, "dumb") == 0)
        con_say("hint: TERM is \"dumb\"; set TERM to a real terminal type, e.g. xterm-256color");
    if (!log_path().empty()) con_say("see %s for details", log_path().c_str());
}

void report_terminal(notcurses* nc) {
    const char* term = getenv("TERM");
    unsigned rows = 0, cols = 0;
    notcurses_stddim_yx(nc, &rows, &cols);

    const bool utf8 = notcurses_canutf8(nc);
    char colors[32];
    if (notcurses_cantruecolor(nc)) std::snprintf(colors, sizeof(colors), "truecolor");
    else                            std::snprintf(colors, sizeof(colors), "%u colors", notcurses_palette_size(nc));

    con_say("[3/6] terminal: %s, %s, %s, %ux%u", term && *term ? term : "(unset)",
            utf8 ? "UTF-8" : "not UTF-8", colors, cols, rows);
    if (!utf8) {
        const char* loc = std::setlocale(LC_CTYPE, nullptr);
        con_say("warning: locale \"%s\" is not UTF-8, borders and graphs may be drawn incorrectly", loc ? loc : "?");
    }
}

void report_config() {
    const std::string path = config_path();
    if (path.empty()) { con_say("[4/6] config: unavailable (HOME is not set), using defaults"); return; }
    if (access(path.c_str(), R_OK) == 0) { con_say("[4/6] config: %s", path.c_str()); return; }
    if (errno == ENOENT) { con_say("[4/6] config: %s (not found, using defaults)", path.c_str()); return; }
    con_say("warning: cannot read %s: %s, using defaults", path.c_str(), std::strerror(errno));
    con_say("[4/6] config: %s (unreadable, using defaults)", path.c_str());
}

void report_data() {
    static const PanelId ORDER[] = { P_CPU, P_MEMORY, P_NETWORK, P_STORAGE, P_GPU, P_THERMAL, P_FAN };
    std::string s;
    for (PanelId id : ORDER) {
        const std::string name = str_lower(PANELS[id].label);
        s += (s.empty() ? "" : ", ") + name + (G.panel_avail[static_cast<size_t>(id)] ? " ok" : " n/a");
    }
    con_say("[5/6] data: %s", s.c_str());
}
