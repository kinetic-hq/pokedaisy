#pragma once
/* BACKGROUND (SETTINGS > DISPLAY OPTIONS, and the DS settings): what the screen round the game and its wings is.
 * BLACK is true black round the game and the wing cards, for OLED screens; DARK a near black; THEME the old look
 * (the wings on the theme's dark slate). Unset, it follows COLORS: OLED is BLACK, the rest THEME.
 *
 * On BLACK and DARK the wings are cards: a near-black surface with a faint 1 px edge, so they don't read as black on
 * black, and their text a little off white. Against burn-in, a wing that sits still dims after a minute without
 * input or change, and its content moves a pixel or two every few minutes (never the game). A thin dark grey gutter
 * round the game keeps a bright game edge off pure black. Pure, so the tests (test_backdrop.c) check it as is. */
#include <stdbool.h>
#include <stdint.h>

enum pocket_background { POCKET_BACKGROUND_BLACK, POCKET_BACKGROUND_DARK, POCKET_BACKGROUND_THEME, POCKET_BACKGROUNDS };
#define POCKET_BACKGROUND_AUTO (-1)
#define POCKET_WING_IDLE_MS 60000u   // a still wing dims after this
#define POCKET_WING_SHIFT_MS 180000u // and moves a step this often

struct pocket_backdrop {
  uint32_t outside;    // round the game and the cards (0xRRGGBB)
  uint32_t card;       // the wings' surface
  uint32_t border;     // the cards' 1 px edge (drawn over the card at borderAlpha)
  uint8_t borderAlpha; // 0: no edge
  uint32_t gutter;     // the thin line round the game
  bool cards;          // wings as cards (BLACK / DARK); THEME fills each wing
};

static inline const char *pocket_background_name(int background) {
  return background == POCKET_BACKGROUND_BLACK ? "BLACK" : background == POCKET_BACKGROUND_DARK ? "DARK" : "THEME";
}

/* The background in force: the one picked, else COLORS' (oled: COLORS is OLED). */
static inline int pocket_background_resolve(int background, bool oled) {
  if (background >= 0 && background < POCKET_BACKGROUNDS)
    return background;
  return oled ? POCKET_BACKGROUND_BLACK : POCKET_BACKGROUND_THEME;
}

static inline struct pocket_backdrop pocket_backdrop_for(int background) {
  struct pocket_backdrop b;
  if (background == POCKET_BACKGROUND_BLACK) {
    b.outside = 0x000000;
    b.card = 0x111216;
    b.border = 0xffffff;
    b.borderAlpha = 26;
    b.gutter = 0x1c1d21;
    b.cards = true;
  } else if (background == POCKET_BACKGROUND_DARK) {
    b.outside = 0x0b0c0e;
    b.card = 0x16171b;
    b.border = 0xffffff;
    b.borderAlpha = 30;
    b.gutter = 0x222328;
    b.cards = true;
  } else {
    b.outside = 0x000000;
    b.card = 0x101418;
    b.border = 0;
    b.borderAlpha = 0;
    b.gutter = 0;
    b.cards = false;
  }
  return b;
}

/* The wings' texture colour mod (255 = as drawn): off white text on the cards, DIM COMPANION darker, and a still
 * wing eased down over two seconds once it has been idle a minute. */
static inline uint8_t pocket_wing_shade(int background, bool dim, uint32_t idleMs) {
  int v = background == POCKET_BACKGROUND_THEME ? 255 : 230;
  if (dim)
    v = v * 150 / 255;
  if (idleMs > POCKET_WING_IDLE_MS) {
    uint32_t into = idleMs - POCKET_WING_IDLE_MS;
    int down = into >= 2000 ? 40 : (int)(into * 40 / 2000); // to 60%
    v = v * (100 - down) / 100;
  }
  return (uint8_t)v;
}

/* The wings' burn-in shift at now: a slow walk round a square of step px (1 or 2 screen pixels), (0, 0) first. */
static inline void pocket_wing_shift(int background, uint32_t nowMs, int step, int *dx, int *dy) {
  static const signed char WALK[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  int i = background == POCKET_BACKGROUND_THEME ? 0 : (int)(nowMs / POCKET_WING_SHIFT_MS % 4);
  if (step < 1)
    step = 1;
  if (step > 2)
    step = 2;
  *dx = WALK[i][0] * step;
  *dy = WALK[i][1] * step;
}
