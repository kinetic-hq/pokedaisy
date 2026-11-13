// The NUZLOCKE tab (NuzlockeScreen.kt): one list in SETTINGS' look. HERE (the area the player stands in: free, or what
// its encounter was), TEAM (the run's living Pokemon), ENCOUNTERS (one row an area, newest first), GRAVEYARD and the
// RULES with NEW RUN. Every row the tracker fills can be put right by hand. While NUZLOCKE is off: what it does and
// TURN ON. GPL-3.0, see ../../NOTICE.
#include <stdio.h>
#include <string.h>

#include "pd_icon.h"
#include "pd_nuzlocke.h"
#include "pd_snapshot.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define TITLE_H 24
#define LIST_Y (CONTENT_Y + TITLE_H + 4)
#define ENTRY_H 34 // an icon row: the name and value, a grey line under them

enum nuz_row { ROW_HERE, ROW_TEAM, ROW_ENCOUNTER, ROW_GRAVE, ROW_DUPES, ROW_WARNINGS, ROW_NEW_RUN };
#define MAX_ROWS (PD_NUZ_MAX_ENCOUNTERS + PD_NUZ_MAX_MONS + 8)
static struct {
    enum nuz_row kind;
    int key; // the encounter's area, or the mon's index
} rows[MAX_ROWS];
static int rowCount;
static char hereName[PD_NUZ_NAME]; // where the player stood when the tab was last drawn (a Pokemon marked fainted)

static const char* outcome_label(enum pd_nuz_outcome o) {
    return o == PD_NUZ_NOW ? "NOW" : o == PD_NUZ_CAUGHT ? "CAUGHT" : "MISSED";
}

static int add_row(enum nuz_row kind, int key) {
    if (rowCount >= MAX_ROWS) return -1;
    rows[rowCount].kind = kind;
    rows[rowCount].key = key;
    return rowCount++;
}

static void group(struct pd_canvas* c, int x, int* y, const char* title) {
    *y += 3;
    if (pd_cur->groupTitleFill != PD_NO_SHADOW) pd_fill(c, x - 4, *y + 1, pd_text_width(title) + 8, ROW_H - 2, pd_cur->groupTitleFill);
    pd_text(c, x, pd_text_y(*y, ROW_H), title, pd_cur->groupTitleText, pd_cur->groupTitleShadow);
    *y += ROW_H;
}

static bool visible(int y, int h, int top, int bottom) { return y >= top && y + h <= bottom; }

// A row with an icon: [label] and [value] on top, [sub] under them in grey.
static void entry(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, int x, int* y, int w, int species,
                  const char* label, const char* value, const char* sub, int row, int top, int bottom) {
    int id = HIT_NUZ_ROW + row;
    if (row >= 0 && ui->pressedId == id) pd_fill(c, x - 4, *y, w + 8, ENTRY_H, PD_ROW_SELECTED);
    int tx = x;
    if (species > 0) {
        pd_draw_mon_icon_shiny(c, g, species, false, x - 2, *y + 1, 1, false);
        tx = x + PD_ICON + 2;
    }
    int vw = value ? pd_text_width(value) : 0;
    pd_text_fit(c, tx, *y + 2, x + w - tx - vw - 8, label, PD_LABEL, PD_LABEL_SHADOW);
    if (value) pd_text(c, x + w - vw, *y + 2, value, PD_VALUE, PD_VALUE_SHADOW);
    if (sub) pd_text_fit(c, tx, *y + 17, x + w - tx, sub, PD_MUTED, PD_MUTED_SHADOW);
    if (row >= 0 && visible(*y, ENTRY_H, top, bottom)) ui_add_scroll_hit(ui, x - 4, *y, w + 8, ENTRY_H, id);
    *y += ENTRY_H;
}

// A plain row (the rules, a "none yet").
static void plain(struct pd_ui* ui, struct pd_canvas* c, int x, int* y, int w, const char* label, const char* value,
                  int row, int top, int bottom) {
    if (row < 0) {
        pd_text(c, x, pd_text_y(*y, ROW_H), label, PD_MUTED, PD_MUTED_SHADOW);
    } else {
        ui_row(c, x, *y, w, label, value, ui->pressedId == HIT_NUZ_ROW + row);
        if (visible(*y, ROW_H, top, bottom)) ui_add_scroll_hit(ui, x - 4, *y, w + 8, ROW_H, HIT_NUZ_ROW + row);
    }
    *y += ROW_H;
}

static void off_page(struct pd_ui* ui, struct pd_canvas* c, const struct pd_nuzlocke* n) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_title_box(c, x, CONTENT_Y, w, TITLE_H, PD_TITLE_FILL);
    int ty = pd_text_y(CONTENT_Y, TITLE_H);
    pd_text(c, x + f + 6, ty, "NUZLOCKE", PD_TITLE_TEXT, PD_TITLE_SHADOW);
    const char* state = n && n->supported ? "OFF" : "N/A";
    pd_text(c, x + w - f - 6 - pd_text_width(state), ty, state, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    int lh = c->h - LIST_Y - MARGIN;
    int lf = pd_list_box(c, x, LIST_Y, w, lh);
    int ix = x + lf + 6, iw = w - 2 * (lf + 6), y = LIST_Y + lf + 4;
    if (!n || !n->supported) {
        y += 16 * pd_text_wrap(c, ix, y, iw, 16, 6,
                               "NUZLOCKE works in the Game Boy Advance games. It isn't there for this one yet.", PD_LABEL,
                               PD_LABEL_SHADOW);
        return;
    }
    y += 16 * pd_text_wrap(c, ix, y, iw, 16, 6,
                           "NUZLOCKE tracking is off for this game. Turn it on and the companion keeps score of your run: "
                           "only the first wild POKéMON in each area counts, and a POKéMON that faints is out for good.",
                           PD_LABEL, PD_LABEL_SHADOW) + 6;
    y += 16 * pd_text_wrap(c, ix, y, iw, 16, 3, "You can turn it off again in SETTINGS, under GAMEPLAY.", PD_MUTED,
                           PD_MUTED_SHADOW) + 8;
    ui->startId = HIT_NUZ_ON;
    ui_button(ui, c, ix, y, iw, 26, "TURN ON", HIT_NUZ_ON);
}

void ui_nuzlocke_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    struct pd_nuzlocke* n = ui->nuzlocke;
    rowCount = 0;
    if (!n || !n->on || !n->supported) {
        off_page(ui, c, n);
        return;
    }
    const struct pd_nuz_run* r = &n->run;
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_title_box(c, x, CONTENT_Y, w, TITLE_H, PD_TITLE_FILL);
    int ty = pd_text_y(CONTENT_Y, TITLE_H);
    pd_text(c, x + f + 6, ty, "NUZLOCKE", PD_TITLE_TEXT, PD_TITLE_SHADOW);
    char buf[96], sub[96], nameBuf[32];
    snprintf(buf, sizeof(buf), "%d ALIVE, %d FAINTED", pd_nuz_alive(r), pd_nuz_dead(r));
    pd_text(c, x + w - f - 6 - pd_text_width(buf), ty, buf, PD_TITLE_TEXT, PD_TITLE_SHADOW);

    int lh = c->h - LIST_Y - MARGIN;
    int lf = pd_list_box(c, x, LIST_Y, w, lh);
    int top = LIST_Y + lf, bottom = LIST_Y + lh - lf, view = bottom - top;
    static int lastTotal;
    bool bar = lastTotal > view;
    int ix = x + lf + 4, iw = w - 2 * (lf + 4) - (bar ? SCROLL_BAR_W + 2 : 0);
    ui->nuzScroll = ui_clamp_scroll(ui->nuzScroll, lastTotal, view);
    ui_add_scroll_hit(ui, x, top, w, view, HIT_NUZ_LIST);
    int cx0 = c->cx0, cy0 = c->cy0, cx1 = c->cx1, cy1 = c->cy1;
    pd_clip(c, x, top, w, view);
    int y = top + 2 - ui->nuzScroll;

    group(c, ix, &y, "HERE");
    const char* here = s && s->valid ? pd_mapsec_name(g, s->mapsec) : NULL;
    snprintf(hereName, sizeof(hereName), "%s", here ? here : "");
    const struct pd_nuz_encounter* he = s && s->valid ? pd_nuz_encounter_at(r, s->mapsec) : NULL;
    if (!he) {
        entry(ui, c, g, ix, &y, iw, 0, here && *here ? here : "?", "FREE", "Your first wild POKéMON here is this area's catch",
              -1, top, bottom);
    } else {
        int row = add_row(ROW_HERE, he->area);
        entry(ui, c, g, ix, &y, iw, he->species, he->areaName, outcome_label(he->outcome),
              pd_species_name(g, he->species, nameBuf, sizeof(nameBuf)), row, top, bottom);
    }

    group(c, ix, &y, "TEAM");
    if (!pd_nuz_alive(r)) plain(ui, c, ix, &y, iw, "Nobody yet", NULL, -1, top, bottom);
    for (int i = 0; i < r->monCount; i++) {
        const struct pd_nuz_mon* m = &r->mons[i];
        if (m->dead) continue;
        snprintf(buf, sizeof(buf), "Lv%d", m->level);
        if (*m->areaName) snprintf(sub, sizeof(sub), "Joined in %s", m->areaName);
        else snprintf(sub, sizeof(sub), "With you from the start");
        entry(ui, c, g, ix, &y, iw, m->species, m->name, buf, sub, add_row(ROW_TEAM, i), top, bottom);
    }

    group(c, ix, &y, "ENCOUNTERS");
    if (!r->encounterCount) plain(ui, c, ix, &y, iw, "None yet", NULL, -1, top, bottom);
    for (int i = r->encounterCount - 1; i >= 0; i--) {
        const struct pd_nuz_encounter* e = &r->encounters[i];
        entry(ui, c, g, ix, &y, iw, e->species, e->areaName, outcome_label(e->outcome),
              pd_species_name(g, e->species, nameBuf, sizeof(nameBuf)), add_row(ROW_ENCOUNTER, e->area), top, bottom);
    }

    if (pd_nuz_dead(r)) {
        group(c, ix, &y, "GRAVEYARD");
        for (int i = 0; i < r->monCount; i++) {
            const struct pd_nuz_mon* m = &r->mons[i];
            if (!m->dead) continue;
            snprintf(buf, sizeof(buf), "Lv%d", m->diedLevel);
            snprintf(sub, sizeof(sub), "Fainted in %s", m->diedIn);
            entry(ui, c, g, ix, &y, iw, m->species, m->name, buf, *m->diedIn ? sub : NULL, add_row(ROW_GRAVE, i), top, bottom);
        }
    }

    group(c, ix, &y, "RULES");
    plain(ui, c, ix, &y, iw, "DUPES CLAUSE", r->dupes ? "ON" : "OFF", add_row(ROW_DUPES, 0), top, bottom);
    plain(ui, c, ix, &y, iw, "WARNINGS", r->warnings ? "ON" : "OFF", add_row(ROW_WARNINGS, 0), top, bottom);
    plain(ui, c, ix, &y, iw, "NEW RUN", NULL, add_row(ROW_NEW_RUN, 0), top, bottom);
    y += 4;

    c->cx0 = cx0, c->cy0 = cy0, c->cx1 = cx1, c->cy1 = cy1;
    lastTotal = y + ui->nuzScroll - top;
    if (lastTotal > view) ui_scroll_rail(ui, c, x + w - lf - SCROLL_BAR_W - 2, top + 2, SCROLL_BAR_W, view - 4, ui->nuzScroll, lastTotal, view);
}

// Pick-list and confirm windows (the SETTINGS ones).
static const char* const PICKS[3] = {"CAUGHT", "MISSED", "NOT USED"};

void ui_nuzlocke_overlay(struct pd_ui* ui, struct pd_canvas* c) {
    const struct pd_nuzlocke* n = ui->nuzlocke;
    if (!n) return;
    if (ui->overlay == PD_OVERLAY_NUZ_PICK) {
        const struct pd_nuz_encounter* e = pd_nuz_encounter_at(&n->run, ui->nuzKey);
        int current = e && e->outcome == PD_NUZ_CAUGHT ? 0 : e && e->outcome == PD_NUZ_MISSED ? 1 : -1;
        ui_pick_list(ui, c, e ? e->areaName : "ENCOUNTER", PICKS, 3, current);
    } else if (ui->overlay == PD_OVERLAY_NUZ_CONFIRM) {
        char title[64], text[160];
        const struct pd_nuz_mon* m = ui->nuzKey >= 0 && ui->nuzKey < n->run.monCount ? &n->run.mons[ui->nuzKey] : NULL;
        if (ui->nuzConfirm == ROW_NEW_RUN) {
            ui_confirm(ui, c, "NEW RUN?",
                       "Forgets every encounter and fainted POKéMON of this run. Your party right now starts the new one.", "YES");
        } else if (m && ui->nuzConfirm == ROW_TEAM) {
            snprintf(title, sizeof(title), "%s FAINTED?", m->name);
            snprintf(text, sizeof(text), "Marks %s as out of the run, as if it had fainted.", m->name);
            ui_confirm(ui, c, title, text, "YES");
        } else if (m) {
            snprintf(title, sizeof(title), "BRING BACK %s?", m->name);
            snprintf(text, sizeof(text), "Puts %s back in the run, for a faint that shouldn't count.", m->name);
            ui_confirm(ui, c, title, text, "YES");
        }
    }
}

void ui_grave_mark(const struct pd_ui* ui, struct pd_canvas* c, const struct pd_mon* m, int x, int y, int scale) {
    if (scale < 1) scale = 1;
    if (!m || !pd_nuz_is_dead(ui->nuzlocke, m->personality)) return;
    static const char* const STONE[12] = {"...KKKKK...", "..KWWWWWK..", ".KWWWKWWWK.", ".KWWWKWWWK.", ".KWKKKKKWK.",
                                          ".KWWWKWWWK.", ".KWWWKWWWK.", ".KWWWWWWWK.", ".KWWWWWWWK.", ".KGGGGGGGK.",
                                          "KKKKKKKKKKK", "KGGGGGGGGGK"};
    static const uint32_t COLORS[3] = {0x303030, 0xC8C8D0, 0x68A048};
    for (int r = 0; r < 12; r++)
        for (int col = 0; col < 11; col++) {
            char k = STONE[r][col];
            if (k != '.') pd_fill(c, x + col * scale, y + r * scale, scale, scale, COLORS[k == 'K' ? 0 : k == 'W' ? 1 : 2]);
        }
}

bool ui_nuzlocke_supported(const struct pd_ui* ui) { return ui->nuzlocke && ui->nuzlocke->supported; }
bool ui_nuzlocke_on(const struct pd_ui* ui) { return ui_nuzlocke_supported(ui) && ui->nuzlocke->on; }
void ui_nuzlocke_toggle(struct pd_ui* ui) {
    if (!ui_nuzlocke_supported(ui)) return;
    ui->nuzlocke->on = !ui->nuzlocke->on;
    ui->nuzlocke->dirty = true;
}

bool ui_nuzlocke_act(struct pd_ui* ui, int id) {
    struct pd_nuzlocke* n = ui->nuzlocke;
    if (!n) return false;
    if (ui->overlay == PD_OVERLAY_NUZ_PICK) {
        if (id >= HIT_OPTION && id < HIT_OPTION + 3) {
            int i = id - HIT_OPTION;
            pd_nuz_set_outcome(n, ui->nuzKey, i == 0 ? PD_NUZ_CAUGHT : i == 1 ? PD_NUZ_MISSED : -1);
            ui->overlay = PD_OVERLAY_NONE;
        } else if (id == HIT_NO || id == HIT_SCRIM) {
            ui->overlay = PD_OVERLAY_NONE;
        }
        return true;
    }
    if (ui->overlay == PD_OVERLAY_NUZ_CONFIRM) {
        if (id == HIT_YES) {
            if (ui->nuzConfirm == ROW_NEW_RUN) {
                pd_nuz_new_run(n);
            } else if (ui->nuzKey >= 0 && ui->nuzKey < n->run.monCount) {
                pd_nuz_set_dead(n, n->run.mons[ui->nuzKey].personality, ui->nuzConfirm == ROW_TEAM, hereName);
            }
            ui->overlay = PD_OVERLAY_NONE;
        } else if (id == HIT_NO || id == HIT_SCRIM) {
            ui->overlay = PD_OVERLAY_NONE;
        }
        return true;
    }
    if (ui->overlay != PD_OVERLAY_NONE) return false;
    if (id == HIT_NUZ_ON && n->supported) {
        n->on = true;
        n->dirty = true;
        return true;
    }
    if (ui->tab != PD_TAB_NUZLOCKE || id < HIT_NUZ_ROW || id >= HIT_NUZ_ROW + rowCount) return false;
    int i = id - HIT_NUZ_ROW;
    switch (rows[i].kind) {
    case ROW_HERE:
    case ROW_ENCOUNTER:
        ui->nuzKey = rows[i].key;
        ui->overlay = PD_OVERLAY_NUZ_PICK;
        break;
    case ROW_TEAM:
    case ROW_GRAVE:
    case ROW_NEW_RUN:
        ui->nuzKey = rows[i].key;
        ui->nuzConfirm = (int)rows[i].kind;
        ui->overlay = PD_OVERLAY_NUZ_CONFIRM;
        break;
    case ROW_DUPES:
        n->run.dupes = !n->run.dupes;
        n->dirty = true;
        break;
    case ROW_WARNINGS:
        n->run.warnings = !n->run.warnings;
        n->dirty = true;
        break;
    }
    return true;
}
