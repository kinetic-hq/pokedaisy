/* GPL-3.0, see ../NOTICE. The game list as a grid of box art (the landing page): a slim bar along the top (the
 * program's name, GAME FOLDERS, SETTINGS), then a tile a game - its art, its name, two tags (the system; ORIGINAL or
 * ROMHACK), its version and how well the companion supports it - driven by the D-pad, touch, the mouse and its wheel. Drawn on the canvas; the art itself
 * is left to the host (art[]: where each visible tile's picture goes), so it stays sharp at the screen's resolution. */
#ifndef POCKET_GAME_GRID_H
#define POCKET_GAME_GRID_H
#include <stdbool.h>

#include "pd_canvas.h"

#define GAME_GRID_ART_MAX 128

enum { GAME_GRID_TILES, GAME_GRID_BAR };
/* A bar entry drawn as an icon instead of words: DARK MODE's switch (a moon while dark, a sun while light). */
#define GAME_GRID_ICON_MOON "\x01MOON"
#define GAME_GRID_ICON_SUN "\x01SUN"

struct game_grid_item {
  const char *name;    // what the list calls it
  const char *version; // "v4.5.3", or "" / NULL
  int system;          // enum boxart_system
  bool hack;           // a ROM hack (else ORIGINAL)
  bool banner;         // the art is the base game's box: the name goes on a banner across its foot
  int support;         // game_support.h's GAME_SUPPORT_*: FULL / PARTLY / PLAY ONLY, beside the version (0: not known yet)
  const char *tag;     // BOX ART's picker: one tag of its own (the cover's source) in place of the system and kind
  uint32_t tagColor;
};

struct game_grid_art {
  int x, y, w, h; // in canvas pixels, clipped to the grid
  int clipY, clipH;
  int index;
};

struct game_grid {
  const struct game_grid_item *items;
  int count;
  int selected;
  int scrollY; // pixels
  int focus;   // GAME_GRID_TILES / GAME_GRID_BAR
  const char *const *bar;
  int barCount, barSelected;
  const char *title;     // the bar's name
  const char *hint;      // the line along the bottom (NULL: none)
  const char *emptyPath; // where games go, shown while there are none
  bool follow;           // keep the selected tile in view on the next draw
  // Worked out by game_grid_draw:
  int columns, tileW, tileH, gap, gridTop, gridBottom, drawnW, drawnH;
  struct game_grid_art art[GAME_GRID_ART_MAX];
  int artCount;
  int barX[8], barW[8], barY, barH;
  // Touch:
  bool touching, dragged;
  int touchX, touchY, lastY, pressedTile, pressedBar;
};

void game_grid_init(struct game_grid *g, const struct game_grid_item *items, int count);
void game_grid_draw(struct game_grid *g, struct pd_canvas *c);
/* The D-pad: dx / dy in tiles; UP off the top row goes to the bar, DOWN from it back. */
void game_grid_move(struct game_grid *g, int dx, int dy);
/* A mouse wheel or a drag: dy pixels. */
void game_grid_scroll(struct game_grid *g, int dy);
/* Stylus / mouse down / up at (x, y); returns the tile a tap picked (on release, not after a drag), else -1, and sets
 * *bar to a bar button tapped (else -1). */
int game_grid_touch(struct game_grid *g, int x, int y, bool down, int *bar);
/* The tile at (x, y), or -1 (a right click's game). */
int game_grid_tile_at(const struct game_grid *g, int x, int y);
#endif
