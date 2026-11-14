/* GPL-3.0, see ../NOTICE. PC FAN GAMES' wings (pc_wings.h). */
#include "pc_wings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb/stb_image.h"
#include "pd_icon.h"
#include "pd_snapshot.h"
#include "pd_style.h"
#include "pd_tables.h"
#include "pd_ui.h"
#include "pd_ui_internal.h"

#define LINE 16

/* ---- the modern chart, Fairy as 9: attacker by defender in quarters (4 = 1x) ---- */
enum { NOR, FIG, FLY, POI, GRO, ROC, BUG, GHO, STE, FAI, FIR, WAT, GRA, ELE, PSY, ICE, DRA, DAR };
static unsigned char chart[18 * 18];
static struct pd_ds_types modern = {"Fairy", chart};

static void chart_fill(void) {
  static const signed char pairs[][3] = {
      {NOR, ROC, 2}, {NOR, GHO, 0}, {NOR, STE, 2},
      {FIG, NOR, 8}, {FIG, FLY, 2}, {FIG, POI, 2}, {FIG, ROC, 8}, {FIG, BUG, 2}, {FIG, GHO, 0}, {FIG, STE, 8},
      {FIG, FAI, 2}, {FIG, PSY, 2}, {FIG, ICE, 8}, {FIG, DAR, 8},
      {FLY, FIG, 8}, {FLY, ROC, 2}, {FLY, BUG, 8}, {FLY, STE, 2}, {FLY, GRA, 8}, {FLY, ELE, 2},
      {POI, POI, 2}, {POI, GRO, 2}, {POI, ROC, 2}, {POI, GHO, 2}, {POI, STE, 0}, {POI, FAI, 8}, {POI, GRA, 8},
      {GRO, FLY, 0}, {GRO, POI, 8}, {GRO, ROC, 8}, {GRO, BUG, 2}, {GRO, STE, 8}, {GRO, FIR, 8}, {GRO, GRA, 2},
      {GRO, ELE, 8},
      {ROC, FIG, 2}, {ROC, FLY, 8}, {ROC, GRO, 2}, {ROC, BUG, 8}, {ROC, STE, 2}, {ROC, FIR, 8}, {ROC, ICE, 8},
      {BUG, FIG, 2}, {BUG, FLY, 2}, {BUG, POI, 2}, {BUG, GHO, 2}, {BUG, STE, 2}, {BUG, FAI, 2}, {BUG, FIR, 2},
      {BUG, GRA, 8}, {BUG, PSY, 8}, {BUG, DAR, 8},
      {GHO, NOR, 0}, {GHO, GHO, 8}, {GHO, PSY, 8}, {GHO, DAR, 2},
      {STE, ROC, 8}, {STE, STE, 2}, {STE, FAI, 8}, {STE, FIR, 2}, {STE, WAT, 2}, {STE, ELE, 2}, {STE, ICE, 8},
      {FAI, FIG, 8}, {FAI, POI, 2}, {FAI, STE, 2}, {FAI, FIR, 2}, {FAI, DRA, 8}, {FAI, DAR, 8},
      {FIR, ROC, 2}, {FIR, BUG, 8}, {FIR, STE, 8}, {FIR, FIR, 2}, {FIR, WAT, 2}, {FIR, GRA, 8}, {FIR, ICE, 8},
      {FIR, DRA, 2},
      {WAT, GRO, 8}, {WAT, ROC, 8}, {WAT, FIR, 8}, {WAT, WAT, 2}, {WAT, GRA, 2}, {WAT, DRA, 2},
      {GRA, FLY, 2}, {GRA, POI, 2}, {GRA, GRO, 8}, {GRA, ROC, 8}, {GRA, BUG, 2}, {GRA, STE, 2}, {GRA, FIR, 2},
      {GRA, WAT, 8}, {GRA, GRA, 2}, {GRA, DRA, 2},
      {ELE, FLY, 8}, {ELE, GRO, 0}, {ELE, WAT, 8}, {ELE, GRA, 2}, {ELE, ELE, 2}, {ELE, DRA, 2},
      {PSY, FIG, 8}, {PSY, POI, 8}, {PSY, STE, 2}, {PSY, PSY, 2}, {PSY, DAR, 0},
      {ICE, FLY, 8}, {ICE, GRO, 8}, {ICE, STE, 2}, {ICE, FIR, 2}, {ICE, WAT, 2}, {ICE, GRA, 8}, {ICE, ICE, 2},
      {ICE, DRA, 8},
      {DRA, STE, 2}, {DRA, FAI, 0}, {DRA, DRA, 8},
      {DAR, FIG, 2}, {DAR, GHO, 8}, {DAR, FAI, 2}, {DAR, PSY, 8}, {DAR, DAR, 2},
  };
  memset(chart, 4, sizeof chart);
  for (size_t i = 0; i < sizeof pairs / sizeof pairs[0]; i++)
    chart[pairs[i][0] * 18 + pairs[i][1]] = (unsigned char)pairs[i][2];
}

void pc_wings_types(bool on) {
  if (on && chart[0] != 4)
    chart_fill();
  pd_ds_types = on ? &modern : NULL;
}

/* ---- icons: the game's own pictures (an Essentials icon is two square frames side by side) ---- */
#define ICON_CACHE 24
static struct {
  char path[300];
  uint32_t px[PD_ICON * PD_ICON]; // 0xAARRGGBB, alpha 0 = see-through
  bool ok;
  unsigned used;
} icons[ICON_CACHE];
static unsigned iconTick;

static const uint32_t *icon_load(const char *gameDir, const char *rel) {
  if (!rel || !*rel)
    return NULL;
  iconTick++;
  int slot = 0;
  for (int i = 0; i < ICON_CACHE; i++) {
    if (!strcmp(icons[i].path, rel)) {
      icons[i].used = iconTick;
      return icons[i].ok ? icons[i].px : NULL;
    }
    if (icons[i].used < icons[slot].used)
      slot = i;
  }
  snprintf(icons[slot].path, sizeof icons[slot].path, "%s", rel);
  icons[slot].used = iconTick;
  icons[slot].ok = false;
  char path[1400];
  int w = 0, h = 0, n = 0;
  unsigned char *img = NULL;
  static const char *const exts[] = {"", ".png", ".PNG"};
  for (int e = 0; e < 3 && !img; e++) {
    if (rel[0] == '/' || (rel[0] && rel[1] == ':'))
      snprintf(path, sizeof path, "%s%s", rel, exts[e]);
    else
      snprintf(path, sizeof path, "%s/%s%s", gameDir, rel, exts[e]);
    img = stbi_load(path, &w, &h, &n, 4);
  }
  if (!img)
    return NULL;
  // The first frame: Essentials' frames are 64 pixels square, in a row (and some sheets, a grid of rows: forms).
  int fw = w < h ? w : h;
  if (fw > 64 && fw % 64 == 0 && w % 64 == 0 && h % 64 == 0)
    fw = 64;
  int fh = fw;
  // Its drawn part, cropped to the pixels that show, then fitted to the icon's square (nearest, whole steps).
  int x0 = fw, y0 = fh, x1 = -1, y1 = -1;
  for (int y = 0; y < fh; y++)
    for (int x = 0; x < fw; x++)
      if (img[(y * w + x) * 4 + 3] > 40) {
        if (x < x0) x0 = x;
        if (y < y0) y0 = y;
        if (x > x1) x1 = x;
        if (y > y1) y1 = y;
      }
  if (x1 < 0) {
    stbi_image_free(img);
    return NULL;
  }
  int cw = x1 - x0 + 1, ch = y1 - y0 + 1, side = cw > ch ? cw : ch;
  int step = (side + PD_ICON - 1) / PD_ICON;
  if (step < 1)
    step = 1;
  // Keep the frame's own scale where it fits (64 px icons halve to 32), centred on the drawn part.
  int fstep = (fw + PD_ICON - 1) / PD_ICON;
  if (fstep >= step && fw / fstep <= PD_ICON)
    step = fstep;
  int cx = x0 + cw / 2, cy = y0 + ch / 2;
  memset(icons[slot].px, 0, sizeof icons[slot].px);
  for (int y = 0; y < PD_ICON; y++)
    for (int x = 0; x < PD_ICON; x++) {
      int sx = cx + (x - PD_ICON / 2) * step, sy = cy + (y - PD_ICON / 2) * step;
      if (sx < 0 || sy < 0 || sx >= fw || sy >= fh)
        continue;
      const unsigned char *p = img + (sy * w + sx) * 4;
      if (p[3] > 100)
        icons[slot].px[y * PD_ICON + x] = 0xFF000000u | (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
    }
  stbi_image_free(img);
  icons[slot].ok = true;
  return icons[slot].px;
}

static void icon_draw(struct pd_canvas *c, const uint32_t *px, int x, int y, bool fainted) {
  for (int j = 0; j < PD_ICON; j++)
    for (int i = 0; i < PD_ICON; i++) {
      uint32_t v = px[j * PD_ICON + i];
      if (!(v >> 24))
        continue;
      if (fainted) { // grey, as the party menu shows a fainted one
        uint32_t g = (((v >> 16) & 255) * 30 + ((v >> 8) & 255) * 59 + (v & 255) * 11) / 100;
        v = g << 16 | g << 8 | g;
      }
      pd_fill(c, x + i, y + j, 1, 1, v & 0xFFFFFF);
    }
}

void pc_wings_free(void) {
  memset(icons, 0, sizeof icons);
  iconTick = 0;
}

void pc_play_time(long seconds, char *out, int size) {
  if (seconds < 0)
    seconds = 0;
  snprintf(out, (size_t)size, "%ld:%02ld", seconds / 3600, (seconds / 60) % 60);
}

void pc_side_panel(struct pd_canvas *c, int which, int hour, int minute, int battery, bool charging, long playSeconds,
                   int zoomPct) {
  char big[24] = "", small[32] = "";
  const char *top = which ? "PLAY TIME" : "CLOCK";
  if (which) {
    pc_play_time(playSeconds, big, sizeof big);
    snprintf(small, sizeof small, "ZOOM %d%%", zoomPct);
  } else {
    if (hour >= 0)
      snprintf(big, sizeof big, "%02d:%02d", hour, minute);
    if (battery >= 0)
      snprintf(small, sizeof small, "BATTERY %d%%%s", battery, charging ? "+" : "");
  }
  // The big line at 2x when it fits, else 1x.
  int k = pd_text_width(big) * 2 + 8 <= c->w ? 2 : 1;
  int h = 16 + 16 * k + (small[0] ? 18 : 0), y = (c->h - h) / 2;
  pd_text_fit(c, (c->w - pd_text_width(top)) / 2 > 2 ? (c->w - pd_text_width(top)) / 2 : 2, y, c->w - 4, top,
              PD_MUTED, PD_MUTED_SHADOW);
  y += 16;
  if (big[0])
    pd_text_scaled(c, (c->w - pd_text_width(big) * k) / 2, y, big, PD_VALUE, PD_VALUE_SHADOW, k);
  y += 16 * k + 2;
  if (small[0]) {
    int sw = pd_text_width(small);
    pd_text_fit(c, sw < c->w - 4 ? (c->w - sw) / 2 : 2, y, c->w - 4, small, PD_LABEL, PD_LABEL_SHADOW);
  }
}

/* ---- the party ---- */
struct icon_ctx {
  const struct pc_state *s;
  const char *dir;
};
static bool party_icon(struct pd_canvas *c, void *data, int slot, int x, int y, bool fainted) {
  struct icon_ctx *k = data;
  const uint32_t *px = slot < k->s->partyCount ? icon_load(k->dir, k->s->party[slot].icon) : NULL;
  if (!px)
    return false;
  icon_draw(c, px, x, y, fainted);
  return true;
}

static int type_or_none(const char *t) {
  int i = pc_type_index(t);
  return i < 0 ? PD_TYPE_NONE : i;
}

static uint32_t hash(const char *s, uint32_t h) {
  for (; *s; s++)
    h = (h ^ (uint8_t)*s) * 16777619u;
  return h;
}

void pc_wing_left(struct pd_canvas *c, const struct pc_state *s, const char *gameDir) {
  static struct pd_snapshot snap;
  memset(&snap, 0, sizeof snap);
  snap.valid = s->valid && s->partyCount > 0;
  snap.partyCount = s->partyCount;
  for (int i = 0; i < s->partyCount && i < PD_PARTY_SIZE; i++) {
    const struct pc_mon *m = &s->party[i];
    struct pd_mon *d = &snap.party[i];
    d->species = m->egg ? PD_SPECIES_EGG : 1;
    snprintf(d->nickname, sizeof d->nickname, "%s", m->egg ? "EGG" : m->name[0] ? m->name : m->species);
    d->level = m->level;
    d->hp = m->hp;
    d->maxHp = m->maxHp;
    d->status = pc_status_bits(m->status);
    d->shiny = m->shiny;
    d->heldItem = m->item[0] ? 1 : 0;
    d->hasHeldItem = true;
    d->personality = hash(m->id, hash(m->name, 2166136261u)); // the slide-in's key: who's where
    d->hasTypes = true;
    d->type1 = type_or_none(m->types[0]);
    d->type2 = m->types[1][0] ? type_or_none(m->types[1]) : d->type1;
    d->egg = m->egg;
  }
  struct icon_ctx k = {s, gameDir};
  pd_ui_party_panel_icons(c, NULL, &snap, party_icon, &k);
}

/* ---- the right wing ---- */
static int heading(struct pd_canvas *c, int x, int y, int w, const char *text) {
  pd_heading_fit(c, x, y, w, text, PD_MUTED, PD_MUTED_SHADOW);
  return y + LINE;
}

static int fact(struct pd_canvas *c, int x, int y, int w, const char *label, const char *value) {
  pd_text(c, x, y, label, PD_MUTED, PD_MUTED_SHADOW);
  int lw = pd_text_width(label) + 6, vw = pd_text_width(value);
  if (vw > w - lw)
    vw = w - lw;
  pd_text_fit(c, x + w - vw, y, w - lw, value, PD_LABEL, PD_LABEL_SHADOW);
  return y + LINE;
}

/* The types that hit T1 / T2 for more than normal, most first: how many written to OUT. */
static int weak_to(int t1, int t2, int *out, int max) {
  int n = 0, pct[18];
  for (int a = 0; a < 18; a++) {
    pct[a] = pd_type_multiplier_pct(NULL, a, t1, t2 == PD_TYPE_NONE ? t1 : t2);
    if (pct[a] > 100 && n < 18)
      out[n++] = a;
  }
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && pct[out[j]] > pct[out[j - 1]]; j--) {
      int t = out[j];
      out[j] = out[j - 1];
      out[j - 1] = t;
    }
  return n < max ? n : max;
}

/* Pills in rows across W from (x, y): returns the y under them. */
static int pills(struct pd_canvas *c, int x, int y, int w, const int *types, int n) {
  int px = 0;
  for (int i = 0; i < n; i++) {
    int pw = ui_type_pills(NULL, 0, 0, types[i], PD_TYPE_NONE, w, false);
    if (px && px + pw > w) {
      px = 0;
      y += 14;
    }
    ui_type_pills(c, x + px, y, types[i], PD_TYPE_NONE, w, false);
    px += pw + 3;
  }
  return y + (n ? 16 : 0);
}

static int foe_card(struct pd_canvas *c, const struct pc_mon *m, const char *dir, int x, int y, int w, int bottom) {
  int t1 = type_or_none(m->types[0]), t2 = m->types[1][0] ? type_or_none(m->types[1]) : PD_TYPE_NONE;
  int h = PD_ICON + 4 + 2 * LINE + 4;
  if (y + h > bottom)
    return y;
  pd_round_rect(c, x, y, w, h, 3, PD_LIST_FILL);
  const uint32_t *px = icon_load(dir, m->icon);
  if (px)
    icon_draw(c, px, x + 3, y + 2, false);
  else
    ui_ball(c, x + 12, y + 12, false);
  int tx = x + PD_ICON + 6, right = x + w - 4;
  char lv[12];
  snprintf(lv, sizeof lv, "%d", m->level);
  int lvW = pd_text_width("Lv") + pd_number_width(lv);
  pd_text_fit(c, tx, y + 2, right - tx, m->species, PD_LABEL, PD_LABEL_SHADOW);
  int lx = tx;
  lx += pd_text(c, lx, y + 2 + LINE, "Lv", PD_MUTED, PD_MUTED_SHADOW);
  pd_number(c, lx, y + 2 + LINE, lv, PD_LABEL, PD_LABEL_SHADOW);
  (void)lvW;
  if (*m->status)
    ui_status_badge(c, right - pd_text_width(m->status) - 6, y + 3 + LINE, pc_status_bits(m->status));
  int by = y + PD_ICON + 6;
  if (w - 30 >= 12)
    ui_hp_bar(c, x + 4, by, w - 26, m->hp, m->maxHp > 0 ? m->maxHp : 1);
  by += 10;
  if (t1 != PD_TYPE_NONE)
    ui_type_pills(c, x + 4, by, t1, t2, w - 8, false);
  return y + h + 4;
}

void pc_wing_right(struct pd_canvas *c, const struct pc_state *s, const char *gameDir) {
  ui_game = NULL;
  int f = pd_title_box(c, 0, 0, c->w, c->h, PD_TITLE_FILL);
  int x = f + 4, y = f + 3, w = c->w - 2 * (f + 4), bottom = c->h - f - 3;
  if (!s->valid) {
    pd_heading(c, x, y, "HERE", PD_VALUE, PD_VALUE_SHADOW);
    pd_text_wrap(c, x, y + LINE + 3, w, LINE, 3, "WAITING FOR THE GAME", PD_MUTED, PD_MUTED_SHADOW);
    return;
  }
  if (s->battle && s->foeCount > 0) {
    pd_heading_fit(c, x, y, w, s->foeTeam ? "TRAINER BATTLE" : "WILD BATTLE", PD_VALUE, PD_VALUE_SHADOW);
    y += LINE + 3;
    for (int i = 0; i < s->foeCount; i++) {
      const struct pc_mon *m = &s->foes[i];
      y = foe_card(c, m, gameDir, x, y, w, bottom);
      int t1 = type_or_none(m->types[0]), t2 = m->types[1][0] ? type_or_none(m->types[1]) : PD_TYPE_NONE;
      if (t1 != PD_TYPE_NONE && y + LINE + 16 <= bottom) {
        int weak[18], n = weak_to(t1, t2, weak, 6);
        y = heading(c, x, y, w, n ? "WEAK TO" : "NO WEAKNESS");
        y = pills(c, x, y, w, weak, n) + 4;
      }
    }
    if (s->foeTeam > 0 && y + LINE + 12 <= bottom) {
      y = heading(c, x, y, w, "FOE TEAM");
      for (int i = 0; i < s->foeTeam && i < 6; i++)
        ui_ball(c, x + 2 + i * 13, y, i < s->foeAlive);
    }
    return;
  }
  // HERE: the place, then its wild Pokemon.
  pd_heading_fit(c, x, y, w, s->map[0] ? s->map : "HERE", PD_VALUE, PD_VALUE_SHADOW);
  y += LINE + 3;
  int keep = 3 * LINE + 8 + 50; // QUICK INFO and PARTY WEAK TO keep their room below
  if (s->wildCount > 0 && y + LINE * 2 <= bottom - keep) {
    y = heading(c, x, y, w, "WILD HERE");
    for (int i = 0; i < s->wildCount && y + LINE <= bottom - keep; i++) {
      const struct pc_wild *e = &s->wild[i];
      char lv[24];
      if (e->min == e->max)
        snprintf(lv, sizeof lv, "%d", e->min);
      else
        snprintf(lv, sizeof lv, "%d-%d", e->min, e->max);
      int lw = pd_number_width(lv);
      int t1 = type_or_none(e->types[0]);
      uint32_t ink = t1 == PD_TYPE_NONE ? PD_MUTED : ui_type_color(t1);
      pd_fill(c, x, y + 4, 3, 8, ink); // the species' first type, a stripe
      pd_text_fit(c, x + 6, y, w - lw - 10, e->species, PD_LABEL, PD_LABEL_SHADOW);
      pd_number_right(c, x + w, y, lv, PD_MUTED, PD_MUTED_SHADOW);
      y += LINE;
    }
    y += 4;
  }
  // QUICK INFO
  if (y + 3 * LINE <= bottom) {
    char v[32];
    y = heading(c, x, y, w, "QUICK INFO");
    snprintf(v, sizeof v, "%d", s->badges);
    y = fact(c, x, y, w, "BADGES", v);
    if (s->money >= 0) {
      pd_ui_format_money(s->money, v, sizeof v);
      y = fact(c, x, y, w, "MONEY", v);
    }
    pc_play_time(s->seconds, v, sizeof v);
    y = fact(c, x, y, w, "TIME", v);
    y += 4;
  }
  // PARTY WEAK TO: the types two or more of the party are weak to.
  if (s->partyCount > 0 && y + LINE + 16 <= bottom) {
    int weak[18] = {0}, n = 0, need = s->partyCount >= 3 ? 2 : 1;
    for (int i = 0; i < s->partyCount; i++) {
      const struct pc_mon *m = &s->party[i];
      int t1 = type_or_none(m->types[0]), t2 = m->types[1][0] ? type_or_none(m->types[1]) : PD_TYPE_NONE;
      if (m->egg || t1 == PD_TYPE_NONE)
        continue;
      n++;
      for (int a = 0; a < 18; a++)
        weak[a] += pd_type_multiplier_pct(NULL, a, t1, t2 == PD_TYPE_NONE ? t1 : t2) > 100;
    }
    int order[18], k = 0;
    for (int a = 0; n && a < 18; a++)
      if (weak[a] >= need)
        order[k++] = a;
    for (int i = 1; i < k; i++)
      for (int j = i; j > 0 && weak[order[j]] > weak[order[j - 1]]; j--) {
        int t = order[j];
        order[j] = order[j - 1];
        order[j - 1] = t;
      }
    if (k > 6)
      k = 6;
    y = heading(c, x, y, w, "PARTY WEAK TO");
    if (!k)
      pd_text_fit(c, x + 4, y, w - 4, "NO SHARED WEAKNESS", PD_LABEL, PD_LABEL_SHADOW);
    else
      pills(c, x, y, w, order, k);
  }
}
