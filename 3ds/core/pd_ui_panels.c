// COMPANION VIEW's narrow panels for the Linux / Windows host's wings: the party down the left, and the strips above
// and below the game. Sized from the canvas they're given, never wider than a wing.
#include <stdio.h>
#include <string.h>

#include "pd_icon.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define LINE 16 // PD_FONT_HEIGHT

void pd_ui_party_panel(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    pd_ui_party_panel_icons(c, g, s, NULL, NULL);
}

void pd_ui_party_panel_icons(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                             pd_ui_icon_fn icon, void* ctx) {
    ui_game = g;
    int f = pd_title_box(c, 0, 0, c->w, c->h, PD_TITLE_FILL);
    int x = f + 4, y = f + 3, w = c->w - 2 * (f + 4), bottom = c->h - f - 3;
    pd_text(c, x, y, "PARTY", PD_VALUE, PD_VALUE_SHADOW);
    y += LINE + 3;
    if (!s->valid || s->partyCount <= 0) {
        pd_text_wrap(c, x, y, w, LINE, 2, "NO PARTY YET", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    // A card a slot, the six sharing the wing's height (as tall as the room gives, at least the icon's): the icon
    // and name, then the level and the HP bar; a tall card (a wing beside a full-height game) puts the HP numbers
    // under the bar.
    int rowMin = PD_ICON + 6, rowH = (bottom - y) / PD_PARTY_SIZE;
    if (rowH < rowMin) rowH = rowMin;
    char buf[40];
    // The places of a party not yet six: an empty card each, so the wing reads as six evenly whatever's in it.
    for (int i = s->partyCount < 0 ? 0 : s->partyCount, ey = y + i * rowH; i < PD_PARTY_SIZE && ey + rowMin <= bottom;
         i++, ey += rowH) {

        pd_round_rect(c, x, ey, w, rowH - 4, 3, PD_LIST_FILL);
        pd_round_rect(c, x + 2, ey + 2, w - 4, rowH - 8, 2, PD_TITLE_FILL);
    }
    for (int i = 0; i < s->partyCount && i < PD_PARTY_SIZE && y + rowMin <= bottom; i++, y += rowH) {
        const struct pd_mon* m = &s->party[i];
        bool fainted = m->species != PD_SPECIES_EGG && m->maxHp > 0 && m->hp == 0;
        int cardH = rowH - 4;
        bool tall = cardH >= 3 * LINE + 10;
        pd_round_rect(c, x, y, w, cardH, 3, PD_LIST_FILL);
        if (icon ? !icon(c, ctx, i, x + 1, y + (cardH - PD_ICON) / 2, fainted)
                 : !pd_draw_mon_icon_shiny(c, g, m->species, m->shiny, x + 1, y + (cardH - PD_ICON) / 2, 1, fainted))
            ui_ball(c, x + 13, y + cardH / 2 - 4, true);
        int tx = x + PD_ICON + 4, right = x + w - 3;
        const char* name = m->nickname[0] || !g ? m->nickname : pd_species_name(g, m->species, buf, sizeof(buf));
        int ty = y + (cardH - (tall ? 3 * LINE + 2 : 2 * LINE)) / 2;
        if (m->species == PD_SPECIES_EGG) {
            pd_text_fit(c, tx, y + (cardH - LINE) / 2, right - tx, name, PD_LABEL, PD_LABEL_SHADOW);
            continue;
        }
        char lv[12];
        snprintf(lv, sizeof(lv), "Lv%d", m->level);
        int lvW = pd_text_width(lv);
        // Too narrow for the level beside the name: it goes before the HP bar.
        bool lvRight = !tall && right - tx >= lvW + 40; // a tall card: the whole name, the level under it
        pd_text_fit(c, tx, ty, right - tx - (lvRight ? lvW + 4 : 0), name, PD_LABEL, PD_LABEL_SHADOW);
        if (lvRight) pd_text_right(c, right, ty, lv, PD_MUTED, PD_MUTED_SHADOW);
        int by = ty + LINE + 1, bx = tx;
        if (!lvRight) bx += pd_text(c, tx, by, lv, PD_MUTED, PD_MUTED_SHADOW) + 4;
        const char* status = pd_status_label(m->status);
        if (*status) {
            ui_status_badge(c, bx, by + 1, m->status);
            bx += pd_text_width(status) + 10;
        }
        if (tall) {
            // The bar its own line, as wide as the text column, the HP numbers under it.
            by += LINE + 2;
            bx = tx;
            char hp[16];
            snprintf(hp, sizeof(hp), "%d/%d", m->hp, m->maxHp);
            if (right - bx - 18 >= 12) ui_hp_bar(c, bx, by, right - bx - 18, m->hp, m->maxHp);
            pd_text_right(c, right, by + 5, hp, PD_MUTED, PD_MUTED_SHADOW);
            continue;
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

void pd_ui_facts_panel(struct pd_canvas* c, const char* title, const char* const* rows, int count) {
    int f = pd_title_box(c, 0, 0, c->w, c->h, PD_TITLE_FILL);
    int x = f + 4, y = f + 3, w = c->w - 2 * (f + 4), bottom = c->h - f - 3;
    pd_text(c, x, y, title, PD_VALUE, PD_VALUE_SHADOW);
    y += LINE + 3;
    // A fact: its label, then its value, each on a line of its own (a wing is too narrow for both on one).
    for (int i = 0; i < count && y + 2 * LINE <= bottom; i++) {
        const char* tab = strchr(rows[i], '\t');
        char label[32];
        int n = tab ? (int)(tab - rows[i]) : (int)strlen(rows[i]);
        if (n > (int)sizeof(label) - 1) n = (int)sizeof(label) - 1;
        memcpy(label, rows[i], (size_t)n);
        label[n] = 0;
        pd_text_fit(c, x, y, w, label, PD_MUTED, PD_MUTED_SHADOW);
        if (tab) pd_text_fit(c, x + 4, y + LINE, w - 4, tab + 1, PD_LABEL, PD_LABEL_SHADOW);
        y += 2 * LINE + 4;
    }
}
