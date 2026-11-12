/* GPL-3.0. EXPAND's camera (wide_camera.h): its zoom levels, the app's scene rules and its eased motions. */
#include "wide_camera.h"
#include <assert.h>
#include <stdio.h>

static float run(struct wide_camera *c, int scene, bool autoZoom, const float *l, int n, uint32_t *now, uint32_t ms) {
  float s = 0;
  for (uint32_t t = 0; t <= ms; t += 16) {
    *now += 16;
    s = wide_camera_frame(c, scene, autoZoom, l, n, *now);
  }
  return s;
}

int main(void) {
  float l[WIDE_CAMERA_LEVELS];
  // 1080p, a GBA game: 6x, 5x, 4x, 3x (each at least 1.2 times the last); 2x would pass the 800 px window.
  int n = wide_camera_levels(1920, 1080, 240, 160, 800, 800, l);
  assert(n == 4 && l[0] == 6 && l[1] == 5 && l[2] == 4 && l[3] == 3);
  // 720p: 4x, 3x, 2x.
  n = wide_camera_levels(1280, 720, 240, 160, 800, 800, l);
  assert(n == 3 && l[0] == 4 && l[1] == 3 && l[2] == 2);
  // A screen smaller than the game: one level, 1x.
  assert(wide_camera_levels(200, 100, 240, 160, 800, 800, l) == 1 && l[0] == 1);

  n = wide_camera_levels(1920, 1080, 240, 160, 800, 800, l);
  struct wide_camera c = {0};
  c.level = 1;
  uint32_t now = 1;
  // The first frame jumps to its target, no motion; on the field after the return delay: the level picked.
  assert(run(&c, WIDE_SCENE_FIELD, true, l, n, &now, 0) == 6);
  assert(run(&c, WIDE_SCENE_FIELD, true, l, n, &now, 1300) == 5);
  // A text box: in with AUTO ZOOM IN, out without.
  assert(run(&c, WIDE_SCENE_TEXT, true, l, n, &now, 600) == 6);
  assert(run(&c, WIDE_SCENE_FIELD, true, l, n, &now, 300) == 6); // the return delay holds it
  assert(run(&c, WIDE_SCENE_FIELD, true, l, n, &now, 1000) == 5);
  assert(run(&c, WIDE_SCENE_TEXT, false, l, n, &now, 600) == 5);
  // Menus and battles always pull in, whatever AUTO ZOOM IN says; midway it's between the two, eased.
  float mid = run(&c, WIDE_SCENE_MENU, false, l, n, &now, 200);
  assert(mid > 5 && mid < 6);
  assert(run(&c, WIDE_SCENE_MENU, false, l, n, &now, 400) == 6);
  // A fade or a picture over the field decides nothing: the close view stays, then still nothing changes.
  assert(run(&c, WIDE_SCENE_FADE, true, l, n, &now, 2000) == 6);
  assert(run(&c, WIDE_SCENE_FIELD, true, l, n, &now, 1300) == 5);
  assert(run(&c, WIDE_SCENE_OVERLAY, true, l, n, &now, 2000) == 5);
  assert(run(&c, WIDE_SCENE_BATTLE_START, true, l, n, &now, 600) == 6);
  // ZOOM OUT / IN step the level; it's kept within the levels.
  c.level = 9;
  assert(run(&c, WIDE_SCENE_FIELD, true, l, n, &now, 1300) == 3);
  // Windows: whole scales fill the screen with whole pixels, even margins around the game.
  unsigned w, h;
  wide_camera_window(1920, 1080, 240, 160, 800, 800, 6, &w, &h);
  assert(w == 320 && h == 180);
  wide_camera_window(1920, 1080, 240, 160, 800, 800, 4.5f, &w, &h);
  assert(w >= 1920 / 4.5f - 1 && (w - 240) % 2 == 0 && (h - 160) % 2 == 0);
  wide_camera_window(1920, 1080, 240, 160, 800, 800, 1, &w, &h);
  assert(w == 800 && h == 800);
  puts("PASS wide camera");
  return 0;
}
