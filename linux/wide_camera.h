#pragma once
/* EXPAND's camera on the desktop host: the app's WideCamera (app/.../WideCamera.kt) without the look-around.
 * - Zoom levels: the game's whole scale that fits the screen first (1x, the closest), then the whole scales below it
 *   that each show at least WIDE_CAMERA_MIN_STEP more of the map and fit pk_wide's largest window. ZOOM IN / ZOOM OUT
 *   step through them; ASPECT ZOOM OUT starts one step out.
 * - Scenes (pk_wide.h's PK_WIDE_SCENE_*): the plain overworld shows the level picked; a text box pulls in to 1x only
 *   with AUTO ZOOM IN on; battles, menus and anything else off the map always pull in; a fade or a picture over the
 *   field (FireRed's map preview) decides nothing. The level picked comes back half a second after the close view.
 * - Every change is one timed ease-in-out in log space (the same feel at every step): a pull-in in 420 ms (550 ms into
 *   a battle), the way back out in 650 ms.
 * Integer scales only (the app's 1x fills the screen's height at any scale): every settled view has whole pixels. */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define WIDE_CAMERA_LEVELS 8
#define WIDE_CAMERA_MIN_STEP 1.2f
#define WIDE_CAMERA_PULL_IN_MS 420u
#define WIDE_CAMERA_PULL_IN_BATTLE_MS 550u
#define WIDE_CAMERA_ZOOM_OUT_MS 650u
#define WIDE_CAMERA_RETURN_DELAY_MS 500u

enum { WIDE_SCENE_FIELD, WIDE_SCENE_TEXT, WIDE_SCENE_OVERLAY, WIDE_SCENE_BATTLE_START, WIDE_SCENE_OFF,
       WIDE_SCENE_FADE, WIDE_SCENE_MENU }; // pk_wide.h's PK_WIDE_SCENE_*

struct wide_camera {
  int level;               // the step picked (0 = 1x)
  float scale;             // screen pixels per game pixel now (0 = not started)
  float from, target;      // the motion under way
  uint32_t start, length;  // its start and length, ms
  uint32_t closeOverSince; // when the last close view ended (0 = still close)
};

/* The scales for an ow x oh screen and a fw x fh game, closest first; returns how many (at least 1). */
static inline int wide_camera_levels(int ow, int oh, int fw, int fh, int maxW, int maxH, float *out) {
  int k = ow / fw < oh / fh ? ow / fw : oh / fh;
  if (k < 1)
    k = 1;
  int n = 0;
  out[n++] = (float)k;
  for (k--; k >= 1 && n < WIDE_CAMERA_LEVELS; k--) {
    if (ow / k > maxW || oh / k > maxH)
      break;
    if (out[n - 1] / (float)k >= WIDE_CAMERA_MIN_STEP)
      out[n++] = (float)k;
  }
  return n;
}

/* Back to the closest view at once (a new game, a state load). */
static inline void wide_camera_reset(struct wide_camera *c) {
  c->scale = 0;
  c->target = 0;
  c->closeOverSince = 0;
}

static inline float wide_camera_smoothstep(float p) { return p * p * (3 - 2 * p); }

/* Before a frame: the scale to draw, from the last frame's scene. now is in ms (never 0). */
static inline float wide_camera_frame(struct wide_camera *c, int scene, bool autoZoom, const float *levels, int n,
                                      uint32_t now) {
  if (n < 1)
    return 1;
  if (c->level >= n)
    c->level = n - 1;
  if (c->level < 0)
    c->level = 0;
  bool wantsClose = scene == WIDE_SCENE_FIELD ? false : scene == WIDE_SCENE_TEXT ? autoZoom : true;
  if (scene == WIDE_SCENE_FADE || scene == WIDE_SCENE_OVERLAY)
    ; // decides nothing
  else if (wantsClose)
    c->closeOverSince = 0;
  else if (!c->closeOverSince)
    c->closeOverSince = now;
  bool close = !c->closeOverSince || now - c->closeOverSince < WIDE_CAMERA_RETURN_DELAY_MS;
  float target = close ? levels[0] : levels[c->level];
  if (target != c->target) {
    c->from = c->scale <= 0 ? target : c->scale;
    c->target = target;
    c->start = now;
    c->length = c->scale <= 0                                           ? 0
                : target > c->from && scene == WIDE_SCENE_BATTLE_START ? WIDE_CAMERA_PULL_IN_BATTLE_MS
                : target > c->from                                     ? WIDE_CAMERA_PULL_IN_MS
                                                                       : WIDE_CAMERA_ZOOM_OUT_MS;
  }
  float p = c->length ? (float)(now - c->start) / (float)c->length : 1;
  if (p > 1)
    p = 1;
  c->scale = c->from * powf(c->target / c->from, wide_camera_smoothstep(p));
  if (fabsf(c->scale - c->target) < 0.002f)
    c->scale = c->target;
  return c->scale;
}

/* The window for scale s: the screen's size over it (whole pixels up), never smaller than the game nor larger than
 * pk_wide's largest, with even margins so the game's own pixels sit in the middle. */
static inline void wide_camera_window(int ow, int oh, int fw, int fh, int maxW, int maxH, float s, unsigned *w,
                                      unsigned *h) {
  bool whole = s == floorf(s);
  int x = whole ? ow / (int)s : (int)ceilf(ow / s), y = whole ? oh / (int)s : (int)ceilf(oh / s);
  x = x > maxW ? maxW : x < fw ? fw : x;
  y = y > maxH ? maxH : y < fh ? fh : y;
  *w = (unsigned)(x - ((x - fw) & 1));
  *h = (unsigned)(y - ((y - fh) & 1));
}
