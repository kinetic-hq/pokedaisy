// COMPANION VIEW's narrow panels for the Linux / Windows host's wings: the party down the left, and the strips above
// and below the game. Sized from the canvas they're given, never wider than a wing.
#include <stdio.h>

#include "pd_icon.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define LINE 16 // PD_FONT_HEIGHT

void pd_ui_party_panel(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    ui_game = g;
    int f = pd_title_box(c, 0, 0, c->w, c->h, PD_TITLE_FILL);
    int x = f + 4, y = f + 3, w = c->w - 2 * (f + 4), bottom = c->h - f - 3;
    pd_text(c, x, y, "PARTY", PD_VALUE, PD_VALUE_SHADOW);
    y += LINE + 3;
    if (!s->valid || s->partyCount <= 0) {
        pd_text_wrap(c, x, y, w, LINE, 2, "NO PARTY YET", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    // A slot: the icon and name, then the level and the HP bar, as tall as the room gives (at least the icon's).
    int rowMin = PD_ICON + 6, rowH = (bottom - y) / PD_PARTY_SIZE;
    if (rowH < rowMin) rowH = rowMin;
    char buf[40];
    for (int i = 0; i < s->partyCount && i < PD_PARTY_SIZE && y + rowMin <= bottom; i++, y += rowH) {
        const struct pd_mon* m = &s->party[i];
        bool fainted = m->species != PD_SPECIES_EGG && m->maxHp > 0 && m->hp == 0;
        pd_round_rect(c, x, y, w, rowH - 3, 3, PD_LIST_FILL);
        if (!pd_draw_mon_icon_shiny(c, g, m->species, m->shiny, x + 1, y + (rowH - 3 - PD_ICON) / 2, 1, fainted))
            ui_ball(c, x + 13, y + (rowH - 3) / 2 - 4, true);
        int tx = x + PD_ICON + 4, right = x + w - 3;
        const char* name = m->nickname[0] ? m->nickname : pd_species_name(g, m->species, buf, sizeof(buf));
        int ty = y + (rowH - 3 - 2 * LINE) / 2;
        if (m->species == PD_SPECIES_EGG) {
            pd_text_fit(c, tx, ty + LINE / 2, right - tx, name, PD_LABEL, PD_LABEL_SHADOW);
            continue;
        }
        char lv[12];
        snprintf(lv, sizeof(lv), "Lv%d", m->level);
        int lvW = pd_text_width(lv);
        // Too narrow for the level beside the name: it goes before the HP bar.
        bool lvRight = right - tx >= lvW + 40;
        pd_text_fit(c, tx, ty, right - tx - (lvRight ? lvW + 4 : 0), name, PD_LABEL, PD_LABEL_SHADOW);
        if (lvRight) pd_text_right(c, right, ty, lv, PD_MUTED, PD_MUTED_SHADOW);
        int by = ty + LINE + 1, bx = tx;
        if (!lvRight) bx += pd_text(c, tx, by, lv, PD_MUTED, PD_MUTED_SHADOW) + 4;
        const char* status = pd_status_label(m->status);
        if (*status) {
            ui_status_badge(c, bx, by + 1, m->status);
            bx += pd_text_width(status) + 10;
        }
        int barW = right - bx - 18;
        if (barW >= 12) ui_hp_bar(c, bx, by + 5, barW, m->hp, m->maxHp);
    }
}

void pd_ui_strip(struct pd_canvas* c, const char* left, const char* middle, const char* right) {
    int f = pd_title_box(c, 0, 0, c->w, c->h, PD_TITLE_FILL);
    int y = pd_text_y(0, c->h), x = f + 6, r = c->w - f - 6;
    int lw = left && *left ? pd_text_width(left) : 0, rw = right && *right ? pd_text_width(right) : 0;
    if (lw) pd_text_fit(c, x, y, r - x, left, PD_LABEL, PD_LABEL_SHADOW);
    if (rw && x + lw + 12 + rw <= r) pd_text_right(c, r, y, right, PD_LABEL, PD_LABEL_SHADOW);
    else rw = 0;
    if (middle && *middle) {
        int mw = pd_text_width(middle), mx = (c->w - mw) / 2;
        // Only where it clears both sides.
        if (mx >= x + lw + 12 && mx + mw + 12 <= r - rw) pd_text(c, mx, y, middle, PD_VALUE, PD_VALUE_SHADOW);
    }
}
