#pragma once
/* BACKGROUND (SETTINGS > DISPLAY OPTIONS, and the DS settings): what the screen round the game and its wings is.
 * BLACK is true black round the game and the wing cards, for OLED screens; DARK a near black; THEME the old look
 * (the wings on the theme's dark slate). Unset, it follows COLORS and DARK MODE: OLED is BLACK, DARK MODE DARK,
 * the rest THEME.
 *
 * On BLACK and DARK the wings are cards: a near-black surface with a faint 1 px edge, so they don't read as black on
 * black, and their text a little off white. Against burn-in, a wing that sits still dims after a minute without
 * input or change, and its content moves a pixel or two every few minutes (never the game). A thin dark grey gutter
 * round the game keeps a bright game edge off pure black. BLUR is DARK's cards on the game's own picture, blurred
 * and darkened, round them (the host draws it: `blur`). Pure, so the tests (test_backdrop.c) check it as is.
 *
 * WING LOOK (pocket_wing_look_scale): COMPACT draws the wings a screen pixel per font pixel smaller (more fits),
 * COMFY as worked out, BIG a step larger (fewer things, easier to read across a room). */
#include <stdbool.h>
#include <stdint.h>

enum pocket_background {
  POCKET_BACKGROUND_BLACK,
  POCKET_BACKGROUND_DARK,
  POCKET_BACKGROUND_THEME,
  POCKET_BACKGROUND_BLUR, // appended: settings files keep their numbers
  POCKET_BACKGROUNDS
};
enum pocket_wing_size { POCKET_WING_COMPACT, POCKET_WING_COMFY, POCKET_WING_BIG, POCKET_WING_LOOKS };
#define POCKET_WING_MIN_W 100 // the narrowest wing canvas BIG goes to
#define POCKET_WING_MIN_H 220
#define POCKET_BACKGROUND_AUTO (-1)
#define POCKET_WING_IDLE_MS 60000u   // a still wing dims after this
#define POCKET_WING_SHIFT_MS 180000u // and moves a step this often

struct pocket_backdrop {
  uint32_t outside;    // round the game and the cards (0xRRGGBB)
  uint32_t card;       // the wings' surface
  uint32_t border;     // the cards' 1 px edge (drawn over the card at borderAlpha)
  uint8_t borderAlpha; // 0: no edge
  uint32_t gutter;     // the thin line round the game
  bool cards;          // wings as cards (BLACK / DARK / BLUR); THEME fills each wing
  bool blur;           // the game's picture, blurred and dark, round the cards (BLUR)
};

static inline const char *pocket_wing_look_name(int look) {
  return look == POCKET_WING_COMPACT ? "COMPACT" : look == POCKET_WING_BIG ? "BIG" : "COMFY";
}

/* The wings' scale (screen pixels a canvas pixel) for WING LOOK, from COMFY's: never below 1, never a canvas wider
 * than maxW, and BIG never a canvas under POCKET_WING_MIN_W x POCKET_WING_MIN_H. */
static inline int pocket_wing_look_scale(int scale, int look, int roomW, int roomH, int maxW, int maxH) {
  if (look == POCKET_WING_COMPACT && scale > 1 && roomW / (scale - 1) <= maxW && roomH / (scale - 1) <= maxH)
    return scale - 1;
  if (look == POCKET_WING_BIG && roomW / (scale + 1) >= POCKET_WING_MIN_W && roomH / (scale + 1) >= POCKET_WING_MIN_H)
    return scale + 1;
  return scale < 1 ? 1 : scale;
}

static inline const char *pocket_background_name(int background) {
  return background == POCKET_BACKGROUND_BLACK  ? "BLACK"
         : background == POCKET_BACKGROUND_DARK ? "DARK"
         : background == POCKET_BACKGROUND_BLUR ? "BLUR"
                                                : "THEME";
}

/* The background in force: the one picked, else COLORS' (oled: COLORS is OLED), else DARK MODE's (dark). */
static inline int pocket_background_resolve(int background, bool oled, bool dark) {
  if (background >= 0 && background < POCKET_BACKGROUNDS)
    return background;
  return oled ? POCKET_BACKGROUND_BLACK : dark ? POCKET_BACKGROUND_DARK : POCKET_BACKGROUND_THEME;
}

static inline struct pocket_backdrop pocket_backdrop_for(int background) {
  struct pocket_backdrop b;
  b.blur = background == POCKET_BACKGROUND_BLUR;
  if (background == POCKET_BACKGROUND_BLACK) {
    b.outside = 0x000000;
    b.card = 0x111216;
    b.border = 0xffffff;
    b.borderAlpha = 26;
    b.gutter = 0x1c1d21;
    b.cards = true;
  } else if (background == POCKET_BACKGROUND_DARK || background == POCKET_BACKGROUND_BLUR) {
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
