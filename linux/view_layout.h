/* COMPANION VIEW: where the game, the companion and the side panels go on a
 * 16:9 / 16:10 screen. Pure arithmetic, no SDL, tested by test_view_layout.c.
 * GPL-3.0, see ../NOTICE.
 *
 * Every rectangle is in screen pixels. The game's picture always sits at a
 * whole-number scale and stays centred, except in SIDE PANEL locked, where it
 * centres in the left half. */
#ifndef POCKET_VIEW_LAYOUT_H
#define POCKET_VIEW_LAYOUT_H
#include <stdbool.h>

/* In enum pd_companion_view's order (SETTINGS > COMPANION VIEW), main.c checks. */
enum pocket_view {
  POCKET_VIEW_BATTLE_WINGS, // walking: EXPAND full screen; battle: wings; companion: a window
  POCKET_VIEW_WINDOW,       // the companion as a centred window over the dimmed game
  POCKET_VIEW_SIDE_PANEL,   // the companion slides in from the right; locked, it keeps the right half
  POCKET_VIEW_WINGS,        // party left, map or battle right, all the time
  POCKET_VIEW_GLASS_DRAWER, // the companion slides up over the bottom 46%, see-through
  POCKET_VIEW_STEP_BACK,    // open: the game two whole scales smaller, the companion around it
  POCKET_VIEWS,
};

struct view_rect {
  int x, y, w, h;
};

/* What the host knows this frame. */
struct view_state {
  int screenW, screenH; // the window's pixels
  int frameW, frameH;   // the game's own picture: 240x160 (GBA), 160x144 (GB)
  bool companionOpen;   // the companion has the buttons (the game waits)
  bool battle;          // a battle is on
  bool panelLocked;     // SIDE PANEL: the panel stays beside the game
  int wingRoom;         // WING ROOM: VIEW_WING_ROOM_* (0, GAME FIRST, is the old layout)
  bool stretch;         // STRETCH: the game may lose its shape by up to VIEW_STRETCH_PERCENT to fill its room
};

struct view_layout {
  struct view_rect game; // the game's own picture (EXPAND's window, when on, is centred on it at `scale`)
  int scale;             // its whole-number scale
  bool expand;           // EXPAND may fill the screen around it (when the game supports it)
  bool hud;              // the overworld HUD may show
  bool dim;              // dim the game (a companion over it)
  struct view_rect left, right;     // the side panels: party / battle or map (w = 0: none)
  struct view_rect top, bottom;     // the info strips above and below the game (h = 0: none)
  struct view_rect companion;       // the full companion (w = 0: closed)
  bool companionGlass;              // drawn see-through (GLASS DRAWER)
  bool companionSlides;             // enters from the right (SIDE PANEL) or the bottom (GLASS DRAWER)
};

/* The share of the screen's width each wing keeps at least (BATTLE WINGS, WINGS). */
#define VIEW_WING_MIN_PERCENT 12
// WING ROOM: each wing's least share of the width, per mille. GAME FIRST keeps the old 12%; BALANCED about 240 px at
// 1920 wide (GBA 6x on 1080p and 1200p); WINGS FIRST about 320 px there.
enum { VIEW_WING_ROOM_GAME_FIRST, VIEW_WING_ROOM_BALANCED, VIEW_WING_ROOM_WINGS_FIRST, VIEW_WING_ROOMS };
#define VIEW_STRETCH_PERCENT 5
int view_wing_room_permille(int room);
/* Strips go above and below the game only when that many percent of the height is free each side. */
#define VIEW_STRIP_MIN_PERCENT 8
#define VIEW_STRIP_MIN_PX 48 // or this much room, whichever is less: a 16:9 screen's ~5% is enough for a bar
/* GLASS DRAWER's sheet: this share of the screen's height, from the bottom. */
#define VIEW_DRAWER_PERCENT 46
/* How long a size change animates, in milliseconds. */
#define VIEW_ANIMATION_MS 400

/* The largest whole scale for a frameW x frameH game in w x h; at least 1. */
int view_fit_scale(int w, int h, int frameW, int frameH);
/* The largest whole scale leaving at least minPercent of the width free on each side. */
int view_wing_scale(int screenW, int screenH, int frameW, int frameH, int minPercent);
/* The layout of VIEW for STATE. */
void view_layout(int view, const struct view_state *state, struct view_layout *out);
/* A rectangle t of the way (0..1000) from A to B, eased in and out. */
struct view_rect view_rect_lerp(struct view_rect a, struct view_rect b, int t);
/* The whole layout t of the way (0..1000) from A to B (rectangles only; flags are B's). */
void view_layout_lerp(const struct view_layout *a, const struct view_layout *b, int t, struct view_layout *out);
#endif
