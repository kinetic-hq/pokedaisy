#include "pd_menu.h"

#include <stddef.h>
#include <string.h>

#include "pd_style.h"

#define MARGIN 4
#define TITLE_H 24
#define ROW_H 18
#define LIST_Y (MARGIN + TITLE_H + 4)

// pd_list_box's frame: 1 + 1 + 2 + 1 + 2 px (the list window's layers).
#define LIST_FRAME 7
// The library rail's width (the app's LibraryNav, narrowed to the canvas) and a header button's height.
#define RAIL_W 76
#define ACTION_H (TITLE_H - 6)
#define HINT_H 16

static bool is_group(const char* item) { return item && item[0] == PD_MENU_GROUP[0]; }

// "LABEL\tVALUE": the value part (NULL if none); a row whose value starts with PD_MENU_MUTED is N/A.
static const char* value_of(const char* item) {
    const char* t = item ? strchr(item, '\t') : NULL;
    return t ? t + 1 : NULL;
}

bool pd_menu_pickable(const struct pd_menu* m, int row) {
    if (row < 0 || row >= m->count || is_group(m->items[row])) return false;
    const char* v = value_of(m->items[row]);
    return !(v && v[0] == PD_MENU_MUTED[0]);
}

static int list_x(const struct pd_menu* m) { return MARGIN + (m->navCount ? RAIL_W + MARGIN : 0); }
static int list_bottom(const struct pd_menu* m, int h) {
    return h - MARGIN - (m->hint ? HINT_H : 0) - (m->keys ? HINT_H : 0);
}

static int rows_for(const struct pd_menu* m, int h) {
    return (list_bottom(m, h) - LIST_Y - 2 * LIST_FRAME - 4) / ROW_H;
}

void pd_menu_init(struct pd_menu* m, const char* title, const char* const* items, int count) {
    m->title = title;
    m->items = items;
    m->count = count;
    m->selected = 0;
    m->scroll = 0;
    m->touching = false;
    m->pressed = -1;
    m->emptyPath = NULL;
    m->nav = NULL;
    m->navCount = m->navSelected = 0;
    m->actions = NULL;
    m->actionCount = m->actionSelected = 0;
    m->focus = PD_MENU_FOCUS_LIST;
    m->navPicked = m->actionPicked = -1;
    m->pressedNav = m->pressedAction = -1;
    m->hint = NULL;
    m->keys = NULL;
    m->overlay = false;
    m->drawnW = m->drawnH = 0;
    // Start on a row that can be picked.
    if (!pd_menu_pickable(m, 0)) pd_menu_move(m, 1);
}

static void keep_visible(struct pd_menu* m, int rows) {
    if (m->selected < m->scroll) m->scroll = m->selected;
    if (m->selected >= m->scroll + rows) m->scroll = m->selected - rows + 1;
    if (m->scroll < 0) m->scroll = 0;
}

// The rail: each item a framed block, the open one white with a red label (LibraryNav), the focused one outlined.
static void draw_rail(struct pd_menu* m, struct pd_canvas* c) {
    int y = LIST_Y;
    for (int i = 0; i < m->navCount; i++) {
        bool open = i == m->navSelected, focused = m->focus == PD_MENU_FOCUS_NAV && open;
        bool pressed = i == m->pressedNav;
        pd_title_box(c, MARGIN, y, RAIL_W, 30, open || pressed ? PD_TITLE_FILL : PD_LIST_FILL);
        pd_text_fit(c, MARGIN + 6, pd_text_y(y, 30), RAIL_W - 12, m->nav[i], open ? PD_VALUE : PD_LABEL,
                    open ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
        if (focused) {
            pd_fill(c, MARGIN, y, RAIL_W, 2, PD_VALUE);
            pd_fill(c, MARGIN, y + 28, RAIL_W, 2, PD_VALUE);
            pd_fill(c, MARGIN, y, 2, 30, PD_VALUE);
            pd_fill(c, MARGIN + RAIL_W - 2, y, 2, 30, PD_VALUE);
        }
        y += 34;
    }
}

static int action_w(const char* label) { return pd_text_width(label) + 14; }

// The header's buttons at the title window's right, right to left as given.
static void draw_actions(struct pd_menu* m, struct pd_canvas* c, int right) {
    int x = right;
    for (int i = m->actionCount - 1; i >= 0; i--) {
        int bw = action_w(m->actions[i]);
        x -= bw;
        bool on = (m->focus == PD_MENU_FOCUS_ACTIONS && i == m->actionSelected) || i == m->pressedAction;
        pd_title_box(c, x, MARGIN + 3, bw, ACTION_H, on ? PD_TITLE_FILL : PD_LIST_FILL);
        pd_text(c, x + 7, pd_text_y(MARGIN + 3, ACTION_H), m->actions[i], on ? PD_VALUE : PD_TITLE_TEXT,
                on ? PD_VALUE_SHADOW : PD_TITLE_SHADOW);
        x -= 4;
    }
}

void pd_menu_draw(struct pd_menu* m, struct pd_canvas* c) {
    m->drawnW = c->w;
    m->drawnH = c->h;
    if (!m->overlay) pd_backdrop(c);
    int w = c->w - 2 * MARGIN;
    int f = pd_title_box(c, MARGIN, MARGIN, w, TITLE_H, PD_TITLE_FILL);
    pd_text(c, MARGIN + f + 6, pd_text_y(MARGIN, TITLE_H), m->title, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    if (m->actionCount) draw_actions(m, c, MARGIN + w - f - 2);
    if (m->navCount) draw_rail(m, c);

    int lx = list_x(m), lw = c->w - lx - MARGIN;
    pd_list_box(c, lx, LIST_Y, lw, list_bottom(m, c->h) - LIST_Y);
    if (m->hint) pd_text_fit(c, lx + 4, list_bottom(m, c->h) + 1, lw - 8, m->hint, PD_ON_BACKDROP, PD_ON_BACKDROP_SHADOW);
    if (m->keys) {
        int ky = list_bottom(m, c->h) + (m->hint ? HINT_H : 0) + 1;
        pd_text_fit(c, lx + 4, ky, lw - 8, m->keys, PD_VALUE, PD_VALUE_SHADOW);
    }
    int rows = rows_for(m, c->h);
    keep_visible(m, rows);
    int x = lx + LIST_FRAME, iw = lw - 2 * LIST_FRAME;
    int y = LIST_Y + LIST_FRAME + 2;
    if (m->count == 0) {
        pd_text(c, x + 6, y, "No games found.", PD_LABEL, PD_LABEL_SHADOW);
        pd_text(c, x + 6, y + ROW_H, m->emptyPath ? "Put your games in" : "Put .gba files in", PD_MUTED, PD_MUTED_SHADOW);
        pd_text(c, x + 6, y + 2 * ROW_H, m->emptyPath ? m->emptyPath : "sdmc:/pokedaisy/roms/", PD_VALUE,
                PD_VALUE_SHADOW);
        return;
    }
    for (int r = 0; r < rows && m->scroll + r < m->count; r++) {
        int i = m->scroll + r;
        const char* item = m->items[i];
        int ry = y + r * ROW_H, ty = pd_text_y(ry, ROW_H);
        if (is_group(item)) {
            // A group title: the app's red caps over its rows (Gen 1's black band where the look has one).
            const char* t = item + 1;
            if (pd_cur->groupTitleFill != PD_NO_SHADOW) pd_fill(c, x + 2, ry + 1, pd_text_width(t) + 8, ROW_H - 2, pd_cur->groupTitleFill);
            pd_text(c, x + 6, ty, t, pd_cur->groupTitleText, pd_cur->groupTitleShadow);
            continue;
        }
        bool cursor = m->focus == PD_MENU_FOCUS_LIST && (i == m->selected || i == m->pressed);
        if (cursor) pd_fill(c, x, ry, iw, ROW_H, PD_ROW_SELECTED);
        const char* value = value_of(item);
        int n = (int) strlen(item);
        bool page = n && item[n - 1] == PD_MENU_PAGE[0];
        char label[160];
        int ln = value ? (int) (value - 1 - item) : n - (page ? 1 : 0);
        if (ln >= (int) sizeof(label)) ln = (int) sizeof(label) - 1;
        memcpy(label, item, (size_t) ln);
        label[ln] = 0;
        bool muted = value && value[0] == PD_MENU_MUTED[0];
        int right = x + iw - 6 - (page ? 10 : 0);
        if (page) pd_triangle(c, x + iw - 12, ry + 5, 4, PD_RIGHT, muted ? PD_MUTED : PD_VALUE);
        if (value) {
            char v[96];
            const char* vs = muted ? value + 1 : value;
            int vn = (int) strlen(vs) - (page ? 1 : 0);
            if (vn < 0) vn = 0;
            if (vn >= (int) sizeof(v)) vn = (int) sizeof(v) - 1;
            memcpy(v, vs, (size_t) vn);
            v[vn] = 0;
            // The app's OPTION rows: labels in the left part, values from 55% (further right past a long label).
            int vx = x + 6 + iw * 55 / 100, past = x + 6 + pd_text_width(label) + 10;
            if (vx < past) vx = past;
            pd_text_fit(c, x + 6, ty, vx - x - 12, label, muted ? PD_MUTED : PD_LABEL, muted ? PD_MUTED_SHADOW : PD_LABEL_SHADOW);
            if (right > vx)
                pd_text_fit(c, vx, ty, right - vx, v, muted ? PD_MUTED : PD_VALUE, muted ? PD_MUTED_SHADOW : PD_VALUE_SHADOW);
        } else {
            pd_text_fit(c, x + 6, ty, right - x - 6, label, cursor ? PD_VALUE : PD_LABEL,
                        cursor ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
        }
    }
    // A scroll bar when the list runs past the window.
    if (m->count > rows) {
        int track = rows * ROW_H;
        int th = track * rows / m->count;
        int ty = track * m->scroll / m->count;
        pd_fill(c, x + iw - 3, y + ty, 2, th < 6 ? 6 : th, PD_MUTED);
    }
}

void pd_menu_move(struct pd_menu* m, int delta) {
    if (!m->count) return;
    int step = delta < 0 ? -1 : 1, moves = delta < 0 ? -delta : delta;
    for (int k = 0; k < moves; k++) {
        int s = m->selected;
        for (int tries = 0; tries < m->count; tries++) {
            s = (s + step + m->count) % m->count;
            if (pd_menu_pickable(m, s)) break;
        }
        m->selected = s;
    }
}

static int row_at(const struct pd_menu* m, int x, int y, int rows, int w) {
    int top = LIST_Y + LIST_FRAME + 2, lx = list_x(m);
    if (x < lx + LIST_FRAME || x >= lx + w - LIST_FRAME || y < top) return -1;
    int r = (y - top) / ROW_H;
    if (r >= rows || m->scroll + r >= m->count || !pd_menu_pickable(m, m->scroll + r)) return -1;
    return m->scroll + r;
}

static int nav_at(const struct pd_menu* m, int x, int y) {
    if (!m->navCount || x < MARGIN || x >= MARGIN + RAIL_W || y < LIST_Y) return -1;
    int i = (y - LIST_Y) / 34;
    return i < m->navCount && (y - LIST_Y) % 34 < 30 ? i : -1;
}

static int action_at(const struct pd_menu* m, int x, int y, int cw) {
    if (!m->actionCount || y < MARGIN + 3 || y >= MARGIN + 3 + ACTION_H) return -1;
    int bx = cw - MARGIN - 4 - 2; // the title box's frame is about 4 px
    for (int i = m->actionCount - 1; i >= 0; i--) {
        int bw = action_w(m->actions[i]);
        bx -= bw;
        if (x >= bx && x < bx + bw) return i;
        bx -= 4;
    }
    return -1;
}

int pd_menu_touch(struct pd_menu* m, int x, int y, bool down) {
    // The canvas these rows were last drawn on (the 3DS's bottom screen, 320x240, before any draw).
    int cw = m->drawnW ? m->drawnW : 320, ch = m->drawnH ? m->drawnH : 240;
    int rows = rows_for(m, ch);
    int row = row_at(m, x, y, rows, cw - list_x(m) - MARGIN);
    int nav = nav_at(m, x, y), act = action_at(m, x, y, cw);
    if (down) {
        if (!m->touching) {
            m->touching = true;
            m->pressed = row;
            m->pressedNav = nav;
            m->pressedAction = act;
        } else {
            if (row != m->pressed) m->pressed = -1;
            if (nav != m->pressedNav) m->pressedNav = -1;
            if (act != m->pressedAction) m->pressedAction = -1;
        }
        return -1;
    }
    if (!m->touching) return -1;
    m->touching = false;
    int picked = m->pressed;
    m->pressed = -1;
    if (m->pressedNav >= 0) m->navPicked = m->pressedNav;
    if (m->pressedAction >= 0) m->actionPicked = m->pressedAction;
    m->pressedNav = m->pressedAction = -1;
    if (picked >= 0) {
        m->selected = picked;
        m->focus = PD_MENU_FOCUS_LIST;
    }
    return picked;
}

void pd_splash_draw(struct pd_canvas* c, const char* line1, const char* line2, const char* line3) {
    pd_backdrop(c);
    int w = 300, h = 120;
    int x = (c->w - w) / 2, y = (c->h - h) / 2;
    int f = pd_title_box(c, x, y, w, h, PD_TITLE_FILL);
    const char* name = "POKéDAISY";
    pd_text(c, x + (w - pd_text_width(name)) / 2, y + f + 6, name, PD_VALUE, PD_VALUE_SHADOW);
    const char* lines[3] = { line1, line2, line3 };
    for (int i = 0; i < 3; i++) {
        if (!lines[i]) continue;
        pd_text_fit(c, x + f + 8, y + f + 34 + i * 20, w - 2 * (f + 8), lines[i], PD_LABEL, PD_LABEL_SHADOW);
    }
}
