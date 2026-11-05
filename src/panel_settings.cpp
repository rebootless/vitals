#include "panels.h"
#include "theme.h"
#include "draw_tty.h"

namespace {

const char* BG_MODE_NAMES[2] = { "Theme background: False", "Theme background: True" };
const char* TTY_MODE_NAMES[3] = { "Auto-detect", "Force TTY mode", "Force graphics mode" };
const char* CORNER_MODE_NAMES[2] = { "Square", "Rounded" };
const char* GRAPH_STYLE_NAMES[2] = { "Sparkline", "Braille" };
const char* MIRROR_NAMES[2] = { "Symbol Mirroring: False", "Symbol Mirroring: True" };

// Focus ids, as main.cpp uses them
enum Focus { F_THEME, F_BG, F_TERMINAL, F_CORNERS, F_GRAPH, F_MIRROR, F_PANELS, F_REFRESH };

// One line of the right-hand column; the whole column is built first so it can be scrolled
enum class Kind { Blank, Block, Header, Note, Option, PanelRow, Refresh };

struct Line {
    Kind        kind  = Kind::Blank;
    std::string text;
    int         group = -1;     // focus id this line belongs to
    bool        selected = false;
    bool        dim      = false;
};

std::vector<Line> build_lines() {
    std::vector<Line> v;
    auto blank  = [&] { v.push_back({}); };
    auto block  = [&](const char* t, int group = -1) { Line l; l.kind = Kind::Block; l.text = t; l.group = group; v.push_back(l); };
    auto header = [&](const char* t, int group) { Line l; l.kind = Kind::Header; l.text = t; l.group = group; v.push_back(l); };
    auto note   = [&](const char* t, int group) { Line l; l.kind = Kind::Note; l.text = t; l.group = group; v.push_back(l); };
    auto option = [&](const char* t, int group, bool sel) {
        Line l; l.kind = Kind::Option; l.text = t; l.group = group; l.selected = sel; v.push_back(l);
    };

    block("Appearance");
    header("Background", F_BG);
    for (int i = 0; i < 2; ++i) option(BG_MODE_NAMES[i], F_BG, i == G.bg_idx);
    header("Corners", F_CORNERS);
    for (int i = 0; i < 2; ++i) option(CORNER_MODE_NAMES[i], F_CORNERS, i == G.corners_idx);
    header("Graph style", F_GRAPH);
    for (int i = 0; i < 2; ++i) option(GRAPH_STYLE_NAMES[i], F_GRAPH, i == G.graph_style_idx);
    header("Graph Symbol Mirroring", F_MIRROR);
    note("(Experimental)", F_MIRROR);
    for (int i = 0; i < 2; ++i) option(MIRROR_NAMES[i], F_MIRROR, i == G.symbol_mirror_idx);

    blank();
    block("Behavior");
    header("Terminal", F_TERMINAL);
    for (int i = 0; i < 3; ++i) option(TTY_MODE_NAMES[i], F_TERMINAL, static_cast<int>(G.tty_force) == i);
    { Line l; l.kind = Kind::Refresh; l.group = F_REFRESH; v.push_back(l); }

    blank();
    block("Panels", F_PANELS);
    for (int i = 0; i < PANEL_N; ++i) {
        const size_t    k    = static_cast<size_t>(i);
        const PanelMode mode = G.panel_mode[k];
        const bool no_data   = (mode == PanelMode::Auto && !G.panel_avail[k]);
        Line l;
        l.kind     = Kind::PanelRow;
        l.group    = F_PANELS;
        l.selected = (G.settings_focus == F_PANELS && i == G.settings_panel_cursor);
        l.dim      = (mode == PanelMode::Off || no_data);
        char buf[48];
        snprintf(buf, sizeof(buf), "%-8s %s%s", PANELS[i].label, mode_name(mode), no_data ? " (no data)" : "");
        l.text = buf;
        v.push_back(l);
    }
    return v;
}

// keep the window [top, top+vis) over the lines lo..hi (hi wins when the range is taller than the window)
int follow(int top, int lo, int hi, int vis, int total) {
    if (hi >= top + vis) top = hi - vis + 1;
    if (lo < top)        top = lo;
    return std::max(0, std::min(top, std::max(0, total - vis)));
}

void draw_scrollbar(ncplane* n, int x, int y, int h, int top, int vis, int total) {
    if (total <= vis || h < 4) return;
    h -= 1; // the track stops one row short of the bottom border
    const int thumb = std::max(1, std::min(h, (h * vis + total - 1) / total)); // rounded up: the thumb never vanishes
    const int pos   = (h - thumb) * top / std::max(1, total - vis);
    for (int i = 0; i < h; ++i) {
        const bool on = (i >= pos && i < pos + thumb);
        nc_set(n, on ? theme().LABEL : theme().BORDER, NCSTYLE_NONE);
        ncplane_putstr_yx(n, y + i, x, on ? "\xe2\x96\x88" : "\xe2\x96\x91");
    }
}

}

void panel_settings(ncplane* n, int rows, int cols) {
    const auto& themes = all_themes();
    const int n_themes = static_cast<int>(themes.size());

    const std::vector<Line> lines = build_lines();
    const int total = static_cast<int>(lines.size());

    int w = std::min(68, std::max(30, cols - 4));
    int h = std::min(total + 2, rows - 4); // the right column sets the height; both columns scroll when it doesn't fit
    if (w < 24 || h < 6) return;

    int x = (cols - w) / 2;
    int y = (rows - h) / 2;

    std::string hint = G.tty_active
        ? "esc:Close  tab:Next  up/down:Select  space:Toggle  enter:Save"
        : "esc:Close  tab:Next  \xe2\x86\x91\xe2\x86\x93:Select  space:Toggle  enter:Save";

    auto [iy, ix, ih, iw] = draw_box(n, y, x, h, w, "Settings", hint);
    if (ih <= 0 || iw <= 0) return;

    // the right column keeps 29 columns for its longest line ("Theme background: False"); the themes get the rest
    constexpr int RIGHT_W = 29;
    const bool two_cols = iw - 1 - RIGHT_W >= 14;
    const int left_w  = two_cols ? iw - 1 - RIGHT_W : iw; // the divider sits in the column after the left area
    const int right_x = ix + left_w + 1;

    // ---- left: themes ----
    {
        const bool focused = (G.settings_focus == F_THEME);
        nc_set(n, theme().LABEL, NCSTYLE_BOLD);
        char hdr[64];
        snprintf(hdr, sizeof(hdr), " %sTheme (%d of %d)", focused ? "> " : "  ", G.theme_idx + 1, n_themes);
        ncplane_putstr_yx(n, iy, ix, str_trunc(hdr, left_w).c_str());

        const int vis = std::max(0, ih - 1);
        G.settings_theme_top = follow(G.settings_theme_top, G.theme_idx, G.theme_idx, vis, n_themes);

        for (int i = 0; i < vis && G.settings_theme_top + i < n_themes; ++i) {
            const int t = G.settings_theme_top + i;
            nc_set(n, t == G.theme_idx ? theme().LOW : theme().FG_DIM, NCSTYLE_NONE);
            const std::string label = std::string(t == G.theme_idx ? "   > " : "     ") +
                                      str_trunc(themes[static_cast<size_t>(t)].name, left_w - 6);
            ncplane_putstr_yx(n, iy + 1 + i, ix, label.c_str());
        }
        draw_scrollbar(n, ix + left_w - 1, iy + 1, vis, G.settings_theme_top, vis, n_themes);
    }

    if (!two_cols) return;

    nc_set(n, theme().BORDER, NCSTYLE_NONE);
    for (int r = 0; r < ih; ++r) ncplane_putstr_yx(n, iy + r, ix + left_w, "\xe2\x94\x82");

    // ---- right: blocks ----
    const int vis    = ih;
    const bool bar   = total > vis;
    const int text_w = iw - left_w - 1 - (bar ? 1 : 0);

    // lines of the focused group: the window must show them (for Panels, up to the cursor row)
    int lo = -1, hi = -1;
    for (int i = 0; i < total; ++i)
        if (lines[static_cast<size_t>(i)].group == G.settings_focus) { if (lo < 0) lo = i; hi = i; }
    if (lo >= 0) {
        const Line& first = lines[static_cast<size_t>(lo)];
        if (first.kind == Kind::Header && lo > 0 && lines[static_cast<size_t>(lo - 1)].kind == Kind::Block) --lo; // keep the block title with its first group
        if (G.settings_focus == F_PANELS) hi = lo + 1 + G.settings_panel_cursor;
        G.settings_right_top = follow(G.settings_right_top, lo, hi, vis, total);
    } else {
        G.settings_right_top = follow(G.settings_right_top, 0, 0, vis, total);
    }

    for (int i = 0; i < vis && G.settings_right_top + i < total; ++i) {
        const Line& l = lines[static_cast<size_t>(G.settings_right_top + i)];
        const int r = iy + i;
        const bool focused = (l.group == G.settings_focus);

        switch (l.kind) {
        case Kind::Blank:
            break;
        case Kind::Block: {
            nc_set(n, theme().TITLE, NCSTYLE_BOLD);
            const std::string t = " " + l.text + " ";
            ncplane_putstr_yx(n, r, right_x, str_trunc(t, text_w).c_str());
            const int used = static_cast<int>(str_trunc(t, text_w).size());
            nc_set(n, theme().BORDER, NCSTYLE_NONE);
            for (int c = used; c < text_w; ++c) ncplane_putstr_yx(n, r, right_x + c, "-");
            break;
        }
        case Kind::Header:
            nc_set(n, theme().LABEL, NCSTYLE_BOLD);
            ncplane_putstr_yx(n, r, right_x, str_trunc(std::string(focused ? " > " : "   ") + l.text, text_w).c_str());
            break;
        case Kind::Note:
            nc_set(n, theme().MUTED, NCSTYLE_NONE);
            ncplane_putstr_yx(n, r, right_x + 3, str_trunc(l.text, text_w - 3).c_str());
            break;
        case Kind::Option:
            nc_set(n, l.selected ? theme().LOW : theme().FG_DIM, NCSTYLE_NONE);
            ncplane_putstr_yx(n, r, right_x, str_trunc(std::string(l.selected ? "   > " : "     ") + l.text, text_w).c_str());
            break;
        case Kind::PanelRow:
            nc_set(n, l.dim ? theme().FG_DIM : theme().LOW, NCSTYLE_NONE);
            ncplane_putstr_yx(n, r, right_x, str_trunc(std::string(l.selected ? " > " : "   ") + l.text, text_w).c_str());
            break;
        case Kind::Refresh: {
            nc_set(n, theme().LABEL);
            const std::string lead = std::string(focused ? " > " : "   ") + "Refresh: ";
            ncplane_putstr_yx(n, r, right_x, str_trunc(lead, text_w).c_str());
            nc_set(n, focused ? theme().LOW : theme().FG, NCSTYLE_NONE);
            const int lx = static_cast<int>(lead.size());
            if (lx < text_w)
                ncplane_putstr_yx(n, r, right_x + lx, str_trunc(std::to_string(G.refresh_ms) + "ms", text_w - lx).c_str());
            break;
        }
        }
    }
    if (bar) draw_scrollbar(n, ix + iw - 1, iy, vis, G.settings_right_top, vis, total);
}
