#pragma once
/* EXPAND's camera on the desktop host: the app's WideCamera (app/.../WideCamera.kt).
 * - Zoom levels: the game's whole scale that fits the screen first (1x, the closest), then the whole scales below it
 *   that each show at least WIDE_CAMERA_MIN_STEP more of the map and fit pk_wide's largest window. ZOOM IN / ZOOM OUT
 *   step through them; ASPECT ZOOM OUT starts one step out.
 * - Scenes (pk_wide.h's PK_WIDE_SCENE_*): the plain overworld shows the level picked; a text box pulls in to 1x only
 *   with AUTO ZOOM IN on; battles, menus and anything else off the map always pull in; a fade or a picture over the
 *   field (FireRed's map preview) decides nothing. The level picked comes back half a second after the close view.
 * - Every change is one timed ease-in-out in log space (the same feel at every step): a pull-in in 420 ms (550 ms into
 *   a battle), the way back out in 650 ms.
 * - LOOK AROUND (the right stick, wide_camera_look): standing, it moves the view, which stays where it's left;
 *   walking glides it back to the player, or with the stick held follows the stick, the player kept on screen. A
 *   close view (a battle, a menu) brings it back to the player.
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
#define WIDE_CAMERA_LOOK_TAU 0.12f     // s: the view following the stick, or going back for a close view
#define WIDE_CAMERA_RETURN_TAU 0.18f   // s: the view gliding back to a player who starts walking
#define WIDE_CAMERA_LOOK_SPEED 150.0f  // game px a second at full stick, at 1x
#define WIDE_CAMERA_PLAYER_KEEP 28.0f  // game px: the player stays at least this far inside the view
#define WIDE_CAMERA_DEAD_ZONE 0.2f

enum { WIDE_SCENE_FIELD, WIDE_SCENE_TEXT, WIDE_SCENE_OVERLAY, WIDE_SCENE_BATTLE_START, WIDE_SCENE_OFF,
       WIDE_SCENE_FADE, WIDE_SCENE_MENU }; // pk_wide.h's PK_WIDE_SCENE_*

struct wide_camera {
  int level;               // the step picked (0 = 1x)
  float scale;             // screen pixels per game pixel now (0 = not started)
  float from, target;      // the motion under way
  uint32_t start, length;  // its start and length, ms
  uint32_t closeOverSince; // when the last close view ended (0 = still close)
  bool close;              // the last frame wanted the close view
  float lookX, lookY;      // LOOK AROUND: the view's centre off the game's own, game px
  uint32_t lookMs;         // the last look step's time (0 = none yet)
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
  c->lookX = c->lookY = 0;
  c->lookMs = 0;
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
  c->close = close;
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

static inline float wide_camera_dead(float v) {
  float a = fabsf(v);
  if (a < WIDE_CAMERA_DEAD_ZONE)
    return 0;
  if (a > 1)
    a = 1;
  return (a - WIDE_CAMERA_DEAD_ZONE) / (1 - WIDE_CAMERA_DEAD_ZONE) * (v < 0 ? -1 : 1);
}

static inline void wide_camera_spring(struct wide_camera *c, float tx, float ty, float dt, float tau) {
  float k = 1 - expf(-dt / tau);
  c->lookX += (tx - c->lookX) * k;
  c->lookY += (ty - c->lookY) * k;
  if (hypotf(c->lookX - tx, c->lookY - ty) < 0.05f)
    c->lookX = tx, c->lookY = ty;
}

/* After wide_camera_frame: LOOK AROUND's step, the right stick at (sx, sy) (-1..1, right and down positive), walking =
 * a direction held, an ow x oh screen, a fw x fh game, fit = the closest level's scale. now is in ms (never 0). */
static inline void wide_camera_look(struct wide_camera *c, float sx, float sy, bool walking, int ow, int oh, int fw,
                                    int fh, float fit, uint32_t now) {
  float dt = c->lookMs ? (float)(now - c->lookMs) / 1000.0f : 0;
  if (dt > 0.1f)
    dt = 0.1f;
  c->lookMs = now;
  float s = c->scale > 0 ? c->scale : 1, vw = (float)ow / s, vh = (float)oh / s;
  sx = wide_camera_dead(sx);
  sy = wide_camera_dead(sy);
  bool stick = sx != 0 || sy != 0;
  if (c->close) {
    wide_camera_spring(c, 0, 0, dt, WIDE_CAMERA_LOOK_TAU);
  } else if (walking && stick) {
    float maxX = fmaxf(0, vw / 2 - WIDE_CAMERA_PLAYER_KEEP), maxY = fmaxf(0, vh / 2 - WIDE_CAMERA_PLAYER_KEEP);
    wide_camera_spring(c, sx * maxX, sy * maxY, dt, WIDE_CAMERA_LOOK_TAU);
  } else if (walking) {
    wide_camera_spring(c, 0, 0, dt, WIDE_CAMERA_RETURN_TAU);
  } else if (stick) {
    float speed = WIDE_CAMERA_LOOK_SPEED * fmaxf(1, fit / s); // further out, the view travels faster
    c->lookX += sx * speed * dt;
    c->lookY += sy * speed * dt;
  }
  // As far as a screen's worth of map past the game's own view each way.
  float limX = fmaxf(vw, (float)fw), limY = fmaxf(vh, (float)fh);
  c->lookX = fminf(fmaxf(c->lookX, -limX), limX);
  c->lookY = fminf(fmaxf(c->lookY, -limY), limY);
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
