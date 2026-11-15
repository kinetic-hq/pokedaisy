/* GPL-3.0, see ../NOTICE. The game list's grid (game_grid.h). */
#include "game_grid.h"
#include "boxart.h"
#include "pd_style.h"

#include <stdio.h>
#include <string.h>

#define PAD 8
#define BAR_H 26
#define TAB_H 20 // COLLECTIONS' row of tabs, under the bar
#define HINT_H 16
#define TARGET_TILE 92 // tiles are about this wide (the system and ROMHACK / ORIGINAL side by side); the columns follow
#define TAG_H 12
#define LINE_H 12 // a line of a tile's name
#define NAME_LINES 2 // room kept for the name (a third line takes the version's)
#define RING 0xFF8A1Eu // the app's focus ring (AppColors.focusRing)

struct palette {
  uint32_t page, bar, barEdge, tile, tileEdge, tileOn, well, text, muted;
};
static struct palette colors(void) {
  if (pd_dark)
    return (struct palette){0x111318, 0x0A0B0E, 0x23262E, 0x1C1F26, 0x2A2E37, 0x272B35, 0x0D0F13, 0xF2F4F8, 0x8B93A1};
  return (struct palette){0xE9ECF1, 0xFFFFFF, 0xD3D8E0, 0xFFFFFF, 0xD3D8E0, 0xFFFFFF, 0xDDE1E8, 0x1E2229, 0x6B7280};
}

static const char *const SYSTEM_NAMES[] = {"GBA", "GBC", "GB", "NDS", "PC"};
static const uint32_t SYSTEM_COLORS[] = {0x5A6FD8, 0x8E5BD0, 0x6E9B3A, 0xC2415E, 0x2F7FB8};

void game_grid_init(struct game_grid *g, const struct game_grid_item *items, int count) {
  memset(g, 0, sizeof *g);
  g->items = items;
  g->count = count;
  g->pressedTile = g->pressedBar = g->pressedTab = g->tappedTab = -1;
  g->follow = true;
}

static void layout(struct game_grid *g, int w, int h) {
  g->drawnW = w;
  g->drawnH = h;
  g->gap = PAD;
  g->columns = (w - PAD) / (TARGET_TILE + PAD);
  if (g->columns < 2)
    g->columns = 2;
  g->tileW = (w - PAD * (g->columns + 1)) / g->columns;
  int aw = g->tileW - 8;
  // The art's shape (ART STYLE): SteamGridDB's 600x900 grid, or a GBA or DS box (about square).
  g->artH = g->artStyle == BOXART_STYLE_TALL ? aw * 3 / 2 : aw;
  g->tileH = 4 + g->artH + 3 + NAME_LINES * LINE_H + 3 + TAG_H + 2 + 12 + 2;
  g->gridTop = BAR_H + 6 + (g->tabs && g->tabCount > 1 ? TAB_H : 0);
  g->gridBottom = h - (g->hint ? HINT_H : 0) - 2;
}

static int rows_of(const struct game_grid *g) {
  return g->columns > 0 ? (g->count + g->columns - 1) / g->columns : 0;
}

static int max_scroll(const struct game_grid *g) {
  int content = rows_of(g) * (g->tileH + PAD) + PAD;
  int view = g->gridBottom - g->gridTop;
  return content > view ? content - view : 0;
}

static void clamp_scroll(struct game_grid *g) {
  int m = max_scroll(g);
  if (g->scrollY > m)
    g->scrollY = m;
  if (g->scrollY < 0)
    g->scrollY = 0;
}

static void tile_rect(const struct game_grid *g, int i, int *x, int *y) {
  int col = i % g->columns, row = i / g->columns;
  *x = PAD + col * (g->tileW + PAD);
  *y = g->gridTop + PAD / 2 + row * (g->tileH + PAD) - g->scrollY;
}

static int tag(struct pd_canvas *c, int x, int y, const char *text, uint32_t color) {
  int w = pd_text_width(text) + 6;
  pd_round_rect(c, x, y, w, TAG_H, 3, color);
  pd_text(c, x + 3, y - 3, text, 0xFFFFFF, PD_NO_SHADOW);
  return w;
}

/* A name in at most `most` lines of width w: one line if it fits, else the break that makes the longest line shortest
 * (two lines, then three); the last line cut to fit when even that doesn't. starts[] / ends[] index name. */
static int span_width(const char *name, int from, int to) {
  char buf[256];
  snprintf(buf, sizeof buf, "%.*s", to - from, name + from);
  return pd_text_width(buf);
}

static int wrap_name(const char *name, int w, int most, int starts[3], int ends[3]) {
  int words[24], wordEnd[24], n = 0;
  for (int i = 0; name[i] && n < 24;) {
    while (name[i] == ' ')
      i++;
    if (!name[i])
      break;
    words[n] = i;
    while (name[i] && name[i] != ' ')
      i++;
    wordEnd[n++] = i;
  }
  if (!n)
    return 0;
#define SPAN(a, b) span_width(name, words[a], wordEnd[b])
  if (SPAN(0, n - 1) <= w || most == 1 || n == 1) {
    starts[0] = words[0], ends[0] = wordEnd[n - 1];
    return 1;
  }
  int best = 1 << 30, b1 = -1, b2 = -1;
  for (int i = 0; i + 1 < n; i++) {
    int m = SPAN(0, i) > SPAN(i + 1, n - 1) ? SPAN(0, i) : SPAN(i + 1, n - 1);
    if (m <= w && m < best)
      best = m, b1 = i;
  }
  if (b1 >= 0 || most == 2 || n == 2) {
    if (b1 < 0) // nothing fits in two: the even split, its second line cut
      for (int i = 0; i + 1 < n; i++) {
        int m = SPAN(0, i) > SPAN(i + 1, n - 1) ? SPAN(0, i) : SPAN(i + 1, n - 1);
        if (m < best)
          best = m, b1 = i;
      }
    starts[0] = words[0], ends[0] = wordEnd[b1];
    starts[1] = words[b1 + 1], ends[1] = wordEnd[n - 1];
    return 2;
  }
  best = 1 << 30;
  for (int i = 0; i + 2 < n; i++)
    for (int j = i + 1; j + 1 < n; j++) {
      int a = SPAN(0, i), b = SPAN(i + 1, j), d = SPAN(j + 1, n - 1);
      int m = a > b ? a : b;
      m = m > d ? m : d;
      bool fits = m <= w;
      int score = fits ? m : (1 << 20) + m;
      if (score < best)
        best = score, b1 = i, b2 = j;
    }
#undef SPAN
  starts[0] = words[0], ends[0] = wordEnd[b1];
  starts[1] = words[b1 + 1], ends[1] = wordEnd[b2];
  starts[2] = words[b2 + 1], ends[2] = wordEnd[n - 1];
  return 3;
}

static void draw_tile(struct game_grid *g, struct pd_canvas *c, int i, const struct palette *p) {
  const struct game_grid_item *it = &g->items[i];
  int x, y;
  tile_rect(g, i, &x, &y);
  if (y + g->tileH < g->gridTop || y > g->gridBottom)
    return;
  bool on = g->focus == GAME_GRID_TILES && i == g->selected;
  if (on)
    pd_round_rect(c, x - 2, y - 2, g->tileW + 4, g->tileH + 4, 7, RING);
  pd_round_rect(c, x, y, g->tileW, g->tileH, 6, on ? RING : p->tileEdge);
  pd_round_rect(c, x + 1, y + 1, g->tileW - 2, g->tileH - 2, 5, on ? p->tileOn : p->tile);
  // Every game here is a Pokémon game: the tile shows the rest of the name, which then fits.
  const char *name = it->name ? it->name : "";
  if (!strncmp(name, "Pok\xC3\xA9mon ", 9) && name[9])
    name += 9;
  // The art's well, until a picture covers it: a title card. The system's colour, darkened, a lighter band across
  // it, the game's name in the middle and the system's name at its foot.
  int ax = x + 4, ay = y + 4, aw = g->tileW - 8, ah = g->artH;
  int sys = it->system >= 0 && it->system <= BOXART_PC ? it->system : 0;
  uint32_t sc = SYSTEM_COLORS[sys];
  uint32_t well = pd_dark ? ((sc >> 2) & 0x3F3F3F) + 0x0A0A0A : (sc & 0xFEFEFE) / 2 + 0x7F7F7F;
  uint32_t band = pd_dark ? ((sc >> 1) & 0x7F7F7F) : (sc & 0xFEFEFE) / 2 + 0x5F5F5F;
  pd_round_rect(c, ax, ay, aw, ah, 4, well);
  pd_fill(c, ax, ay + ah * 3 / 10, aw, ah * 4 / 10, band);
  pd_fill(c, ax, ay + ah * 3 / 10, aw, 1, sc);
  pd_fill(c, ax, ay + ah * 7 / 10 - 1, aw, 1, sc);
  {
    int cs[3], ce[3];
    int n = wrap_name(name, aw - 10, 3, cs, ce);
    int top = ay + ah / 2 - n * LINE_H / 2 - 3;
    for (int l = 0; l < n; l++) {
      char line[256];
      snprintf(line, sizeof line, "%.*s", ce[l] - cs[l], name + cs[l]);
      int lw = pd_text_width(line);
      if (lw > aw - 6)
        pd_text_fit(c, ax + 3, top + l * LINE_H, aw - 6, line, 0xFFFFFF, 0x202020);
      else
        pd_text(c, ax + (aw - lw) / 2, top + l * LINE_H, line, 0xFFFFFF, 0x202020);
    }
  }
  const char *sn = SYSTEM_NAMES[sys];
  const char *foot = sys == BOXART_PC ? "PC FAN GAME" : sn;
  pd_text(c, ax + (aw - pd_text_width(foot)) / 2, ay + ah - 16, foot, pd_dark ? 0x9AA3B2 : 0xFFFFFF, PD_NO_SHADOW);
  // A hack shown with its base game's box: its name on a banner across the box's foot (the art stops above it).
  int bannerH = it->banner ? 15 : 0;
  if (g->artCount < GAME_GRID_ART_MAX) {
    struct game_grid_art *a = &g->art[g->artCount++];
    a->x = ax, a->y = ay, a->w = aw, a->h = ah - bannerH, a->index = i;
    a->clipY = g->gridTop;
    a->clipH = g->gridBottom - g->gridTop;
  }
  if (bannerH) {
    pd_fill(c, ax, ay + ah - bannerH, aw, bannerH, 0xD9822B);
    pd_text_fit(c, ax + 3, ay + ah - bannerH - 1, aw - 6, name, 0xFFFFFF, PD_NO_SHADOW);
  }
  int ty = ay + ah + 3;
  int starts[3], ends[3];
  bool version = it->version && *it->version;
  // One word each, the same length, never shortened; a PC game is a fan game. Too narrow for the system and the kind
  // side by side, the kind leads the next line: then that line is taken, and the name keeps to two (a third ran off
  // the tile's foot).
  const char *kind = sys == BOXART_PC ? "FAN GAME" : it->hack ? "ROMHACK" : "ORIGINAL";
  uint32_t kindColor = sys == BOXART_PC ? 0x7A55B8 : it->hack ? 0xD9822B : 0x2E9E6A;
  bool kindLater = !it->tag && x + 4 + pd_text_width(sn) + 6 + 3 + pd_text_width(kind) + 6 > x + g->tileW - 4;
  int lines = wrap_name(name, g->tileW - 8, version || kindLater ? NAME_LINES : NAME_LINES + 1, starts, ends);
  for (int l = 0; l < lines; l++) {
    char line[256];
    snprintf(line, sizeof line, "%.*s", ends[l] - starts[l], name + starts[l]);
    pd_text_fit(c, x + 4, ty - 3 + l * LINE_H, g->tileW - 8, line, p->text, PD_NO_SHADOW);
  }
  ty += NAME_LINES * LINE_H + 3;
  if (lines > NAME_LINES) // the third line sits where the tags were; they move down a line (no version here)
    ty += LINE_H;
  if (it->tag) { // the picker's tile: its source, and its size on the version's line
    tag(c, x + 4, ty, it->tag, it->tagColor);
    ty += TAG_H + 2;
    if (version)
      pd_text_fit(c, x + 4, ty - 4, g->tileW - 8, it->version, p->muted, PD_NO_SHADOW);
    return;
  }
  int tx = x + 4;
  tx += tag(c, tx, ty, sn, sc) + 3;
  if (!kindLater)
    tag(c, tx, ty, kind, kindColor);
  // How well the companion supports it, at the right of the version's line: FULL, PARTLY or PLAY ONLY. With the kind
  // moved down a line, the support tag takes the system's line instead (never over the kind).
  static const char *const SUPPORT[] = {NULL, "FULL", "PARTLY", "PLAY ONLY"};
  static const uint32_t SUPPORT_COLORS[] = {0, 0x1F8F9E, 0xB7791F, 0x5B6270};
  int right = x + g->tileW - 4;
  if (it->support > 0 && it->support <= 3) {
    int w = pd_text_width(SUPPORT[it->support]) + 6;
    if (kindLater && right - w >= tx)
      tag(c, right - w, ty, SUPPORT[it->support], SUPPORT_COLORS[it->support]);
    else if (!kindLater && right - w >= x + 4) {
      tag(c, right - w, ty + TAG_H + 1, SUPPORT[it->support], SUPPORT_COLORS[it->support]);
      right -= w + 4;
    }
  }
  ty += TAG_H + 2;
  int left = x + 4;
  if (kindLater)
    left += tag(c, left, ty - 1, kind, kindColor) + 3;
  if (version && right - left > 12)
    pd_text_fit(c, left, ty - 4, right - left, it->version, p->muted, PD_NO_SHADOW);
}

/* The app's daisy (its launcher icon, app/src/main/res/mipmap-xxxhdpi/ic_launcher_foreground.png), redrawn 16 x 16 for the
 * bar: canvas pixels, so the window's scaling keeps it crisp at every UI SIZE. '.' is clear. */
static const char *const LOGO[16] = {
    ".......aa.......",
    "..a...abca...a..",
    ".aba..adda..aca.",
    "abdda.adda.addca",
    "adddcaaddaabddda",
    ".acdcaaaaaabdba.",
    "aaaaaafeefaaaaaa",
    "abbdcafggfabdcca",
    "ahiihafggfahiiha",
    "aaaaaaffffaaaaaa",
    ".aiiiaaaaaaiiia.",
    "aiiiihaiiahiiiia",
    "aiiihaiiiiahiiia",
    ".ahhaaiiiiaahha.",
    "..aa..ahha..aa..",
    ".......aa.......",
};
static uint32_t logo_color(char ch) {
  switch (ch) {
  case 'a': return 0x202020; // outline
  case 'b': return 0xFF9080; // red petals: light,
  case 'c': return 0xB01C14; // shade,
  case 'd': return 0xE83028; // red
  case 'e': return 0xFFF0A0; // the middle: light,
  case 'f': return 0xF8D030; // yellow,
  case 'g': return 0xFFF8D8; // its spot
  case 'h': return 0xC4C8D4; // white petals: shade,
  default: return 0xFFFFFF;  // white
  }
}

/* The logo at (x, y); in the dark looks a light rim, so its dark outline reads on the dark (and OLED black) bar. Its
 * width. */
static int draw_logo(struct pd_canvas *c, int x, int y) {
  for (int r = 0; pd_dark && r < 16; r++)
    for (int col = 0; col < 16; col++)
      if (LOGO[r][col] == 'a') {
        static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0; k < 4; k++) {
          int rr = r + d[k][1], cc = col + d[k][0];
          if (rr < 0 || rr > 15 || cc < 0 || cc > 15 || LOGO[rr][cc] == '.')
            pd_fill(c, x + cc, y + rr, 1, 1, 0x5A6070);
        }
      }
  for (int r = 0; r < 16; r++)
    for (int col = 0; col < 16; col++)
      if (LOGO[r][col] != '.')
        pd_fill(c, x + col, y + r, 1, 1, logo_color(LOGO[r][col]));
  return 16;
}

/* DARK MODE's switch, 12 x 12: a crescent moon or a sun. '#' is the ink. */
static const char *const MOON[12] = {
    "....####....", "..####......", ".####.......", ".###........", "####........", "####........",
    "####........", "####.......#", ".####.....##", ".#########..", "..#######...", "....####....",
};
static const char *const SUN[12] = {
    ".....##.....", ".#...##...#.", "..#......#..", "....####....", "...######...", "##.######.##",
    "##.######.##", "...######...", "....####....", "..#......#..", ".#...##...#.", ".....##.....",
};
static void draw_icon(struct pd_canvas *c, int x, int y, bool moon, uint32_t ink) {
  const char *const *rows = moon ? MOON : SUN;
  for (int r = 0; r < 12; r++)
    for (int col = 0; col < 12; col++)
      if (rows[r][col] == '#')
        pd_fill(c, x + col, y + r, 1, 1, ink);
}

static void draw_bar(struct game_grid *g, struct pd_canvas *c, const struct palette *p) {
  pd_fill(c, 0, 0, c->w, BAR_H, p->bar);
  pd_fill(c, 0, BAR_H, c->w, 1, p->barEdge);
  int titleX = PAD + draw_logo(c, PAD, (BAR_H - 16) / 2) + 5;
  g->barY = 4;
  g->barH = BAR_H - 8;
  int right = c->w - PAD;
  for (int i = g->barCount - 1; i >= 0 && i < 8; i--) {
    bool icon = g->bar[i][0] == '\x01';
    int w = icon ? g->barH + 4 : pd_text_width(g->bar[i]) + 14;
    int x = right - w;
    bool on = g->focus == GAME_GRID_BAR && i == g->barSelected;
    pd_round_rect(c, x, g->barY, w, g->barH, 5, on ? RING : p->tileEdge);
    pd_round_rect(c, x + 1, g->barY + 1, w - 2, g->barH - 2, 4, on ? RING : p->tile);
    if (icon)
      draw_icon(c, x + (w - 12) / 2, g->barY + (g->barH - 12) / 2, !strcmp(g->bar[i] + 1, "MOON"),
                on ? 0xFFFFFF : pd_dark ? 0xF2D46A : 0x4A5568);
    else
      pd_text(c, x + 7, g->barY - 1, g->bar[i], on ? 0xFFFFFF : p->text, PD_NO_SHADOW);
    g->barX[i] = x;
    g->barW[i] = w;
    right = x - 6;
  }
  // The name after the buttons, in the room they leave (a narrow window or a big UI SIZE: left out, never under them).
  pd_clip(c, 0, 0, 0, 0); // its width: drawn nowhere
  int titleW = pd_heading(c, 0, 0, g->title ? g->title : "", RING, PD_NO_SHADOW);
  pd_unclip(c);
  if (titleX + titleW <= right) { // whole or not at all
    pd_clip(c, titleX, 0, right - titleX, BAR_H);
    pd_heading(c, titleX, 5, g->title ? g->title : "", RING, PD_NO_SHADOW);
    pd_unclip(c);
  }
}

/* COLLECTIONS' tabs: a pill each, the picked one filled; "LB" before them and "RB" after. A row wider than the
 * window slides so the picked tab shows. */
static void draw_tabs(struct game_grid *g, struct pd_canvas *c, const struct palette *p) {
  g->tabY = BAR_H + 4;
  g->tabH = TAB_H - 4;
  pd_fill(c, 0, BAR_H + 1, c->w, TAB_H + 1, p->page);
  int n = g->tabCount < GAME_GRID_TABS_MAX ? g->tabCount : GAME_GRID_TABS_MAX;
  int keyW0 = g->tabKeys[0] ? pd_text_width(g->tabKeys[0]) + 8 : 0;
  int keyW1 = g->tabKeys[1] ? pd_text_width(g->tabKeys[1]) + 8 : 0;
  int widths[GAME_GRID_TABS_MAX], total = 0, before = 0;
  for (int i = 0; i < n; i++) {
    widths[i] = pd_text_width(g->tabs[i]) + 14;
    if (i < g->tabSelected)
      before += widths[i] + 4;
    total += widths[i] + 4;
  }
  int room = c->w - PAD * 2 - keyW0 - keyW1;
  int shift = 0;
  if (total > room) { // the picked tab's middle as near the middle as the row allows
    int sel = g->tabSelected >= 0 && g->tabSelected < n ? widths[g->tabSelected] : 0;
    shift = before + sel / 2 - room / 2;
    if (shift > total - room)
      shift = total - room;
    if (shift < 0)
      shift = 0;
  }
  int left = PAD + keyW0, x = left - shift;
  pd_clip(c, left, g->tabY, room, g->tabH);
  for (int i = 0; i < n; i++) {
    bool on = i == g->tabSelected;
    int w = widths[i];
    pd_round_rect(c, x, g->tabY, w, g->tabH, 5, on ? RING : p->tileEdge);
    pd_round_rect(c, x + 1, g->tabY + 1, w - 2, g->tabH - 2, 4, on ? RING : p->bar);
    pd_text(c, x + 7, g->tabY - 2, g->tabs[i], on ? 0xFFFFFF : p->text, PD_NO_SHADOW);
    g->tabX[i] = x < left ? left : x;
    g->tabW[i] = (x + w > left + room ? left + room : x + w) - g->tabX[i];
    x += w + 4;
  }
  pd_unclip(c);
  if (g->tabKeys[0])
    pd_text(c, PAD, g->tabY - 3, g->tabKeys[0], p->muted, PD_NO_SHADOW);
  if (g->tabKeys[1]) {
    int end = x - 4 < left + room ? x - 4 : left + room;
    pd_text(c, end + 6, g->tabY - 3, g->tabKeys[1], p->muted, PD_NO_SHADOW);
  }
}

void game_grid_draw(struct game_grid *g, struct pd_canvas *c) {
  struct palette p = colors();
  layout(g, c->w, c->h);
  if (g->selected >= g->count)
    g->selected = g->count ? g->count - 1 : 0;
  if (g->follow && g->count && g->focus == GAME_GRID_TILES) {
    int row = g->selected / g->columns;
    int top = row * (g->tileH + PAD), bottom = top + g->tileH + PAD;
    int view = g->gridBottom - g->gridTop;
    if (top < g->scrollY)
      g->scrollY = top;
    if (bottom > g->scrollY + view)
      g->scrollY = bottom - view;
  }
  if (g->follow && g->focus == GAME_GRID_BAR)
    g->scrollY = 0;
  g->follow = false;
  clamp_scroll(g);
  pd_fill(c, 0, 0, c->w, c->h, p.page);
  g->artCount = 0;
  pd_clip(c, 0, g->gridTop, c->w, g->gridBottom - g->gridTop);
  for (int i = 0; i < g->count; i++)
    draw_tile(g, c, i, &p);
  pd_unclip(c);
  if (!g->count && g->emptyTitle) {
    int y = g->gridTop + 30;
    pd_heading(c, PAD * 2, y, g->emptyTitle, p.text, PD_NO_SHADOW);
    if (g->emptyText)
      pd_text_wrap(c, PAD * 2, y + 22, c->w - PAD * 4, 16, 3, g->emptyText, p.muted, PD_NO_SHADOW);
  } else if (!g->count) {
    int y = g->gridTop + 30;
    pd_heading(c, PAD * 2, y, "NO GAMES YET", p.text, PD_NO_SHADOW);
    pd_text_wrap(c, PAD * 2, y + 22, c->w - PAD * 4, 16, 3,
                 "Pick GAME FOLDERS above to add the folders your games are in.", p.muted, PD_NO_SHADOW);
    if (g->emptyPath) {
      char line[300];
      snprintf(line, sizeof line, "Or put them in %s", g->emptyPath);
      pd_text_fit(c, PAD * 2, y + 58, c->w - PAD * 4, line, p.muted, PD_NO_SHADOW);
    }
  }
  // A scroll bar while there is more than fits.
  int m = max_scroll(g);
  if (m > 0) {
    int view = g->gridBottom - g->gridTop, content = view + m;
    int h = view * view / content;
    if (h < 12)
      h = 12;
    int y = g->gridTop + (view - h) * g->scrollY / m;
    pd_round_rect(c, c->w - 4, y, 3, h, 1, p.muted);
  }
  draw_bar(g, c, &p);
  if (g->tabs && g->tabCount > 1)
    draw_tabs(g, c, &p);
  if (g->hint) {
    pd_fill(c, 0, c->h - HINT_H, c->w, HINT_H, p.bar);
    pd_text_fit(c, PAD, c->h - HINT_H - 2, c->w - PAD * 2, g->hint, p.muted, PD_NO_SHADOW);
  }
}

void game_grid_move(struct game_grid *g, int dx, int dy) {
  g->follow = true;
  if (g->focus == GAME_GRID_BAR) {
    if (dx)
      g->barSelected = (g->barSelected + dx + g->barCount) % (g->barCount ? g->barCount : 1);
    if (dy > 0 && g->count)
      g->focus = GAME_GRID_TILES;
    return;
  }
  int cols = g->columns > 0 ? g->columns : 1;
  if (!g->count) {
    if (dy < 0 && g->barCount)
      g->focus = GAME_GRID_BAR;
    return;
  }
  if (dx) {
    int next = g->selected + dx;
    if (next >= 0 && next < g->count)
      g->selected = next;
  }
  if (dy < 0) {
    if (g->selected < cols) {
      if (g->barCount)
        g->focus = GAME_GRID_BAR;
    } else
      g->selected -= cols;
  }
  if (dy > 0) {
    if (g->selected + cols < g->count)
      g->selected += cols;
    else if (g->selected / cols < (g->count - 1) / cols)
      g->selected = g->count - 1; // the last row is short: its last game
  }
}

void game_grid_scroll(struct game_grid *g, int dy) {
  g->scrollY += dy;
  clamp_scroll(g);
}

int game_grid_tile_at(const struct game_grid *g, int x, int y) {
  if (y < g->gridTop || y >= g->gridBottom || g->columns <= 0)
    return -1;
  for (int i = 0; i < g->count; i++) {
    int tx, ty;
    tile_rect(g, i, &tx, &ty);
    if (x >= tx && x < tx + g->tileW && y >= ty && y < ty + g->tileH)
      return i;
  }
  return -1;
}

static int tab_at(const struct game_grid *g, int x, int y) {
  if (!g->tabs || g->tabCount < 2 || y < g->tabY || y >= g->tabY + g->tabH)
    return -1;
  for (int i = 0; i < g->tabCount && i < GAME_GRID_TABS_MAX; i++)
    if (g->tabW[i] > 0 && x >= g->tabX[i] && x < g->tabX[i] + g->tabW[i])
      return i;
  return -1;
}

static int bar_at(const struct game_grid *g, int x, int y) {
  if (y < 0 || y >= BAR_H)
    return -1;
  for (int i = 0; i < g->barCount && i < 8; i++)
    if (x >= g->barX[i] && x < g->barX[i] + g->barW[i])
      return i;
  return -1;
}

int game_grid_touch(struct game_grid *g, int x, int y, bool down, int *bar) {
  *bar = -1;
  if (down && !g->touching) {
    g->touching = true;
    g->dragged = false;
    g->touchX = x;
    g->touchY = g->lastY = y;
    g->pressedTile = game_grid_tile_at(g, x, y);
    g->pressedBar = bar_at(g, x, y);
    g->pressedTab = tab_at(g, x, y);
    return -1;
  }
  if (down) {
    if (!g->dragged && (y - g->touchY > 6 || g->touchY - y > 6 || x - g->touchX > 6 || g->touchX - x > 6))
      g->dragged = true;
    if (g->dragged && g->pressedBar < 0)
      game_grid_scroll(g, g->lastY - y);
    g->lastY = y;
    return -1;
  }
  if (!g->touching)
    return -1;
  g->touching = false;
  if (g->dragged)
    return -1;
  int tb = tab_at(g, x, y);
  if (tb >= 0 && tb == g->pressedTab) {
    g->tappedTab = tb;
    return -1;
  }
  int b = bar_at(g, x, y);
  if (b >= 0 && b == g->pressedBar) {
    *bar = b;
    return -1;
  }
  int t = game_grid_tile_at(g, x, y);
  if (t >= 0 && t == g->pressedTile) {
    g->selected = t;
    g->focus = GAME_GRID_TILES;
    return t;
  }
  return -1;
}
