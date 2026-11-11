/* COMPANION VIEW layouts on the handhelds' screens. GPL-3.0, see ../NOTICE. */
#include "view_layout.h"
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(c)                                                                                                       \
  do {                                                                                                                 \
    if (!(c)) {                                                                                                        \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                                          \
      failures++;                                                                                                      \
    }                                                                                                                  \
  } while (0)

static struct view_state gba(int w, int h) {
  struct view_state s = {w, h, 240, 160, false, false, false};
  return s;
}

static bool inside(struct view_rect r, int w, int h) {
  return r.x >= 0 && r.y >= 0 && r.x + r.w <= w && r.y + r.h <= h;
}

static bool centred(struct view_rect r, int w, int h) {
  int dx = r.x * 2 + r.w - w, dy = r.y * 2 + r.h - h;
  return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}

// Battle wings: the scale, centring, the 12% wings and the strips only where the rows are worth it.
static void battle_wings(int w, int h, int scale, bool strips) {
  struct view_state s = gba(w, h);
  s.battle = true;
  struct view_layout l;
  view_layout(POCKET_VIEW_BATTLE_WINGS, &s, &l);
  CHECK(l.scale == scale);
  CHECK(l.game.w == 240 * scale && l.game.h == 160 * scale);
  CHECK(centred(l.game, w, h));
  CHECK(l.left.w * 100 >= w * VIEW_WING_MIN_PERCENT && l.right.w * 100 >= w * VIEW_WING_MIN_PERCENT);
  CHECK(l.left.x == 0 && l.left.x + l.left.w == l.game.x && l.right.x == l.game.x + l.game.w);
  CHECK(l.right.x + l.right.w == w && l.left.h == h && l.right.h == h);
  CHECK((l.top.h > 0) == strips && (l.bottom.h > 0) == strips);
  if (strips)
    CHECK(l.top.y + l.top.h == l.game.y && l.bottom.y == l.game.y + l.game.h && l.bottom.y + l.bottom.h == h);
  CHECK(!l.expand && !l.dim && l.companion.w == 0);
  // One scale up wouldn't leave the wings their share.
  CHECK(240 * (scale + 1) > w - 2 * ((w * VIEW_WING_MIN_PERCENT + 99) / 100) || 160 * (scale + 1) > h);
  // Walking, the same view gives the game the whole screen.
  s.battle = false;
  view_layout(POCKET_VIEW_BATTLE_WINGS, &s, &l);
  CHECK(l.expand && l.hud && l.left.w == 0 && l.right.w == 0 && centred(l.game, w, h));
  CHECK(l.scale == view_fit_scale(w, h, 240, 160));
}

static void every_view(int w, int h) {
  for (int v = 0; v < POCKET_VIEWS; v++)
    for (int bits = 0; bits < 8; bits++) {
      struct view_state s = gba(w, h);
      s.companionOpen = bits & 1;
      s.battle = bits & 2;
      s.panelLocked = bits & 4;
      struct view_layout l;
      view_layout(v, &s, &l);
      CHECK(l.scale >= 1 && l.game.w == 240 * l.scale && l.game.h == 160 * l.scale);
      CHECK(inside(l.game, w, h) && inside(l.left, w, h) && inside(l.right, w, h));
      CHECK(inside(l.top, w, h) && inside(l.bottom, w, h) && inside(l.companion, w, h));
      bool lockedPanel = v == POCKET_VIEW_SIDE_PANEL && s.panelLocked;
      if (lockedPanel) {
        CHECK(l.game.x + l.game.w <= w / 2 && centred(l.game, w / 2, h));
        CHECK(l.companion.x == w / 2 && l.companion.w == w - w / 2 && !l.expand);
      } else {
        CHECK(centred(l.game, w, h));
      }
      // A companion opened over the game: a window over the dim, a sheet, a panel or the wings around it.
      if (s.companionOpen && !lockedPanel)
        CHECK(l.companion.w > 0 || (v == POCKET_VIEW_STEP_BACK && l.left.w > 0));
      if (!s.companionOpen && !lockedPanel)
        CHECK(l.companion.w == 0 && !l.dim);
      // Panels never cover the game.
      CHECK(l.left.w == 0 || l.left.x + l.left.w <= l.game.x);
      CHECK(l.right.w == 0 || l.right.x >= l.game.x + l.game.w);
    }
}

static void window_and_drawer(int w, int h) {
  struct view_state s = gba(w, h);
  s.companionOpen = true;
  struct view_layout l;
  view_layout(POCKET_VIEW_WINDOW, &s, &l);
  CHECK(l.dim && centred(l.companion, w, h) && l.companion.w * 3 == l.companion.h * 4);
  CHECK(l.companion.w * 100 <= w * 90 && l.companion.h * 100 <= h * 84 + 100);
  view_layout(POCKET_VIEW_GLASS_DRAWER, &s, &l);
  CHECK(l.companionGlass && l.companion.w == w && l.companion.y + l.companion.h == h);
  CHECK(l.companion.h == h * VIEW_DRAWER_PERCENT / 100);
  view_layout(POCKET_VIEW_STEP_BACK, &s, &l);
  CHECK(l.scale == view_fit_scale(w, h, 240, 160) - 2 && l.left.w > 0 && l.right.w > 0 && !l.expand);
}

static void animation(void) {
  struct view_rect a = {0, 0, 1920, 1080}, b = {240, 60, 1440, 960};
  struct view_rect r = view_rect_lerp(a, b, 0);
  CHECK(r.x == a.x && r.w == a.w);
  r = view_rect_lerp(a, b, 1000);
  CHECK(r.x == b.x && r.y == b.y && r.w == b.w && r.h == b.h);
  r = view_rect_lerp(a, b, 500);
  CHECK(r.w == (a.w + b.w) / 2);
  // Eased: a quarter of the time is less than a quarter of the way.
  r = view_rect_lerp(a, b, 250);
  CHECK(r.x - a.x < (b.x - a.x) / 4);
  // A panel appearing grows from its own middle.
  struct view_rect none = {0, 0, 0, 0}, panel = {0, 0, 240, 1080};
  r = view_rect_lerp(none, panel, 0);
  CHECK(r.w == 0 && r.x == 120);
  r = view_rect_lerp(none, panel, 1000);
  CHECK(r.w == 240 && r.x == 0);
  CHECK(VIEW_ANIMATION_MS == 400);
}

int main(void) {
  battle_wings(1920, 1080, 6, false); // ROG Ally: 1440x960, 60 rows over and under: no strips
  battle_wings(1920, 1200, 6, true);  // Legion Go 2, OneXPlayer: 120 rows each side
  battle_wings(2560, 1600, 8, true);  // Legion Go: 1920x1280
  battle_wings(1280, 800, 4, true);   // Steam Deck: 960x640
  const int screens[][2] = {{1920, 1080}, {1920, 1200}, {2560, 1600}, {1280, 800}};
  for (int i = 0; i < 4; i++) {
    every_view(screens[i][0], screens[i][1]);
    window_and_drawer(screens[i][0], screens[i][1]);
  }
  animation();
  CHECK(view_fit_scale(10, 10, 240, 160) == 1);
  if (failures) {
    fprintf(stderr, "FAIL view layout: %d\n", failures);
    return 1;
  }
  printf("PASS view layout: battle wings, window, side panel, wings, glass drawer and step back on 4 screens\n");
  return 0;
}
