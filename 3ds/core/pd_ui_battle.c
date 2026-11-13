// BATTLE: INFO (BattleInfoScreen.kt) and SUGGESTIONS (SuggestionsScreen.kt).
//
// INFO: one white window split by rules - the foe, you, then your moves with
// their plain-words verdicts against the foe. SUGGESTIONS: the foe on a VS
// line, then the party's three best picks against it, each with its best
// move (the app's rankSuggestions). Under either, a bar: the button to the
// other pane and, in a trainer battle, the FOE TEAM - the trainer's party in
// order, Poké Balls until sent out or announced, fainted ones faded, the one
// the panes are about framed in red. Tapping one shows it (NEXT / FAINTED /
// RESERVE / UNSEEN), tapping it again follows the battle.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pd_dex.h"
#include "pd_icon.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define BAR_H 34
#define ROW 34      // a card's row: the icon and two lines of text
#define RULE 5      // a separator and the space around it
#define FRAME 3     // the white window's frame
#define LINE 16     // PD_FONT_HEIGHT

// BattleControlsScreen's button colours.
#define SUGGEST_GOLD 0xE8B830
#define SUGGEST_GOLD_DARK 0xA87C10
#define INFO_PURPLE 0x9068D0
#define INFO_PURPLE_DARK 0x5C3C98
#define STATS_PURPLE 0x9070D0
#define STATS_PURPLE_DARK 0x5C4490
#define VERDICT_GREY 0x8C8C94
#define OUT_GREEN 0x48A858

// What a card shows: a battler (types from the battle) or a party Pokémon.
struct mon_view {
    int species, level, hp, maxHp;
    int type1, type2;
    uint32_t status;
    const int* moves;
    const int* pp;
    bool shiny;
};

struct battle_data {
    const struct pd_game* game;
    const struct pd_battle_source* source;
};

static const char* species_name(const struct battle_data* d, int id, char* buf, size_t len) {
    return d->source ? d->source->species(d->source->ctx, id) : pd_species_name(d->game, id, buf, len);
}

static const char* move_name(const struct battle_data* d, int id, char* buf, size_t len) {
    return d->source ? d->source->move(d->source->ctx, id) : pd_move_name(d->game, id, buf, len);
}

static int move_type(const struct battle_data* d, int id) {
    return d->source ? d->source->moveType(d->source->ctx, id) : pd_move_type(d->game, id);
}

static int move_power(const struct battle_data* d, int id) {
    return d->source ? d->source->movePower(d->source->ctx, id) : pd_move_power(d->game, id);
}

static void species_types(const struct battle_data* d, int id, int* t1, int* t2) {
    if (d->source) d->source->speciesTypes(d->source->ctx, id, t1, t2);
    else pd_species_type_pair(d->game, id, t1, t2);
}

static bool draw_icon(const struct battle_data* d, struct pd_canvas* c, int species, bool shiny, int x, int y, bool faded) {
    return d->source ? d->source->icon(d->source->ctx, c, species, x, y, faded) :
        pd_draw_mon_icon_shiny(c, d->game, species, shiny, x, y, 1, faded);
}

static struct mon_view from_battler(const struct pd_battle_mon* b) {
    return (struct mon_view) { b->species, b->level, b->hp, b->maxHp, b->type1, b->type2, b->status, b->moves, b->pp, b->shiny };
}

static struct mon_view from_party(const struct battle_data* d, const struct pd_mon* m) {
    struct mon_view v = { m->species, m->level, m->hp, m->maxHp, PD_TYPE_NONE, PD_TYPE_NONE, m->status, m->moves, m->pp, m->shiny };
    species_types(d, m->species, &v.type1, &v.type2);
    return v;
}

static void types_text(const struct battle_data* d, int t1, int t2, char* buf, size_t len) {
    if (t1 == PD_TYPE_NONE) snprintf(buf, len, "-");
    else if (t2 != t1 && t2 != PD_TYPE_NONE) snprintf(buf, len, "%s/%s", pd_type_name(d->game, t1), pd_type_name(d->game, t2));
    else snprintf(buf, len, "%s", pd_type_name(d->game, t1));
}

// A small pill of white text (VerdictPill): returns its width.
static int pill(struct pd_canvas* c, int x, int y, const char* text, uint32_t fill) {
    int w = pd_text_width(text) + 6;
    pd_round_rect(c, x, y + 1, w, 14, 2, fill);
    pd_text(c, x + 3, y - 1, text, 0xFFFFFF, PD_NO_SHADOW);
    return w;
}

// The icon on the window's grey (a shiny one in its own colours, on pale gold in a gold frame: the app's monIconBox), or a
// ball when the ROM has no icon for it.
#define SHINY_FRAME 0xE0A820
#define SHINY_FILL 0xFFF4C8
static void icon(struct pd_canvas* c, const struct battle_data* d, int species, bool shiny, int x, int y, bool faded) {
    if (shiny) {
        pd_round_rect(c, x, y, PD_ICON + 2, PD_ICON + 2, 3, SHINY_FRAME);
        pd_round_rect(c, x + 1, y + 1, PD_ICON, PD_ICON, 2, SHINY_FILL);
    } else {
        pd_round_rect(c, x, y, PD_ICON + 2, PD_ICON + 2, 3, PD_LIST_FILL);
    }
    if (!draw_icon(d, c, species, shiny, x + 1, y + 1, faded)) ui_ball(c, x + 12, y + 12, true);
}

// A card across w: icon, then [side] name ... [state] Lv on the first line,
// types, HP bar and HP on the second.
static void card(struct pd_canvas* c, const struct battle_data* d, int x, int y, int w, const struct mon_view* m,
                 const char* side, const char* state) {
    char buf[40];
    icon(c, d, m->species, m->shiny, x, y, false);
    int tx = x + PD_ICON + 8, right = x + w;
    int ty = y + 1;
    int nx = tx;
    if (side) nx += pd_text(c, nx, ty, side, PD_VALUE, PD_VALUE_SHADOW) + 5;
    char lv[12];
    snprintf(lv, sizeof(lv), "Lv%d", m->level);
    int lvx = right - pd_text_width(lv);
    pd_text(c, lvx, ty, lv, PD_LABEL, PD_LABEL_SHADOW);
    int stateW = state && *state ? pd_text_width(state) + 6 + 4 : 0;
    const char* status = pd_status_label(m->status);
    int statusW = *status ? pd_text_width(status) + 6 + 4 : 0;
    int nameEnd = nx + pd_text_fit(c, nx, ty, lvx - 6 - stateW - statusW - nx,
                                   species_name(d, m->species, buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
    if (m->shiny) nameEnd += pd_text(c, nameEnd + 2, ty, "*", SHINY_FRAME, PD_LABEL_SHADOW) + 2;  // the app's ShinySparkle
    if (*status) {
        ui_status_badge(c, nameEnd + 4, ty + 1, m->status);
        nameEnd += statusW;
    }
    if (stateW) pill(c, nameEnd + 4, ty, state, VERDICT_GREY);

    ty += LINE + 1;
    types_text(d, m->type1, m->type2, buf, sizeof(buf));
    int typesW = pd_text_fit(c, tx, ty, 92, buf, PD_MUTED, PD_MUTED_SHADOW);
    char hp[16];
    snprintf(hp, sizeof(hp), "%d/%d", m->hp, m->maxHp);
    int hpx = right - pd_text_width(hp);
    pd_text(c, hpx, ty, hp, PD_LABEL, PD_LABEL_SHADOW);
    int barX = tx + (typesW > 92 ? typesW : 92) + 6;
    int barW = hpx - 6 - barX - 18;
    if (barW >= 24) ui_hp_bar(c, barX, ty + 5, barW, m->hp, m->maxHp);
}

// The plain-words verdict of BattleInfoScreen: SUPER 2x / RESISTED ½x / ...
static void verdict(int pct, const char** text, uint32_t* color, uint32_t* shadow) {
    if (pct == 0) { *text = "IMMUNE 0x"; *color = PD_MUTED; *shadow = PD_MUTED_SHADOW; }
    else if (pct >= 400) { *text = "SUPER 4x"; *color = PD_VALUE; *shadow = PD_VALUE_SHADOW; }
    else if (pct >= 200) { *text = "SUPER 2x"; *color = PD_VALUE; *shadow = PD_VALUE_SHADOW; }
    else if (pct <= 25) { *text = "RESISTED 1/4x"; *color = PD_MUTED; *shadow = PD_MUTED_SHADOW; }
    else if (pct < 100) { *text = "RESISTED 1/2x"; *color = PD_MUTED; *shadow = PD_MUTED_SHADOW; }
    else { *text = "NEUTRAL 1x"; *color = PD_LABEL; *shadow = PD_LABEL_SHADOW; }
}

// A move on the window's grey: its name and PP, then its verdict on foe
// (NULL = no foe: no verdict).
static void move_cell(struct pd_canvas* c, const struct battle_data* d, int x, int y, int w, int h, int move, int pp, const struct mon_view* foe) {
    char buf[40];
    pd_round_rect(c, x, y, w, h, 3, PD_LIST_FILL);
    if (!move) return;
    int ix = x + 5, iw = w - 10;
    int ty = y + (h - 2 * LINE - 1) / 2;
    char ppText[12];
    snprintf(ppText, sizeof(ppText), "PP %d", pp);
    int ppW = pd_text_width(ppText);
    pd_text_fit(c, ix, ty, iw - ppW - 6, move_name(d, move, buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
    pd_text_right(c, ix + iw, ty, ppText, pd_pp_color(pd_move_low_pp(d->source ? NULL : d->game, move, pp), PD_MUTED),
                  PD_MUTED_SHADOW);
    const char* text = "";
    uint32_t color = PD_LABEL, shadow = PD_LABEL_SHADOW;
    if (move_power(d, move) == 0) {
        text = "STATUS";
        color = PD_MUTED;
        shadow = PD_MUTED_SHADOW;
    } else if (foe) {
        verdict(pd_type_multiplier_pct(d->game, move_type(d, move), foe->type1, foe->type2), &text, &color, &shadow);
    }
    pd_text(c, ix, ty + LINE + 1, text, color, shadow);
}

static void rule(struct pd_canvas* c, int x, int y, int w) {
    pd_fill(c, x, y + RULE / 2, w, 1, PD_DIVIDER);
}

// A coloured square-cornered button (PlatinumButton / TextNavButton).
static void nav_button(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, int h, const char* label,
                       uint32_t fill, uint32_t dark, int id) {
    bool pressed = ui->pressedId == id;
    static const struct pd_layer FRAME_LAYERS[1] = { { 0, 2 } };
    struct pd_layer frame = FRAME_LAYERS[0];
    frame.color = dark;
    pd_layered_box(c, x, y, w, h, 3, &frame, 1, pressed ? dark : fill);
    pd_text(c, x + (w - pd_text_width(label)) / 2, pd_text_y(y, h), label, 0xFFFFFF, dark);
    ui_add_hit(ui, x, y, w, h, id);
}

// The FOE TEAM, from x to the right edge.
static void foe_strip(struct pd_ui* ui, struct pd_canvas* c, const struct battle_data* d, const struct pd_snapshot* s,
                      int x, int y, int right) {
    if (s->foeCount < 2) return;
    int cell = BAR_H, gap = 3;
    int need = s->foeCount * cell + (s->foeCount - 1) * gap;
    if (right - x < need) cell = (right - x - (s->foeCount - 1) * gap) / s->foeCount;
    int sx = right - (s->foeCount * cell + (s->foeCount - 1) * gap);
    for (int i = 0; i < s->foeCount; i++, sx += cell + gap) {
        bool shown = (ui->foeSeen >> i & 1) || i == ui->foeShown;
        bool current = i == ui->foeShown;
        struct pd_layer layers[2] = { { PD_VALUE, 2 }, { PD_VALUE_SHADOW, 1 } };
        struct pd_layer plain[2] = { { 0x63737B, 2 }, { 0xCED6D6, 1 } };
        pd_layered_box(c, sx, y, cell, cell, 3, current ? layers : plain, 2, shown ? PD_TITLE_FILL : PD_LIST_FILL);
        const struct pd_mon* m = &s->foes[i];
        if (!shown) {
            ui_ball(c, sx + (cell - 10) / 2, y + (cell - 10) / 2, true);
        } else if (!draw_icon(d, c, m->species, m->shiny, sx + (cell - PD_ICON) / 2, y + (cell - PD_ICON) / 2,
                                           m->hp == 0)) {
            char lv[8];
            snprintf(lv, sizeof(lv), "%d", m->level);
            pd_text(c, sx + (cell - pd_text_width(lv)) / 2, pd_text_y(y, cell), lv,
                    m->hp ? PD_LABEL : PD_MUTED, PD_NO_SHADOW);
        }
        if (!d->source || shown) ui_add_hit(ui, sx, y, cell, cell, HIT_FOE_SLOT + i);
    }
}

// The foe the panes are about: the tapped one, else the one about to come
// in, else the one out (CompanionScreen's shownFoe). A party Pokémon only
// when it isn't the one out - that one is the battler, with its live types.
static const struct pd_mon* shown_foe(struct pd_ui* ui, const struct pd_snapshot* s, const char** state) {
    int shown = ui->foeSelected >= 0 ? ui->foeSelected : s->foeNext >= 0 ? s->foeNext : s->foeActive;
    ui->foeShown = shown;
    *state = "";
    if (shown < 0 || shown >= s->foeCount || shown == s->foeActive) return NULL;
    const struct pd_mon* m = &s->foes[shown];
    if (shown == s->foeNext) *state = "NEXT";
    else if (m->hp == 0) *state = "FAINTED";
    else if (ui->foeSeen >> shown & 1) *state = "RESERVE";
    else *state = "UNSEEN";
    return m;
}

// --- SUGGESTIONS ---

struct pick {
    const struct pd_mon* mon;
    int move, pp, pct, score;
};

// Whether party Pokémon m is one of yours on the field (isOut: battlers
// carry no personality to match on).
static bool is_out(const struct pd_mon* m, const struct pd_snapshot* s) {
    for (int p = 0; p < 4; p += 2) {
        const struct pd_battle_mon* b = &s->battlers[p];
        if (b->species && b->species == m->species && b->level == m->level && b->maxHp == m->maxHp) return true;
    }
    return false;
}

static int by_score(const void* a, const void* b) {
    const struct pick *x = a, *y = b;
    if (x->score != y->score) return y->score - x->score;
    return (int) (x->mon - y->mon); // stable: party order on ties
}

// rankSuggestions: every living party Pokémon with a usable damaging move,
// by (its best move's power x multiplier on the foe) minus the worst the
// foe's own types do to it.
static int rank(const struct battle_data* d, const struct pd_snapshot* s, const struct mon_view* foe, struct pick* out) {
    int n = 0;
    for (int i = 0; i < s->partyCount; i++) {
        const struct pd_mon* m = &s->party[i];
        if (m->hp <= 0 || m->egg) continue;
        int best = -1, bestValue = -1;
        for (int k = 0; k < PD_NUM_MOVES; k++) {
            int power = m->moves[k] ? move_power(d, m->moves[k]) : 0;
            if (power <= 0 || m->pp[k] <= 0) continue;
            int value = power * pd_type_multiplier_pct(d->game, move_type(d, m->moves[k]), foe->type1, foe->type2);
            if (value > bestValue) {
                bestValue = value;
                best = k;
            }
        }
        if (best < 0) continue;
        int t1, t2;
        species_types(d, m->species, &t1, &t2);
        int risk = 100;
        if (foe->type1 != PD_TYPE_NONE) {
            risk = pd_type_multiplier_pct(d->game, foe->type1, t1, t2);
            if (foe->type2 != PD_TYPE_NONE && foe->type2 != foe->type1) {
                int r2 = pd_type_multiplier_pct(d->game, foe->type2, t1, t2);
                if (r2 > risk) risk = r2;
            }
        }
        int pct = pd_type_multiplier_pct(d->game, move_type(d, m->moves[best]), foe->type1, foe->type2);
        out[n++] = (struct pick) { m, m->moves[best], m->pp[best], pct, move_power(d, m->moves[best]) * pct / 100 - risk };
    }
    qsort(out, (size_t) n, sizeof(out[0]), by_score);
    return n;
}

static void suggestions(struct pd_canvas* c, const struct battle_data* d, const struct pd_snapshot* s,
                        const struct mon_view* foe, const char* foeState, int ix, int iy, int iw, int ih) {
    if (!foe) {
        pd_text(c, ix, iy, "NO FOE ON THE FIELD", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    card(c, d, ix, iy, iw, foe, "VS", foeState);
    int y = iy + ROW;
    rule(c, ix, y, iw);
    y += RULE;
    struct pick picks[PD_PARTY_SIZE];
    int n = rank(d, s, foe, picks);
    if (!n) {
        pd_text_wrap(c, ix, y + 2, iw, LINE, 2, "NO USABLE DAMAGING MOVES IN YOUR PARTY", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    static const char* const RANKS[3] = { "1ST", "2ND", "3RD" };
    int rowH = (iy + ih - y - 2 * RULE) / 3;
    int half = (iw - RULE) / 2;
    for (int i = 0; i < n && i < 3; i++) {
        if (i) {
            rule(c, ix, y, iw);
            y += RULE;
        }
        const struct pick* p = &picks[i];
        int ry = y + (rowH - ROW) / 2;
        // The Pokémon: rank and name, then its level, HP and OUT when it's in battle.
        char buf[40];
        icon(c, d, p->mon->species, p->mon->shiny, ix, ry, false);
        int tx = ix + PD_ICON + 8, right = ix + half;
        int nx = tx + pd_text(c, tx, ry + 1, RANKS[i], PD_VALUE, PD_VALUE_SHADOW) + 5;
        pd_text_fit(c, nx, ry + 1, right - nx, species_name(d, p->mon->species, buf, sizeof(buf)), PD_LABEL,
                    PD_LABEL_SHADOW);
        int ty = ry + LINE + 1;
        char lv[12];
        snprintf(lv, sizeof(lv), "Lv%d", p->mon->level);
        int bx = tx + pd_text(c, tx, ty, lv, PD_LABEL, PD_LABEL_SHADOW) + 5;
        int outW = is_out(p->mon, s) ? pd_text_width("OUT") + 6 : 0;
        if (outW) pill(c, right - outW, ty, "OUT", OUT_GREEN);
        int barW = right - (outW ? outW + 5 : 0) - bx - 18;
        if (barW > 48) barW = 48;
        if (barW >= 16) ui_hp_bar(c, bx, ty + 5, barW, p->mon->hp, p->mon->maxHp);
        // Its best move against the foe.
        pd_fill(c, ix + half + RULE / 2, y + 2, 1, rowH - 4, PD_DIVIDER);
        move_cell(c, d, ix + half + RULE, y + 1, iw - half - RULE, rowH - 2, p->move, p->pp, foe);
        y += rowH;
    }
}

// --- INFO ---

static void info(struct pd_canvas* c, const struct battle_data* d, const struct pd_snapshot* s,
                 const struct mon_view* foe, const char* foeState, const struct mon_view* you, int ix, int iy,
                 int iw, int ih) {
    int y = iy;
    if (foe) card(c, d, ix, y, iw, foe, s->isTrainer ? "FOE" : "WILD", foeState);
    else pd_text(c, ix, y + 1, "NO FOE ON THE FIELD", PD_MUTED, PD_MUTED_SHADOW);
    y += ROW;
    rule(c, ix, y, iw);
    y += RULE;
    card(c, d, ix, y, iw, you, "YOU", "");
    y += ROW;
    rule(c, ix, y, iw);
    y += RULE;
    int gap = 4;
    int bw = (iw - gap) / 2, bh = (iy + ih - y - gap) / 2;
    for (int i = 0; i < PD_NUM_MOVES; i++) {
        move_cell(c, d, ix + (i % 2) * (bw + gap), y + (i / 2) * (bh + gap), bw, bh, you->moves[i], you->pp[i], foe);
    }
}

// --- STATS (FOE IVS: BattleStatsScreen.kt) ---

// Yours: the party Pokemon out on the left. The foe: the FOE TEAM's shown one (else the one out), or the wild one.
static const struct pd_mon* stats_you(const struct pd_snapshot* s) {
    const struct pd_battle_mon* b = &s->battlers[PD_POS_PLAYER_LEFT];
    for (int i = 0; i < s->partyCount; i++)
        if (b->species && s->party[i].species == b->species && s->party[i].level == b->level && s->party[i].maxHp == b->maxHp)
            return &s->party[i];
    return NULL;
}
static const struct pd_mon* stats_foe(const struct pd_ui* ui, const struct pd_snapshot* s) {
    if (s->isTrainer) {
        int shown = ui->foeShown >= 0 && ui->foeShown < s->foeCount ? ui->foeShown : s->foeActive;
        return shown >= 0 && shown < s->foeCount ? &s->foes[shown] : NULL;
    }
    return s->wildFoeCount > 0 ? &s->wildFoes[0] : NULL;
}
// FOE IVS on and the foe's IVs read: INFO gets STATS.
static bool stats_offered(const struct pd_ui* ui, const struct pd_snapshot* s) {
    const struct pd_mon* foe = stats_foe(ui, s);
    return ui->settings && ui->settings->foeIvs && foe && foe->stats.hasIvs;
}

// [roomy]: the icon on a line of its own above; else the side, name and level share one line (a short pane).
static void stats_column(struct pd_canvas* c, const struct battle_data* d, const struct pd_mon* m, const char* side, int x,
                         int y, int w, bool roomy) {
    int sw = pd_text(c, x, y, side, PD_VALUE, PD_VALUE_SHADOW) + 6;
    if (roomy || !m) y += 15;
    if (!m) {
        pd_text_fit(c, x, y, w, "NO STATS FOR THIS ONE", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    char name[32], lv[12];
    snprintf(lv, sizeof(lv), "Lv%d", m->level);
    int lw = pd_text_width(lv), nx = x + sw;
    const char* shown = *m->nickname ? m->nickname : species_name(d, m->species, name, sizeof(name));
    if (roomy) {
        pd_round_rect(c, x, y, PD_ICON + 4, PD_ICON + 4, 2, PD_LIST_FILL);
        draw_icon(d, c, m->species, m->shiny, x + 2, y + 2, false);
        nx = x + PD_ICON + 8;
    }
    pd_text_fit(c, nx, roomy ? y + 2 : y, x + w - lw - 4 - nx, shown, PD_LABEL, PD_LABEL_SHADOW);
    pd_text_right(c, x + w, roomy ? y + 2 : y, lv, PD_VALUE, PD_VALUE_SHADOW);
    y += roomy ? PD_ICON + 8 : 15;
    if (m->stats.hasIvs || m->stats.hasEvs || m->stats.hasNature) ui_mon_stats(c, m, x, y, w);
    else pd_text_fit(c, x, y, w, "NO STATS FOR THIS ONE", PD_MUTED, PD_MUTED_SHADOW);
}

static void stats_pane(struct pd_ui* ui, struct pd_canvas* c, const struct battle_data* d, const struct pd_snapshot* s,
                       int ix, int iy, int iw, int ih) {
    int gap = 6, cw = (iw - gap) / 2;
    bool roomy = ih >= 15 + PD_ICON + 8 + 167; // the table: its head, six stats, the totals, nature and hidden power
    int cx0 = c->cx0, cy0 = c->cy0, cx1 = c->cx1, cy1 = c->cy1;
    pd_clip(c, ix, iy, iw, ih);
    stats_column(c, d, stats_you(s), "YOU", ix, iy, cw, roomy);
    pd_fill(c, ix + cw + gap / 2, iy, 1, ih, PD_MUTED);
    stats_column(c, d, stats_foe(ui, s), s->isTrainer ? "FOE" : "WILD", ix + cw + gap, iy, iw - cw - gap, roomy);
    c->cx0 = cx0, c->cy0 = cy0, c->cx1 = cx1, c->cy1 = cy1;
}

// --- BATTLE FOCUS (BattleFocusScreen.kt): INFO as one page about one Pokemon, the foe or yours ---

struct focus_mon {
    struct mon_view v;
    const char* pill;
    int ability;             // the battler's ability id; 0 = not read
    int item;                // the held item; -1 = not known
    bool hasStats, hasStages;
    int stats[5], stages[7]; // Attack, Defense, Speed, Sp. Atk, Sp. Def (stages: then accuracy, evasion)
};

static void focus_from_battler(struct focus_mon* f, const struct pd_battle_mon* b) {
    memset(f, 0, sizeof(*f));
    f->v = from_battler(b);
    f->item = -1;
    if (!b->hasBattleStats) return;
    f->ability = b->ability;
    f->item = b->heldItem;
    f->hasStats = f->hasStages = true;
    memcpy(f->stats, b->stats, sizeof(f->stats));
    memcpy(f->stages, b->stages, sizeof(f->stages));
}

static void focus_from_party(struct focus_mon* f, const struct battle_data* d, const struct pd_mon* m) {
    memset(f, 0, sizeof(*f));
    f->v = from_party(d, m);
    f->item = m->hasHeldItem ? m->heldItem : -1;
    if (m->stats.valid) {
        f->hasStats = true;
        for (int i = 0; i < 5; i++) f->stats[i] = m->stats.values[i + 1];
    }
}

// The ability: the battler's own, else the species' when it has only the one (the dex page's).
static void focus_ability(const struct battle_data* d, const struct focus_mon* f, char* out, size_t len) {
    out[0] = 0;
    if (!d->source && f->ability) pd_ability_name(d->game, f->ability, out, len);
    if (!*out && !d->source) {
        static const struct pd_game* cachedGame;
        static int cachedSpecies = -1;
        static char cached[16];
        if (cachedGame != d->game || cachedSpecies != f->v.species) {
            static struct pd_dex_entry e;
            cachedGame = d->game;
            cachedSpecies = f->v.species;
            cached[0] = 0;
            int national = pd_dex_available(d->game) ? pd_dex_national(d->game, f->v.species) : 0;
            if (national > 0 && pd_dex_entry(d->game, national, &e) && *e.ability1 && !*e.ability2)
                snprintf(cached, sizeof(cached), "%s", e.ability1);
        }
        snprintf(out, len, "%s", cached);
    }
    if (!*out) snprintf(out, len, "-");
}

static int type_badge_w(const struct pd_game* g, int type) {
    char text[12];
    const char* name = pd_type_name(g, type);
    int n = 0;
    for (; name && name[n] && n < 11; n++) text[n] = name[n] >= 'a' && name[n] <= 'z' ? (char) (name[n] - 32) : name[n];
    text[n] = 0;
    return n ? pd_text_width(text) + 6 : 0;
}

// A stat's name, its value and its stage as a pill (green up, red down).
static void focus_stat(struct pd_canvas* c, int x, int y, int w, const char* label, int value, bool hasValue, int stage) {
    pd_text(c, x, y, label, PD_LABEL, PD_LABEL_SHADOW);
    int right = x + w;
    if (stage) {
        char t[8];
        snprintf(t, sizeof(t), "%+d", stage);
        int pw = pd_text_width(t) + 6;
        pill(c, right - pw, y, t, stage > 0 ? OUT_GREEN : 0xD84838);
        right -= pw + 4;
    }
    if (hasValue) {
        char v[8];
        snprintf(v, sizeof(v), "%d", value);
        pd_text_right(c, right, y, v, PD_VALUE, PD_VALUE_SHADOW);
    }
}

// Whether type [t] lands in matchup group [kind]: 0 WEAK TO, 1 RESISTS, 2 IMMUNE TO; [pass] 0 the strong ones first.
static bool in_group(int pct, int kind, int pass) {
    if (kind == 0) return pass == 0 ? pct >= 400 : pct > 100 && pct < 400;
    if (kind == 1) return pass == 0 ? pct > 0 && pct <= 25 : pct > 25 && pct < 100;
    return pass == 0 && pct == 0;
}

static int group_size(const struct pd_game* g, const struct mon_view* m, int kind, int* list) {
    int n = 0, count = pd_type_count(g);
    if (count > 32) count = 32;
    for (int pass = 0; pass < 2; pass++)
        for (int t = 0; t < count; t++) {
            const char* name = pd_type_name(g, t);
            if (name && *name && in_group(pd_type_multiplier_pct(g, t, m->type1, m->type2), kind, pass)) list[n++] = t;
        }
    return n;
}

// One matchup group: its label, then the types' badges over at most [lines] lines. Returns the height used.
static int focus_group(struct pd_canvas* c, const struct pd_game* g, int x, int y, int w, const char* label,
                       const struct mon_view* m, int kind, int lines) {
    int list[64];
    int n = group_size(g, m, kind, list);
    if (!n) return 0;
    int lx = x + pd_text(c, x, y, label, PD_LABEL, PD_LABEL_SHADOW) + 5;
    int cx = lx, line = 0, right = x + w;
    for (int i = 0; i < n; i++) {
        int bw = type_badge_w(g, list[i]);
        if (cx + bw > right) {
            if (line + 1 >= lines) break;
            line++;
            cx = lx;
        }
        cx += ui_type_badge(c, cx, y + line * (LINE + 2) + 3, list[i]) + 3;
    }
    return (line + 1) * (LINE + 2);
}

static void focus_mon_page(struct pd_canvas* c, const struct battle_data* d, const struct focus_mon* f,
                           const struct mon_view* other, bool hints, int ix, int iy, int iw, int ih) {
    char buf[40], ability[32];
    const struct mon_view* m = &f->v;
    // The header: icon, name and types; ABILITY and HELD ITEM beside them. No HP or level.
    icon(c, d, m->species, m->shiny, ix, iy, false);
    int tx = ix + PD_ICON + 8, right = ix + iw;
    int split = tx + (right - tx) * 44 / 100;
    int ty = iy + 1;
    const char* status = pd_status_label(m->status);
    int statusW = *status ? pd_text_width(status) + 10 : 0;
    int nameEnd = tx + pd_text_fit(c, tx, ty, split - 6 - statusW - tx, species_name(d, m->species, buf, sizeof(buf)),
                                   PD_LABEL, PD_LABEL_SHADOW);
    if (m->shiny) nameEnd += pd_text(c, nameEnd + 2, ty, "*", SHINY_FRAME, PD_LABEL_SHADOW) + 2;
    if (*status) ui_status_badge(c, nameEnd + 4, ty + 1, m->status);
    bool roomy = right - split >= pd_text_width("HELD ITEM") + 60;
    focus_ability(d, f, ability, sizeof(ability));
    int vx = split + pd_text(c, split, ty, "ABILITY", PD_MUTED, PD_MUTED_SHADOW) + 5;
    pd_text_fit(c, vx, ty, right - vx, ability, PD_VALUE, PD_VALUE_SHADOW);
    ty += LINE + 3;
    int bx = tx;
    if (m->type1 != PD_TYPE_NONE) {
        bx += ui_type_badge(c, bx, ty + 2, m->type1) + 3;
        if (m->type2 != m->type1 && m->type2 != PD_TYPE_NONE) bx += ui_type_badge(c, bx, ty + 2, m->type2) + 3;
    }
    if (f->pill && *f->pill && bx + pd_text_width(f->pill) + 6 <= split - 4) pill(c, bx + 1, ty, f->pill, VERDICT_GREY);
    const char* item = f->item < 0 || d->source ? "-" : f->item == 0 ? "NONE" : pd_item_name(d->game, f->item);
    vx = split + pd_text(c, split, ty, roomy ? "HELD ITEM" : "ITEM", PD_MUTED, PD_MUTED_SHADOW) + 5;
    pd_text_fit(c, vx, ty, right - vx, item && *item ? item : "-", PD_VALUE, PD_VALUE_SHADOW);
    int y = iy + (PD_ICON + 4 > 2 * LINE + 4 ? PD_ICON + 4 : 2 * LINE + 4);
    rule(c, ix, y, iw);
    y += RULE;

    // The stats, two columns of three, with their stage pills.
    if (f->hasStats || f->hasStages) {
        static const char* const NAMES[5] = { "ATTACK", "DEFENSE", "SPEED", "SP. ATK", "SP. DEF" };
        int cw = (iw - 12) / 2;
        for (int i = 0; i < 5; i++)
            focus_stat(c, ix + (i / 3) * (cw + 12), y + (i % 3) * LINE, cw, NAMES[i], f->stats[i], f->hasStats,
                       f->hasStages ? f->stages[i] : 0);
        // Accuracy and evasion, where a move has changed them.
        int ax = ix + cw + 12, ay = y + 2 * LINE;
        if (f->hasStages)
            for (int i = 5; i < 7; i++) {
                if (!f->stages[i]) continue;
                char t[8];
                snprintf(t, sizeof(t), "%+d", f->stages[i]);
                ax += pd_text(c, ax, ay, i == 5 ? "ACC" : "EVA", PD_LABEL, PD_LABEL_SHADOW) + 3;
                ax += pill(c, ax, ay, t, f->stages[i] > 0 ? OUT_GREEN : 0xD84838) + 6;
            }
        y += 3 * LINE;
    } else {
        pd_text(c, ix, y, "NO STATS FOR THIS ONE", PD_MUTED, PD_MUTED_SHADOW);
        y += LINE;
    }
    rule(c, ix, y, iw);
    y += RULE;

    // The matchups get what the moves leave them: a line a group, WEAK TO two when there's room.
    static const char* const GROUPS[3] = { "WEAK TO", "RESISTS", "IMMUNE TO" };
    int movesMin = 2 * (2 * LINE + 6) + 4;
    int lines = (iy + ih - y - movesMin - RULE) / (LINE + 2);
    int nGroups = 0, list[64];
    if (hints && m->type1 != PD_TYPE_NONE)
        for (int k = 0; k < 3; k++)
            if (group_size(d->game, m, k, list)) nGroups++;
    if (nGroups > lines) nGroups = lines > 0 ? lines : 0;
    int extra = lines > nGroups ? 1 : 0;
    int groupH = nGroups ? (nGroups + extra) * (LINE + 2) + RULE : 0;
    int movesH = iy + ih - y - groupH;
    if (m->moves[0] || m->moves[1] || m->moves[2] || m->moves[3]) {
        int gap = 4, bw = (iw - gap) / 2, bh = (movesH - gap) / 2;
        for (int i = 0; i < PD_NUM_MOVES; i++)
            move_cell(c, d, ix + (i % 2) * (bw + gap), y + (i / 2) * (bh + gap), bw, bh, m->moves[i], m->pp[i],
                      hints ? other : NULL);
    } else {
        pd_text(c, ix, y, "NO MOVE DATA", PD_MUTED, PD_MUTED_SHADOW);
    }
    if (!nGroups) return;
    y += movesH;
    rule(c, ix, y - RULE / 2 - 1, iw);
    y += RULE - 2;
    int drawn = 0;
    for (int k = 0; k < 3 && drawn < nGroups; k++) {
        int used = focus_group(c, d->game, ix, y, iw, GROUPS[k], m, k, 1 + (drawn == 0 ? extra : 0));
        if (used) drawn++;
        y += used;
    }
}

// The column down the page's right: the team (the trainer's, or yours), then the turn button and the way back.
static void focus_column(struct pd_ui* ui, struct pd_canvas* c, const struct battle_data* d, const struct pd_snapshot* s,
                         int x, int y, int w, int h, bool yours, int viewed, bool canTurn, bool controls) {
    int bottom = y + h;
    if (controls) {
        bottom -= BAR_H;
        nav_button(ui, c, x, bottom, w, BAR_H, "<", 0x4890D8, 0x2860A0, HIT_BATTLE_CONTROLS);
        bottom -= 4;
    }
    if (canTurn) {
        bottom -= BAR_H;
        nav_button(ui, c, x, bottom, w, BAR_H, yours ? "FOE" : "YOU", INFO_PURPLE, INFO_PURPLE_DARK, HIT_FOCUS_TURN);
        bottom -= 4;
    }
    int n = yours ? s->partyCount : s->foeCount, gap = 3;
    if (n < 2) return;
    int cell = (bottom - y - (n - 1) * gap) / n;
    if (cell > w) cell = w;
    if (cell < 12) return;
    int cx = x + (w - cell) / 2;
    for (int i = 0; i < n; i++, y += cell + gap) {
        const struct pd_mon* m = yours ? &s->party[i] : &s->foes[i];
        bool shown = yours || (ui->foeSeen >> i & 1) || i == ui->foeShown;
        bool current = yours ? i == viewed : i == ui->foeShown;
        struct pd_layer layers[2] = { { PD_VALUE, 2 }, { PD_VALUE_SHADOW, 1 } };
        struct pd_layer plain[2] = { { 0x63737B, 2 }, { 0xCED6D6, 1 } };
        pd_layered_box(c, cx, y, cell, cell, 3, current ? layers : plain, 2, shown ? PD_TITLE_FILL : PD_LIST_FILL);
        if (!shown) {
            ui_ball(c, cx + (cell - 10) / 2, y + (cell - 10) / 2, true);
        } else if (cell < PD_ICON || !draw_icon(d, c, m->species, m->shiny, cx + (cell - PD_ICON) / 2,
                                                y + (cell - PD_ICON) / 2, m->hp == 0)) {
            char lv[8];
            snprintf(lv, sizeof(lv), "%d", m->level);
            pd_text(c, cx + (cell - pd_text_width(lv)) / 2, pd_text_y(y, cell), lv, m->hp ? PD_LABEL : PD_MUTED,
                    PD_NO_SHADOW);
        }
        if (yours) ui_add_hit(ui, cx, y, cell, cell, HIT_FOCUS_OWN + i);
        else if (!d->source || shown) ui_add_hit(ui, cx, y, cell, cell, HIT_FOE_SLOT + i);
    }
}

static void focus_page(struct pd_ui* ui, struct pd_canvas* c, const struct battle_data* d, const struct pd_snapshot* s,
                       bool controls) {
    const char* foeState;
    const struct pd_mon* foeParty = shown_foe(ui, s, &foeState);
    static struct focus_mon foe, you;
    bool haveFoe = true;
    if (foeParty) focus_from_party(&foe, d, foeParty);
    else if (s->battlers[PD_POS_OPPONENT_LEFT].species) focus_from_battler(&foe, &s->battlers[PD_POS_OPPONENT_LEFT]);
    else haveFoe = false;
    foe.pill = foeState;
    // A new foe in: back to its page.
    int key = haveFoe ? foe.v.species * 128 + foe.v.level + 1 : 0;
    if (key != ui->focusFoeKey) {
        ui->focusFoeKey = key;
        ui->focusYours = false;
    }
    // Yours: the one out (the battler, with its stages), or the one tapped in the column.
    const struct pd_mon* out = stats_you(s);
    int outSlot = out ? (int) (out - s->party) : -1;
    int viewed = ui->focusSlot >= 0 && ui->focusSlot < s->partyCount ? ui->focusSlot : outSlot;
    if (viewed < 0 || viewed == outSlot) {
        focus_from_battler(&you, &s->battlers[PD_POS_PLAYER_LEFT]);
        if (out && you.item < 0 && out->hasHeldItem) you.item = out->heldItem;
        you.pill = "YOU";
    } else {
        focus_from_party(&you, d, &s->party[viewed]);
        you.pill = s->party[viewed].hp == 0 ? "FAINTED" : "RESERVE";
    }
    struct mon_view youOut = from_battler(&s->battlers[PD_POS_PLAYER_LEFT]);
    bool yours = ui->focusYours || !haveFoe;
    bool hints = !ui->settings || !ui->settings->hideHints;

    int x = MARGIN, w = c->w - 2 * MARGIN, top = CONTENT_Y, h = c->h - MARGIN - CONTENT_Y;
    int teamN = yours ? s->partyCount : s->foeCount;
    bool column = teamN > 1 || controls || haveFoe;
    int colW = BAR_H, pw = column ? w - colW - 6 : w;
    int f = pd_title_box(c, x, top, pw, h, PD_TITLE_FILL);
    int ix = x + f + 4, iy = top + f + 2, iw = pw - 2 * (f + 4), ih = h - 2 * (f + 2);
    int cx0 = c->cx0, cy0 = c->cy0, cx1 = c->cx1, cy1 = c->cy1;
    pd_clip(c, ix, iy, iw, ih);
    if (yours) focus_mon_page(c, d, &you, haveFoe ? &foe.v : NULL, hints, ix, iy, iw, ih);
    else focus_mon_page(c, d, &foe, &youOut, hints, ix, iy, iw, ih);
    c->cx0 = cx0, c->cy0 = cy0, c->cx1 = cx1, c->cy1 = cy1;
    if (column) focus_column(ui, c, d, s, x + pw + 6, top, colW, h, yours, viewed, haveFoe, controls);
}

void ui_battle_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    const struct battle_data data = {g, ui->battleSource};
    const struct pd_battle_mon* youB = &s->battlers[PD_POS_PLAYER_LEFT];
    if (!s->inBattle || !youB->species) {
        ui_notice(c, "NO BATTLE", "A battle opens this tab by itself,", "and its end takes you back.");
        return;
    }
    bool controls = ui_battle_controls_usable(ui, s);
    if (!ui_battle_info_on(ui)) {
        // BATTLE INFO off: the buttons alone, or a word to use the game's own menu.
        ui->battlePane = 0;
        ui->battleInfoOpen = false;
        if (controls) ui_battle_controls(ui, c, g, s);
        else ui_notice(c, "BATTLE INFO IS OFF", "Use the game's own menu.", "SETTINGS turns it back on.");
        return;
    }
    if (controls && !ui->battleInfoOpen && !ui->battlePane) {
        ui_battle_controls(ui, c, g, s);
        return;
    }
    // BATTLE FOCUS: INFO is one page, with no SUGGESTIONS or STATS.
    if (ui->settings && ui->settings->battleFocus) {
        ui->battlePane = 0;
        focus_page(ui, c, &data, s, controls);
        return;
    }
    // The foe: the FOE TEAM's shown one, else the battler out.
    const char* foeState;
    const struct pd_mon* party = shown_foe(ui, s, &foeState);
    struct mon_view foeView, you = from_battler(youB);
    const struct mon_view* foe = NULL;
    if (party) {
        foeView = from_party(&data, party);
        foe = &foeView;
    } else if (s->battlers[PD_POS_OPPONENT_LEFT].species) {
        foeView = from_battler(&s->battlers[PD_POS_OPPONENT_LEFT]);
        foe = &foeView;
    }

    int x = MARGIN, w = c->w - 2 * MARGIN;
    int barY = c->h - MARGIN - BAR_H;
    int h = barY - 4 - CONTENT_Y;
    int f = pd_title_box(c, x, CONTENT_Y, w, h, PD_TITLE_FILL);
    int ix = x + f + 4, iy = CONTENT_Y + f + 2, iw = w - 2 * (f + 4), ih = h - 2 * (f + 2);
    bool statsOk = stats_offered(ui, s);
    if (ui->battlePane == 2 && !statsOk) ui->battlePane = 0;
    if (ui->battlePane == 2) stats_pane(ui, c, &data, s, ix, iy, iw, ih);
    else if (ui->battlePane) suggestions(c, &data, s, foe, foeState, ix, iy, iw, ih);
    else info(c, &data, s, foe, foeState, &you, ix, iy, iw, ih);

    // The bar: the other pane's button, then the FOE TEAM.
    // One width for both, so the same spot flips back.
    const char* label = ui->battlePane ? "INFO" : "SUGGESTIONS";
    int bw = pd_text_width("SUGGESTIONS") + 20;
    if (ui->battlePane) nav_button(ui, c, x, barY, bw, BAR_H, label, INFO_PURPLE, INFO_PURPLE_DARK, HIT_BATTLE_INFO);
    else nav_button(ui, c, x, barY, bw, BAR_H, label, SUGGEST_GOLD, SUGGEST_GOLD_DARK, HIT_BATTLE_SUGGEST);
    int stripX = x + bw + 8;
    if (statsOk && !ui->battlePane) {
        int sw = pd_text_width("STATS") + 20;
        nav_button(ui, c, stripX - 4, barY, sw, BAR_H, "STATS", STATS_PURPLE, STATS_PURPLE_DARK, HIT_BATTLE_STATS);
        stripX += sw + 4;
    }
    int right = x + w;
    // Back to the controls, while there are any to go back to.
    if (controls) {
        right -= BAR_H + 2;
        nav_button(ui, c, right + 2, barY, BAR_H, BAR_H, "<", 0x4890D8, 0x2860A0, HIT_BATTLE_CONTROLS);
    }
    foe_strip(ui, c, &data, s, stripX, barY, right);
}

// The pick of ui_battle_tab's SUGGESTIONS, for the POKéMON pane: recommendedSwitch.
int ui_recommended_switch(const struct pd_snapshot* s) {
    const struct pd_battle_mon* b = &s->battlers[PD_POS_OPPONENT_LEFT];
    if (!b->species) return -1;
    struct mon_view foe = from_battler(b);
    struct pick picks[PD_PARTY_SIZE];
    const struct battle_data data = {ui_game, NULL};
    int n = rank(&data, s, &foe, picks);
    for (int i = 0; i < n; i++)
        if (!is_out(picks[i].mon, s)) return (int) (picks[i].mon - s->party);
    return -1;
}

int ui_out_slots(const struct pd_snapshot* s) {
    int bits = 0;
    for (int i = 0; i < s->partyCount; i++)
        if (is_out(&s->party[i], s)) bits |= 1 << i;
    return bits;
}

bool ui_battle_info_on(const struct pd_ui* ui) {
    return !ui->settings || ui->settings->battleInfo;
}

bool ui_battle_act(struct pd_ui* ui, int id) {
    if ((id == HIT_BATTLE_SUGGEST || id == HIT_BATTLE_INFO || id == HIT_BATTLE_STATS) && !ui_battle_info_on(ui)) return true;
    if (id == HIT_BATTLE_SUGGEST || id == HIT_BATTLE_INFO || id == HIT_BATTLE_STATS) {
        ui->battlePane = id == HIT_BATTLE_SUGGEST ? 1 : id == HIT_BATTLE_STATS ? 2 : 0;
        ui->battleInfoOpen = true;
        return true;
    }
    if (id == HIT_FOCUS_TURN) {
        ui->focusYours = !ui->focusYours;
        return true;
    }
    if (id >= HIT_FOCUS_OWN && id < HIT_FOCUS_OWN + PD_PARTY_SIZE) {
        // Tapping the one already shown goes back to the one out.
        int i = id - HIT_FOCUS_OWN;
        ui->focusSlot = i == ui->focusSlot ? -1 : i;
        return true;
    }
    if (id == HIT_BATTLE_CONTROLS) {
        ui->battleInfoOpen = false;
        ui->battlePane = 0;
        return true;
    }
    if (id >= HIT_FOE_SLOT && id < HIT_FOE_SLOT + PD_PARTY_SIZE) {
        int i = id - HIT_FOE_SLOT;
        // Tapping the one shown goes back to following the battle.
        ui->foeSelected = i == ui->foeShown && ui->foeSelected >= 0 ? -1 : i;
        return true;
    }
    return false;
}

// --- COMPANION VIEW's battle panel (the Linux / Windows host's right wing) ---

// A battler stacked for a narrow column: icon and name, then the level and HP bar, then its types.
static int narrow_card(struct pd_canvas* c, const struct battle_data* d, int x, int y, int w, const struct mon_view* m,
                       const char* side) {
    char buf[40];
    icon(c, d, m->species, m->shiny, x, y, false);
    int tx = x + PD_ICON + 6, right = x + w;
    pd_text(c, tx, y + 1, side, PD_VALUE, PD_VALUE_SHADOW);
    pd_text_fit(c, tx, y + LINE + 1, right - tx, species_name(d, m->species, buf, sizeof(buf)), PD_LABEL,
                PD_LABEL_SHADOW);
    int ty = y + PD_ICON + 4;
    char lv[12];
    snprintf(lv, sizeof(lv), "Lv%d", m->level);
    int bx = x + pd_text(c, x, ty, lv, PD_LABEL, PD_LABEL_SHADOW) + 5;
    const char* status = pd_status_label(m->status);
    if (*status) {
        ui_status_badge(c, bx, ty + 1, m->status);
        bx += pd_text_width(status) + 10;
    }
    int barW = right - bx - 18;
    if (barW >= 16) ui_hp_bar(c, bx, ty + 5, barW, m->hp, m->maxHp);
    ty += LINE + 2;
    if (m->type1 != PD_TYPE_NONE) {
        int tx2 = x + ui_type_badge(c, x, ty + 2, m->type1) + 3;
        if (m->type2 != m->type1 && m->type2 != PD_TYPE_NONE) ui_type_badge(c, tx2, ty + 2, m->type2);
    }
    return ty + LINE - y;
}

// What hits the foe hard: a WEAK label and the types doing 4x, then 2x, as many chips as fit on one line.
static int weak_row(struct pd_canvas* c, const struct pd_game* g, int x, int y, int w, const struct mon_view* foe) {
    if (foe->type1 == PD_TYPE_NONE) return 0;
    int n = pd_type_count(g), right = x + w;
    int cx = x + pd_text(c, x, y + 1, "WEAK", PD_VALUE, PD_VALUE_SHADOW) + 5;
    bool any = false;
    for (int want = 400; want >= 200; want /= 2)
        for (int t = 0; t < n; t++) {
            int pct = pd_type_multiplier_pct(g, t, foe->type1, foe->type2);
            if (want == 400 ? pct < 400 : (pct < 200 || pct >= 400)) continue;
            const char* name = pd_type_name(g, t);
            if (!name || !*name) continue;
            if (cx + pd_text_width(name) + 6 > right) continue;
            cx += ui_type_badge(c, cx, y + 3, t) + 3;
            any = true;
        }
    if (!any) pd_text(c, cx, y + 1, "NOTHING", PD_MUTED, PD_MUTED_SHADOW);
    return LINE + 4;
}

// SUGGESTIONS' top pick when it isn't the one out already.
static const struct pd_mon* better_switch(const struct battle_data* d, const struct pd_snapshot* s,
                                          const struct mon_view* foe) {
    struct pick picks[PD_PARTY_SIZE];
    int n = rank(d, s, foe, picks);
    if (!n || is_out(picks[0].mon, s)) return NULL;
    return picks[0].mon;
}

int pd_ui_switch_pick(const struct pd_game* g, const struct pd_snapshot* s) {
    const struct pd_battle_mon* b = &s->battlers[PD_POS_OPPONENT_LEFT];
    if (!s->inBattle || !b->species) return -1;
    ui_game = g;
    const struct battle_data data = {g, NULL};
    struct mon_view foe = from_battler(b);
    const struct pd_mon* m = better_switch(&data, s, &foe);
    return m ? (int) (m - s->party) : -1;
}

// --- LAYOUT's BATTLE RIGHT WING ---

const char* const pd_battle_section_names[PD_BATTLE_SECTIONS] = {"FOE AND YOU", "MOVES", "SPEED", "FOE IVS",
                                                                 "SWITCH TO", "FOE TEAM"};
const int pd_battle_default[] = {PD_BS_CARDS, PD_BS_MOVES, PD_BS_SPEED, PD_BS_FOE_STATS, PD_BS_SWITCH, PD_BS_FOE_TEAM};
const int pd_battle_default_count = sizeof pd_battle_default / sizeof pd_battle_default[0];
const int* pd_ui_battle_list;
int pd_ui_battle_count;
bool pd_ui_foe_ivs;
int pd_ui_switch_slot = -1;

#define NARROW_H (PD_ICON + 4 + LINE + 2 + LINE) // narrow_card's height
#define EXTRA_H (LINE + 2)                       // a line under the foe's card
#define MOVE_GAP 3

struct wing {
    const struct battle_data* d;
    const struct pd_snapshot* s;
    struct mon_view you, foeView;
    const struct mon_view* foe;
    const struct pd_battle_mon *foeB, *youB;
    struct focus_mon foeF;
    const struct pd_mon* better;    // SWITCH TO
    const struct pd_mon* foeStats;  // FOE IVS' Pokemon, NULL = none to show
    int mark;                       // the foe's dex mark: 1 CAUGHT, 0 NOT CAUGHT, -1 none
    bool stages;                    // the foe has a stat stage that isn't 0
    bool detail;                    // the moves' power and accuracy line
};

static const char* const STAGE_NAMES[7] = {"ATK", "DEF", "SPE", "SPA", "SPD", "ACC", "EVA"};

// The foe's dex mark (Gen 3 carts: the save's caught flags by national number).
static int dex_mark(const struct battle_data* d, const struct pd_snapshot* s, int species) {
    if (d->source || !d->game || d->game->kind == PD_GAME_GB || !pd_dex_available(d->game)) return -1;
    int national = pd_dex_national(d->game, species);
    if (national <= 0 || (national - 1) / 8 >= PD_DEX_BYTES) return -1;
    bool any = false;
    for (int i = 0; i < PD_DEX_BYTES && !any; i++) any = s->dexSeen[i] != 0;
    if (!any) return -1;
    return s->dexCaught[(national - 1) / 8] >> ((national - 1) % 8) & 1;
}

// FOE IVS' Pokemon: the trainer's one out, or the wild one, when FOE IVS is on and its IVs are read.
static const struct pd_mon* foe_stats_mon(const struct pd_snapshot* s) {
    if (!pd_ui_foe_ivs) return NULL;
    const struct pd_mon* m = NULL;
    if (s->isTrainer) m = s->foeActive >= 0 && s->foeActive < s->foeCount ? &s->foes[s->foeActive] : NULL;
    else if (s->wildFoeCount > 0) m = &s->wildFoes[0];
    return m && m->stats.hasIvs ? m : NULL;
}

// A battler's speed with its stage and paralysis (Gen 3's: a quarter), -1 unknown.
static int battle_speed(const struct pd_battle_mon* b) {
    if (!b->species || !b->hasBattleStats || b->stats[2] <= 0) return -1;
    int st = b->stages[2], v = b->stats[2];
    v = st >= 0 ? v * (2 + st) / 2 : v * 2 / (2 - st);
    if (b->status & 0x40) v /= 4;
    return v;
}

static int moves_h(bool detail) {
    return PD_NUM_MOVES * ((detail ? 3 : 2) * LINE + 6) + (PD_NUM_MOVES - 1) * MOVE_GAP;
}
// The least the moves take: a line each (the name, then the verdict or the PP).
#define MOVE_ROW_MIN (LINE + 4)
static int moves_min_h(void) {
    return PD_NUM_MOVES * MOVE_ROW_MIN + (PD_NUM_MOVES - 1) * MOVE_GAP;
}

// A section's height across the wing; 0 = nothing to show.
static int section_h(const struct wing* w, int sec) {
    switch (sec) {
    case PD_BS_CARDS: {
        int h = 2 * NARROW_H + RULE;
        if (w->foe) {
            h += (w->foeF.hasStats ? EXTRA_H : 0) + (w->stages ? EXTRA_H : 0);
            if (w->foe->type1 != PD_TYPE_NONE) h += LINE + 4;
        } else {
            h += 2 * LINE - NARROW_H;
        }
        return h;
    }
    case PD_BS_MOVES: return moves_h(w->detail);
    case PD_BS_SPEED: return w->foe && battle_speed(w->youB) >= 0 && battle_speed(w->foeB) >= 0 ? LINE + 2 : 0;
    case PD_BS_FOE_STATS: return w->foeStats ? LINE + 10 + LINE + 2 + LINE + 2 : 0;
    case PD_BS_SWITCH: return w->better ? LINE + 2 : 0;
    case PD_BS_FOE_TEAM: return w->s->isTrainer && w->s->foeCount >= 2 && !w->d->source ? BAR_H - 6 : 0;
    }
    return 0;
}

// The foe's lines under its card: ability and held item, then its stat stages that aren't 0.
static int foe_extras(struct pd_canvas* c, const struct wing* w, int x, int y, int iw) {
    int y0 = y;
    if (w->foeF.hasStats) {
        char ability[32];
        focus_ability(w->d, &w->foeF, ability, sizeof(ability));
        const char* item = w->foeF.item < 0 || w->d->source ? "" : w->foeF.item == 0 ? "NO ITEM"
                                                                                      : pd_item_name(w->d->game, w->foeF.item);
        int half = iw / 2;
        pd_text_fit(c, x, y, half - 4, ability, PD_LABEL, PD_LABEL_SHADOW);
        if (item && *item) pd_text_fit(c, x + half, y, iw - half, item, PD_MUTED, PD_MUTED_SHADOW);
        y += EXTRA_H;
    }
    if (w->stages) {
        int cx = x;
        for (int i = 0; i < 7; i++) {
            if (!w->foeF.stages[i]) continue;
            char t[12];
            snprintf(t, sizeof(t), "%s%+d", STAGE_NAMES[i], w->foeF.stages[i]);
            if (cx + pd_text_width(t) + 6 > x + iw) break;
            cx += pill(c, cx, y, t, w->foeF.stages[i] > 0 ? OUT_GREEN : 0xD84838) + 3;
        }
        y += EXTRA_H;
    }
    return y - y0;
}

static int draw_cards(struct pd_canvas* c, const struct wing* w, int ix, int y, int iw) {
    int y0 = y;
    if (w->foe) {
        narrow_card(c, w->d, ix, y, iw, w->foe, w->s->isTrainer ? "FOE" : "WILD");
        // Beside FOE / WILD: a shiny star and whether you have caught one.
        int mx = ix + PD_ICON + 6 + pd_text_width(w->s->isTrainer ? "FOE" : "WILD") + 5;
        if (w->foe->shiny) mx += pd_text(c, mx, y + 1, "*", SHINY_FRAME, PD_LABEL_SHADOW) + 4;
        if (w->mark >= 0 && mx + pd_text_width("NOT CAUGHT") + 6 <= ix + iw)
            pill(c, mx, y + 1, w->mark ? "CAUGHT" : "NOT CAUGHT", w->mark ? OUT_GREEN : VERDICT_GREY);
        y += NARROW_H;
        y += foe_extras(c, w, ix, y, iw);
        y += weak_row(c, w->d->game, ix, y, iw, w->foe);
    } else {
        pd_text_wrap(c, ix, y, iw, LINE, 2, "NO FOE ON THE FIELD", PD_MUTED, PD_MUTED_SHADOW);
        y += 2 * LINE;
    }
    rule(c, ix, y, iw);
    y += RULE;
    narrow_card(c, w->d, ix, y, iw, &w->you, "YOU");
    if (w->you.shiny) pd_text(c, ix + PD_ICON + 6 + pd_text_width("YOU") + 5, y + 1, "*", SHINY_FRAME, PD_LABEL_SHADOW);
    return y + NARROW_H - y0;
}

// A move for the wing: name and PP; its kind and verdict; then (detail) its power and accuracy.
static void wing_move(struct pd_canvas* c, const struct battle_data* d, int x, int y, int w, int h, int move, int pp,
                      const struct mon_view* foe, bool detail) {
    char buf[40];
    pd_round_rect(c, x, y, w, h, 3, PD_LIST_FILL);
    if (!move) return;
    int ix = x + 5, iw = w - 10;
    if (h < 2 * LINE) { // a short wing: one line, the name and what it does to the foe (its PP for a status move)
        int ty1 = y + (h - LINE) / 2;
        const char* text = "";
        uint32_t color = PD_MUTED, shadow = PD_MUTED_SHADOW;
        char ppText1[12];
        if (move_power(d, move) && foe) {
            verdict(pd_type_multiplier_pct(d->game, move_type(d, move), foe->type1, foe->type2), &text, &color, &shadow);
        } else {
            snprintf(ppText1, sizeof(ppText1), "PP %d", pp);
            text = ppText1;
            color = pd_pp_color(pd_move_low_pp(d->source ? NULL : d->game, move, pp), PD_MUTED);
        }
        int rw = pd_text_width(text);
        pd_text_fit(c, ix, ty1, iw - rw - 6, move_name(d, move, buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
        pd_text_right(c, ix + iw, ty1, text, color, shadow);
        return;
    }
    int lines = detail ? 3 : 2;
    int ty = y + (h - lines * LINE - (lines - 1)) / 2;
    char ppText[12];
    snprintf(ppText, sizeof(ppText), "PP %d", pp);
    int ppW = pd_text_width(ppText);
    pd_text_fit(c, ix, ty, iw - ppW - 6, move_name(d, move, buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
    pd_text_right(c, ix + iw, ty, ppText, pd_pp_color(pd_move_low_pp(d->source ? NULL : d->game, move, pp), PD_MUTED),
                  PD_MUTED_SHADOW);
    ty += LINE + 1;
    int vx = ix;
    int kind = d->source ? -1 : pd_move_kind(d->game, move);
    static const char* const KINDS[3] = {"PHY", "SPE", "STA"};
    static const uint32_t KIND_FILL[3] = {0xD86030, 0x4878D0, 0x8C8C94};
    if (kind >= 0) vx += pill(c, vx, ty, KINDS[kind], KIND_FILL[kind]) + 4;
    const char* text = "";
    uint32_t color = PD_LABEL, shadow = PD_LABEL_SHADOW;
    if (move_power(d, move) == 0) {
        if (kind < 0) {
            text = "STATUS";
            color = PD_MUTED;
            shadow = PD_MUTED_SHADOW;
        }
    } else if (foe) {
        verdict(pd_type_multiplier_pct(d->game, move_type(d, move), foe->type1, foe->type2), &text, &color, &shadow);
    }
    pd_text_fit(c, vx, ty, ix + iw - vx, text, color, shadow);
    if (!detail) return;
    ty += LINE + 1;
    int power = move_power(d, move), acc = d->source ? -1 : pd_move_accuracy(d->game, move);
    char line[40];
    char pw[12], ac[12];
    if (power > 1) snprintf(pw, sizeof(pw), "%d", power);
    else snprintf(pw, sizeof(pw), "-");
    if (acc > 0) snprintf(ac, sizeof(ac), "%d%%", acc);
    else snprintf(ac, sizeof(ac), acc == 0 ? "ALWAYS" : "-");
    snprintf(line, sizeof(line), "POW %s  ACC %s", pw, ac);
    pd_text_fit(c, ix, ty, iw, line, PD_MUTED, PD_MUTED_SHADOW);
}

static int draw_moves(struct pd_canvas* c, const struct wing* w, int ix, int y, int iw, int h) {
    int cellH = (h - (PD_NUM_MOVES - 1) * MOVE_GAP) / PD_NUM_MOVES;
    int full = (w->detail ? 3 : 2) * LINE + 6;
    if (cellH > full) cellH = full;
    bool detail = w->detail && cellH >= 3 * LINE + 2;
    if (cellH < 2 * LINE && cellH > MOVE_ROW_MIN) cellH = MOVE_ROW_MIN;
    for (int i = 0; i < PD_NUM_MOVES && cellH >= MOVE_ROW_MIN; i++)
        wing_move(c, w->d, ix, y + i * (cellH + MOVE_GAP), iw, cellH, w->you.moves[i], w->you.pp[i], w->foe, detail);
    return PD_NUM_MOVES * cellH + (PD_NUM_MOVES - 1) * MOVE_GAP;
}

static void draw_speed(struct pd_canvas* c, const struct wing* w, int ix, int y, int iw) {
    int a = battle_speed(w->youB), b = battle_speed(w->foeB);
    const char* text = a > b ? "YOU MOVE FIRST" : a < b ? "FOE MOVES FIRST" : "SPEED TIE";
    int x = ix + pd_text(c, ix, y, "SPEED", PD_VALUE, PD_VALUE_SHADOW) + 5;
    char nums[24];
    snprintf(nums, sizeof(nums), "%d/%d", a, b);
    int nw = pd_text_width(nums);
    bool roomy = x + pd_text_width(text) + 6 + nw <= ix + iw;
    pd_text_fit(c, x, y, ix + iw - x - (roomy ? nw + 6 : 0), text, a > b ? PD_LABEL : PD_MUTED,
                a > b ? PD_LABEL_SHADOW : PD_MUTED_SHADOW);
    if (roomy) pd_text_right(c, ix + iw, y, nums, PD_MUTED, PD_MUTED_SHADOW);
}

// FOE IVS, compact: a bar an IV, the stats' short names under them, then the nature's + and -, hidden power's type
// and the EV total.
static void draw_foe_stats(struct pd_canvas* c, const struct wing* w, int ix, int y, int iw) {
    const struct pd_mon_stats* st = &w->foeStats->stats;
    static const char* const NAMES[6] = {"HP", "ATK", "DEF", "SPE", "SPA", "SPD"};
    int lx = ix + pd_text(c, ix, y, "IVS", PD_VALUE, PD_VALUE_SHADOW) + 6;
    int cell = (ix + iw - lx) / 6;
    for (int i = 0; i < 6; i++) {
        int bx = lx + i * cell, bw = cell - 4, iv = st->ivs[i];
        pd_round_rect(c, bx, y + 4, bw, 8, 2, PD_LIST_FILL);
        int fw = (bw - 2) * iv / 31;
        uint32_t fill = iv >= 25 ? OUT_GREEN : iv <= 5 ? 0xD84838 : 0xE8B830;
        if (fw > 0) pd_round_rect(c, bx + 1, y + 5, fw, 6, 1, fill);
        char n[8];
        snprintf(n, sizeof(n), "%d", iv);
        pd_text(c, bx + (bw - pd_text_width(n)) / 2, y + LINE - 2, n, PD_LABEL, PD_LABEL_SHADOW);
        pd_text(c, bx + (bw - pd_text_width(NAMES[i])) / 2, y + 2 * LINE - 4, NAMES[i], PD_MUTED, PD_MUTED_SHADOW);
    }
    y += LINE + 10 + LINE + 2;
    int x = ix;
    if (st->hasNature) {
        int up = pd_nature_raises(st->nature), down = pd_nature_lowers(st->nature);
        char t[24];
        if (up >= 0 && down >= 0 && up < 6 && down < 6) snprintf(t, sizeof(t), "+%s -%s", NAMES[up], NAMES[down]);
        else snprintf(t, sizeof(t), "NEUTRAL");
        x += pd_text(c, x, y, t, PD_LABEL, PD_LABEL_SHADOW) + 8;
    }
    int hp = st->hasHiddenPower ? st->hiddenPowerType : -1;
    if (hp >= 0 && x + type_badge_w(w->d->game, hp) + pd_text_width("HP ") <= ix + iw) {
        x += pd_text(c, x, y, "HP", PD_MUTED, PD_MUTED_SHADOW) + 3;
        x += ui_type_badge(c, x, y + 3, hp) + 8;
    }
    if (st->hasEvs) {
        int total = 0;
        for (int i = 0; i < 6; i++) total += st->evs[i];
        char t[16];
        snprintf(t, sizeof(t), "EV %d", total);
        if (x + pd_text_width(t) <= ix + iw) pd_text_right(c, ix + iw, y, t, PD_MUTED, PD_MUTED_SHADOW);
    }
}

static void draw_switch(struct pd_canvas* c, const struct wing* w, int ix, int y, int iw) {
    char buf[40];
    int nx = ix + pd_text(c, ix, y, "SWITCH TO", PD_VALUE, PD_VALUE_SHADOW) + 5;
    pd_text_fit(c, nx, y, ix + iw - nx, species_name(w->d, w->better->species, buf, sizeof(buf)), PD_LABEL,
                PD_LABEL_SHADOW);
}

// The trainer's team: a cell each, a ball until one has been out, a fainted one faded, the one out framed.
static void draw_foe_team(struct pd_canvas* c, const struct wing* w, int ix, int y, int iw, int h) {
    static uint32_t battleKey;
    static unsigned seen;
    const struct pd_snapshot* s = w->s;
    uint32_t key = (uint32_t) s->foeCount;
    for (int i = 0; i < s->foeCount; i++) key = key * 31u + (uint32_t) s->foes[i].species;
    if (key != battleKey) {
        battleKey = key;
        seen = 0;
    }
    for (int i = 0; i < s->foeCount; i++)
        if (i == s->foeActive || s->foes[i].hp < s->foes[i].maxHp) seen |= 1u << i;
    int n = s->foeCount, gap = 3, cell = h;
    if (n * cell + (n - 1) * gap > iw) cell = (iw - (n - 1) * gap) / n;
    int x = ix;
    for (int i = 0; i < n; i++, x += cell + gap) {
        const struct pd_mon* m = &s->foes[i];
        bool shown = seen >> i & 1, current = i == s->foeActive;
        struct pd_layer layers[2] = {{PD_VALUE, 2}, {PD_VALUE_SHADOW, 1}};
        struct pd_layer plain[2] = {{0x63737B, 2}, {0xCED6D6, 1}};
        pd_layered_box(c, x, y, cell, cell, 3, current ? layers : plain, 2, shown ? PD_TITLE_FILL : PD_LIST_FILL);
        if (!shown) ui_ball(c, x + (cell - 10) / 2, y + (cell - 10) / 2, true);
        else if (cell < PD_ICON || !draw_icon(w->d, c, m->species, m->shiny, x + (cell - PD_ICON) / 2,
                                              y + (cell - PD_ICON) / 2, m->hp == 0)) {
            char lv[8];
            snprintf(lv, sizeof(lv), "%d", m->level);
            pd_text(c, x + (cell - pd_text_width(lv)) / 2, pd_text_y(y, cell), lv, m->hp ? PD_LABEL : PD_MUTED,
                    PD_NO_SHADOW);
        }
    }
}

void pd_ui_battle_panel(struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                        const struct pd_battle_source* source) {
    ui_game = g;
    const struct battle_data data = {g, source};
    int f = pd_title_box(c, 0, 0, c->w, c->h, PD_TITLE_FILL);
    int ix = f + 4, iy = f + 4, iw = c->w - 2 * (f + 4), bottom = c->h - f - 4;
    const struct pd_battle_mon* youB = &s->battlers[PD_POS_PLAYER_LEFT];
    if (!s->inBattle || !youB->species) {
        pd_text_wrap(c, ix, iy, iw, LINE, 3, "NO BATTLE", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    static struct wing w;
    memset(&w, 0, sizeof(w));
    w.d = &data;
    w.s = s;
    w.youB = youB;
    w.foeB = &s->battlers[PD_POS_OPPONENT_LEFT];
    w.you = from_battler(youB);
    w.mark = -1;
    if (w.foeB->species) {
        w.foeView = from_battler(w.foeB);
        w.foe = &w.foeView;
        focus_from_battler(&w.foeF, w.foeB);
        for (int i = 0; i < 7 && w.foeF.hasStages; i++) w.stages = w.stages || w.foeF.stages[i] != 0;
        w.mark = dex_mark(&data, s, w.foeB->species);
        w.better = better_switch(&data, s, w.foe);
    }
    w.foeStats = source ? NULL : foe_stats_mon(s);

    // The sections in LAYOUT's order, then what goes when they don't all fit.
    int list[PD_WING_LIST_MAX], n = 0;
    const int* from = pd_ui_battle_list ? pd_ui_battle_list : pd_battle_default;
    int count = pd_ui_battle_list ? pd_ui_battle_count : pd_battle_default_count;
    for (int i = 0; i < count && n < PD_WING_LIST_MAX; i++)
        if (from[i] >= 0 && from[i] < PD_BATTLE_SECTIONS) list[n++] = from[i];
    bool on[PD_BATTLE_SECTIONS] = {false};
    for (int i = 0; i < n; i++) on[list[i]] = true;
    int room = bottom - iy;
    w.detail = true;
    int total = 0;
    for (int i = 0; i < n; i++) {
        int h = on[list[i]] ? section_h(&w, list[i]) : 0;
        if (h) total += h + (total ? RULE : 0);
    }
    static const int DROP[] = {-1, PD_BS_SWITCH, PD_BS_FOE_TEAM, PD_BS_SPEED};
    for (size_t k = 0; k < sizeof DROP / sizeof DROP[0] && total > room; k++) {
        if (DROP[k] < 0) w.detail = false;
        else on[DROP[k]] = false;
        total = 0;
        for (int i = 0; i < n; i++) {
            int h = on[list[i]] ? section_h(&w, list[i]) : 0;
            if (h) total += h + (total ? RULE : 0);
        }
    }
    // Still too much: the moves give what they can (down to a line each), then FOE IVS goes.
    if (total > room && on[PD_BS_MOVES] && on[PD_BS_FOE_STATS] && section_h(&w, PD_BS_FOE_STATS) &&
        total - room > moves_h(false) - moves_min_h()) {
        total -= section_h(&w, PD_BS_FOE_STATS) + RULE;
        on[PD_BS_FOE_STATS] = false;
    }
    int y = iy;
    bool first = true;
    for (int i = 0; i < n; i++) {
        int sec = list[i];
        int h = on[sec] ? section_h(&w, sec) : 0;
        if (!h) continue;
        if (!first) {
            rule(c, ix, y, iw);
            y += RULE;
        }
        first = false;
        switch (sec) {
        case PD_BS_CARDS: y += draw_cards(c, &w, ix, y, iw); break;
        case PD_BS_MOVES: {
            // The moves take what the others leave them.
            int others = 0;
            for (int k = i + 1; k < n; k++) {
                int hk = on[list[k]] ? section_h(&w, list[k]) : 0;
                if (hk) others += hk + RULE;
            }
            int give = bottom - y - others;
            if (give > h) give = h;
            y += draw_moves(c, &w, ix, y, iw, give);
            break;
        }
        case PD_BS_SPEED: draw_speed(c, &w, ix, y + 1, iw); y += h; break;
        case PD_BS_FOE_STATS: draw_foe_stats(c, &w, ix, y + 1, iw); y += h; break;
        case PD_BS_SWITCH: draw_switch(c, &w, ix, y + 1, iw); y += h; break;
        case PD_BS_FOE_TEAM: draw_foe_team(c, &w, ix, y, iw, h); y += h; break;
        }
        if (y > bottom) break;
    }
}
