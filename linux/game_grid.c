/* GPL-3.0, see ../NOTICE. The game list's grid (game_grid.h). */
#include "game_grid.h"
#include "boxart.h"
#include "pd_style.h"

#include <stdio.h>
#include <string.h>

#define PAD 8
#define BAR_H 26
#define HINT_H 16
#define TARGET_TILE 96 // tiles are about this wide; the columns follow the canvas
#define TAG_H 12
#define RING 0xFF8A1Eu // the app's focus ring (AppColors.focusRing)

struct palette {
  uint32_t page, bar, barEdge, tile, tileEdge, tileOn, well, text, muted;
};
static struct palette colors(void) {
  if (pd_dark)
    return (struct palette){0x111318, 0x0A0B0E, 0x23262E, 0x1C1F26, 0x2A2E37, 0x272B35, 0x0D0F13, 0xF2F4F8, 0x8B93A1};
  return (struct palette){0xE9ECF1, 0xFFFFFF, 0xD3D8E0, 0xFFFFFF, 0xD3D8E0, 0xFFFFFF, 0xDDE1E8, 0x1E2229, 0x6B7280};
}

static const char *const SYSTEM_NAMES[] = {"GBA", "GBC", "GB", "NDS"};
static const uint32_t SYSTEM_COLORS[] = {0x5A6FD8, 0x8E5BD0, 0x6E9B3A, 0x6B7280};

void game_grid_init(struct game_grid *g, const struct game_grid_item *items, int count) {
  memset(g, 0, sizeof *g);
  g->items = items;
  g->count = count;
  g->pressedTile = g->pressedBar = -1;
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
  int artH = (g->tileW - 8) * 3 / 4;
  g->tileH = 4 + artH + 4 + 16 + TAG_H + 2 + 12 + 4;
  g->gridTop = BAR_H + 6;
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
  // The art's well: the system's colour, darkened, with its name, until a picture covers it.
  int ax = x + 4, ay = y + 4, aw = g->tileW - 8, ah = (g->tileW - 8) * 3 / 4;
  int sys = it->system >= 0 && it->system <= BOXART_NDS ? it->system : 0;
  uint32_t sc = SYSTEM_COLORS[sys];
  uint32_t well = pd_dark ? ((sc >> 2) & 0x3F3F3F) + 0x0A0A0A : (sc & 0xFEFEFE) / 2 + 0x7F7F7F;
  pd_round_rect(c, ax, ay, aw, ah, 4, well);
  const char *sn = SYSTEM_NAMES[sys];
  pd_heading(c, ax + (aw - pd_text_width(sn)) / 2, ay + ah / 2 - 9, sn, pd_dark ? 0x9AA3B2 : 0xFFFFFF, PD_NO_SHADOW);
  if (g->artCount < GAME_GRID_ART_MAX) {
    struct game_grid_art *a = &g->art[g->artCount++];
    a->x = ax, a->y = ay, a->w = aw, a->h = ah, a->index = i;
    a->clipY = g->gridTop;
    a->clipH = g->gridBottom - g->gridTop;
  }
  int ty = ay + ah + 3;
  // Every game here is a Pokémon game: the tile shows the rest of the name, which then fits.
  const char *name = it->name ? it->name : "";
  if (!strncmp(name, "Pok\xC3\xA9mon ", 9) && name[9])
    name += 9;
  pd_text_fit(c, x + 4, ty - 3, g->tileW - 8, name, p->text, PD_NO_SHADOW);
  ty += 15;
  int tx = x + 4;
  tx += tag(c, tx, ty, sn, sc) + 3;
  const char *kind = it->hack ? "ROM HACK" : "ORIGINAL";
  if (tx + pd_text_width(kind) + 6 <= x + g->tileW - 4)
    tag(c, tx, ty, kind, it->hack ? 0xD9822B : 0x2E9E6A);
  ty += TAG_H + 2;
  if (it->version && *it->version)
    pd_text_fit(c, x + 4, ty - 4, g->tileW - 8, it->version, p->muted, PD_NO_SHADOW);
}

static void draw_bar(struct game_grid *g, struct pd_canvas *c, const struct palette *p) {
  pd_fill(c, 0, 0, c->w, BAR_H, p->bar);
  pd_fill(c, 0, BAR_H, c->w, 1, p->barEdge);
  pd_heading(c, PAD, 5, g->title ? g->title : "", RING, PD_NO_SHADOW);
  g->barY = 4;
  g->barH = BAR_H - 8;
  int right = c->w - PAD;
  for (int i = g->barCount - 1; i >= 0 && i < 8; i--) {
    int w = pd_text_width(g->bar[i]) + 14;
    int x = right - w;
    bool on = g->focus == GAME_GRID_BAR && i == g->barSelected;
    pd_round_rect(c, x, g->barY, w, g->barH, 5, on ? RING : p->tileEdge);
    pd_round_rect(c, x + 1, g->barY + 1, w - 2, g->barH - 2, 4, on ? RING : p->tile);
    pd_text(c, x + 7, g->barY - 1, g->bar[i], on ? 0xFFFFFF : p->text, PD_NO_SHADOW);
    g->barX[i] = x;
    g->barW[i] = w;
    right = x - 6;
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
  if (!g->count) {
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
