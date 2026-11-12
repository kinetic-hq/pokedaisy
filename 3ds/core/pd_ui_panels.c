// COMPANION VIEW's narrow panels for the Linux / Windows host's wings: the party down the left, and the strips above
// and below the game. Sized from the canvas they're given, never wider than a wing.
#include <stdio.h>
#include <string.h>

#include "pd_experience.h"
#include "pd_gb.h"
#include "pd_icon.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define LINE 16 // PD_FONT_HEIGHT
#define CARD_R 6 // a card's corner, in canvas pixels (12 on screen at the wings' usual 2x)
#define EASE_MS 400u
#define SLIDE_MS 220u

uint32_t pd_ui_now_ms;
bool pd_ui_reduce_motion;
bool pd_ui_panel_moving;
uint32_t pd_ui_bar_facts;

static uint32_t mix(uint32_t a, uint32_t b, int t256) {
    uint32_t r = 0;
    for (int s = 0; s < 24; s += 8) {
        int ca = (int)(a >> s & 0xFF), cb = (int)(b >> s & 0xFF);
        r |= (uint32_t)(ca + (cb - ca) * t256 / 256) << s;
    }
    return r;
}
static int luma(uint32_t c) { return (int)((c >> 16 & 0xFF) * 3 + (c >> 8 & 0xFF) * 6 + (c & 0xFF)) / 10; }

// A wing card: rounded, a soft top-to-bottom gradient, a faint lighter edge (a glow on a dark look, a shade on a
// light one), and an accent stripe down the left (0 = none).
static void ui_card(struct pd_canvas* c, int x, int y, int w, int h, uint32_t fill, uint32_t accent) {
    bool dark = luma(fill) < 128;
    uint32_t edge = dark ? mix(fill, 0xFFFFFF, 40) : mix(fill, 0x000000, 36);
    pd_round_rect(c, x, y, w, h, CARD_R, edge);
    // The body a pixel in, in bands: lighter at the top, darker at the bottom.
    int bands = h - 2 < 8 ? 1 : 8;
    for (int b = 0; b < bands; b++) {
        int by = y + 1 + (h - 2) * b / bands, bh = y + 1 + (h - 2) * (b + 1) / bands - by;
        int t = 10 - 20 * b / (bands > 1 ? bands - 1 : 1); // +10 .. -10 (of 256)
        uint32_t band = t >= 0 ? mix(fill, 0xFFFFFF, t) : mix(fill, 0x000000, -t);
        // Only the rows inside the rounded body: the first and last bands take the corners.
        if (b == 0) pd_round_rect(c, x + 1, by, w - 2, bh + CARD_R, CARD_R - 1, band);
        else if (b == bands - 1) pd_round_rect(c, x + 1, by - CARD_R, w - 2, bh + CARD_R, CARD_R - 1, band);
        else pd_fill(c, x + 1, by, w - 2, bh, band);
    }
    if (bands == 1) pd_round_rect(c, x + 1, y + 1, w - 2, h - 2, CARD_R - 1, fill);
    // The glow: a lighter line along the top inside the edge.
    if (dark) pd_fill(c, x + CARD_R, y + 1, w - 2 * CARD_R, 1, mix(fill, 0xFFFFFF, 64));
    if (accent) {
        int sy = y + CARD_R / 2 + 2, sh = h - CARD_R - 4;
        if (sh > 4) pd_round_rect(c, x + 3, sy, 3, sh, 1, accent);
    }
}

// How far a Pokemon is to its next level, 0..100 (-1: the game's curve isn't known).
static int exp_filled(const struct pd_game* g, const struct pd_mon* m) {
    if (!g || !g->cfg || !m->hasExperience) return -1;
    struct pd_exp e;
    bool ok = g->cfg->gb ? pd_exp_progress(pd_polished_growth(g, m->species), m->level, m->experience, 100, &e)
                         : !g->hackMaps && pd_retail_exp_progress(m->species, m->level, m->experience, 100, &e);
    return ok ? e.filled : -1;
}

// The EXP line under an HP bar: a thin blue bar, as in the games' summary.
static void exp_line(struct pd_canvas* c, int x, int y, int w, int filled) {
    if (filled < 0 || w < 8) return;
    pd_fill(c, x, y, w, 2, mix(PD_LIST_FILL, 0x000000, 90));
    int f = filled * w / 100;
    if (f > w) f = w;
    if (f > 0) pd_fill(c, x, y, f, 2, 0x48A8F8);
}

// An empty party place's Poke Ball, drawn as an outline.
static void empty_ball(struct pd_canvas* c, int x, int y, uint32_t ink, uint32_t back) {
    pd_round_rect(c, x, y, 12, 12, 6, ink);
    pd_round_rect(c, x + 1, y + 1, 10, 10, 5, back);
    pd_fill(c, x + 1, y + 5, 10, 2, ink);
    pd_round_rect(c, x + 4, y + 4, 4, 4, 2, ink);
    pd_fill(c, x + 5, y + 5, 2, 2, back);
}

// What a party slot showed last draw, for the HP bar's easing and the cards' slide when the party changes.
static struct {
    uint32_t key;       // personality ^ species: the same Pokemon
    int hpFrom, hpTo;   // the bar eases from hpFrom to hpTo
    uint32_t since;
} shown[PD_PARTY_SIZE];
static uint32_t partyKey, slideSince;

// The HP the bar shows now for slot i (eased toward hp over EASE_MS; REDUCE MOTION jumps).
static int eased_hp(int i, uint32_t key, int hp) {
    if (shown[i].key != key || pd_ui_reduce_motion || !pd_ui_now_ms) { // no clock (3DS, previews): no easing
        shown[i].key = key;
        shown[i].hpFrom = shown[i].hpTo = hp;
        shown[i].since = pd_ui_now_ms;
        return hp;
    }
    uint32_t t = pd_ui_now_ms - shown[i].since;
    int now = t >= EASE_MS ? shown[i].hpTo
                           : shown[i].hpFrom + (shown[i].hpTo - shown[i].hpFrom) * (int)t / (int)EASE_MS;
    if (hp != shown[i].hpTo) {
        shown[i].hpFrom = now;
        shown[i].hpTo = hp;
        shown[i].since = pd_ui_now_ms;
        t = 0;
    }
    if (t < EASE_MS) pd_ui_panel_moving = true;
    return now;
}

void pd_ui_party_panel(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    pd_ui_party_panel_icons(c, g, s, NULL, NULL);
}

void pd_ui_party_panel_icons(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                             pd_ui_icon_fn icon, void* ctx) {
    ui_game = g;
    int f = pd_title_box(c, 0, 0, c->w, c->h, PD_TITLE_FILL);
    int x = f + 4, y = f + 3, w = c->w - 2 * (f + 4), bottom = c->h - f - 3;
    pd_heading(c, x, y, "PARTY", PD_VALUE, PD_VALUE_SHADOW);
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
        // An outline card with a Poke Ball's outline where the icon would be: seen on a dark wing too.
        uint32_t ink = luma(PD_TITLE_FILL) < 128 ? mix(PD_TITLE_FILL, 0xFFFFFF, 44) : mix(PD_TITLE_FILL, 0x000000, 44);
        pd_round_rect(c, x, ey, w, rowH - 4, CARD_R, ink);
        pd_round_rect(c, x + 1, ey + 1, w - 2, rowH - 6, CARD_R - 1, PD_TITLE_FILL);
        empty_ball(c, x + 8 + (PD_ICON - 12) / 2, ey + (rowH - 4 - 12) / 2, ink, PD_TITLE_FILL);
    }
    // A new party (a Pokemon in or out, the order changed): the cards slide in from the left, one after another.
    uint32_t key = 2166136261u;
    for (int i = 0; i < s->partyCount && i < PD_PARTY_SIZE; i++)
        key = (key ^ (s->party[i].personality ^ (uint32_t)s->party[i].species)) * 16777619u;
    if (key != partyKey) {
        if (partyKey && !pd_ui_reduce_motion) slideSince = pd_ui_now_ms;
        partyKey = key;
    }
    for (int i = 0; i < s->partyCount && i < PD_PARTY_SIZE && y + rowMin <= bottom; i++, y += rowH) {
        const struct pd_mon* m = &s->party[i];
        bool egg = m->species == PD_SPECIES_EGG;
        bool fainted = !egg && m->maxHp > 0 && m->hp == 0;
        int cardH = rowH - 4;
        bool tall = cardH >= 3 * LINE + 10;
        int slide = 0;
        if (slideSince && !pd_ui_reduce_motion) {
            uint32_t t = pd_ui_now_ms - slideSince, start = (uint32_t)i * 40u;
            if (t < start + SLIDE_MS) {
                uint32_t k = t < start ? 0 : t - start; // 0 .. SLIDE_MS
                int left = (int)(SLIDE_MS - k);
                slide = -(w / 3) * left * left / (int)(SLIDE_MS * SLIDE_MS); // eased out
                pd_ui_panel_moving = true;
            }
        }
        int cx = x + slide;
        int t1 = 0, t2 = 0;
        if (!egg && g) pd_species_type_pair(g, m->species, &t1, &t2);
        uint32_t accent = egg || t1 == PD_TYPE_NONE ? 0 : ui_type_color(t1);
        ui_card(c, cx, y, w, cardH, PD_LIST_FILL, fainted ? mix(accent, PD_LIST_FILL, 160) : accent);
        int iconX = cx + 8, iconY = y + (cardH - PD_ICON) / 2;
        if (icon ? !icon(c, ctx, i, iconX, iconY, fainted)
                 : !pd_draw_mon_icon_shiny(c, g, m->species, m->shiny, iconX, iconY, 1, fainted))
            ui_ball(c, iconX + 12, y + cardH / 2 - 4, true);
        if (!egg && m->heldItem) ui_held_mark(c, iconX + PD_ICON - 8, iconY + PD_ICON - 8);
        int tx = iconX + PD_ICON + 3, right = cx + w - 6;
        const char* name = m->nickname[0] || !g ? m->nickname : pd_species_name(g, m->species, buf, sizeof(buf));
        // A long name takes the icon's see-through right edge and most of the card's right margin before it's cut.
        if (pd_text_width(name) + (m->shiny ? pd_text_width("*") + 2 : 0) > right - tx) {
            tx = iconX + PD_ICON - 3;
            right = cx + w - 3;
        }
        int ty = y + (cardH - (tall ? 3 * LINE + 2 : 2 * LINE)) / 2;
        if (m->species == PD_SPECIES_EGG) {
            pd_text_fit(c, tx, y + (cardH - LINE) / 2, right - tx, name, PD_LABEL, PD_LABEL_SHADOW);
            continue;
        }
        char lv[12];
        snprintf(lv, sizeof(lv), "%d", m->level);
        int lvW = pd_text_width("Lv") + pd_number_width(lv);
        // The level beside the name only while the whole name fits next to it; else it goes before the HP bar, so
        // a name is cut only when it's wider than the card itself.
        int starW = m->shiny ? pd_text_width("*") + 2 : 0;
        bool lvRight = !tall && right - tx >= pd_text_width(name) + starW + lvW + 6; // tall: the level under it
        int nameW = pd_text_fit(c, tx, ty, right - tx - starW - (lvRight ? lvW + 4 : 0), name, PD_LABEL, PD_LABEL_SHADOW);
        if (m->shiny) pd_text(c, tx + nameW + 2, ty, "*", 0xF8C830, PD_LABEL_SHADOW); // shiny: a gold star
        int by = ty + LINE + 1, bx = tx;
        int lx = lvRight ? right - lvW : tx;
        int ly = lvRight ? ty : by;
        lx += pd_text(c, lx, ly, "Lv", PD_MUTED, PD_MUTED_SHADOW);
        lx += pd_number(c, lx, ly, lv, PD_LABEL, PD_LABEL_SHADOW);
        if (!lvRight) bx = lx + 4;
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
            int hpShown = eased_hp(i, m->personality ^ (uint32_t)m->species, m->hp);
            if (right - bx - 18 >= 12) {
                ui_hp_bar(c, bx, by, right - bx - 18, hpShown, m->maxHp);
                exp_line(c, bx + 16, by + 8, right - bx - 18, exp_filled(g, m));
            }
            pd_number_right(c, right, by + 5, hp, PD_MUTED, PD_MUTED_SHADOW);
            continue;
        }
        int barW = right - bx - 18;
        int hpShown = eased_hp(i, m->personality ^ (uint32_t)m->species, m->hp);
        if (barW >= 12) {
            ui_hp_bar(c, bx, by + 5, barW, hpShown, m->maxHp);
            exp_line(c, bx + 16, by + 13, barW, exp_filled(g, m));
        }
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
    pd_heading_fit(c, x, y, w, title, PD_VALUE, PD_VALUE_SHADOW);
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
