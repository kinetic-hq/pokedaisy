/* COMPANION VIEW layouts, see view_layout.h. GPL-3.0, see ../NOTICE. */
#include "view_layout.h"
#include <string.h>

int view_fit_scale(int w, int h, int frameW, int frameH) {
  if (frameW <= 0 || frameH <= 0)
    return 1;
  int sx = w / frameW, sy = h / frameH, s = sx < sy ? sx : sy;
  return s < 1 ? 1 : s;
}

int view_wing_scale(int screenW, int screenH, int frameW, int frameH, int minPercent) {
  // Each wing keeps minPercent of the width: the game may use the rest.
  int room = screenW - 2 * ((screenW * minPercent + 99) / 100);
  return view_fit_scale(room, screenH, frameW, frameH);
}

static struct view_rect rect(int x, int y, int w, int h) {
  struct view_rect r = {x, y, w < 0 ? 0 : w, h < 0 ? 0 : h};
  return r;
}

// The game at SCALE, centred in the area x0..x0+w0 by y0..y0+h0.
static void place_game(struct view_layout *out, int scale, int x0, int y0, int w0, int h0, int fw, int fh) {
  out->scale = scale;
  out->game = rect(x0 + (w0 - fw * scale) / 2, y0 + (h0 - fh * scale) / 2, fw * scale, fh * scale);
}

// The companion as a window: about 84% of the height, 4:3, no wider than 90% of the screen.
static struct view_rect companion_window(int sw, int sh) {
  int h = sh * 84 / 100 / 3 * 3, w = h * 4 / 3; // an exact 4:3
  if (w > sw * 90 / 100) {
    w = sw * 90 / 100 / 4 * 4;
    h = w * 3 / 4;
  }
  return rect((sw - w) / 2, (sh - h) / 2, w, h);
}

// Panels in the columns beside the game, strips above and below it when the rows there are worth it.
static void wings_around(struct view_layout *out, int sw, int sh) {
  struct view_rect g = out->game;
  out->left = rect(0, 0, g.x, sh);
  out->right = rect(g.x + g.w, 0, sw - g.x - g.w, sh);
  int above = g.y, below = sh - g.y - g.h;
  int min = (sh * VIEW_STRIP_MIN_PERCENT + 99) / 100;
  if (min > VIEW_STRIP_MIN_PX)
    min = VIEW_STRIP_MIN_PX;
  if (above >= min && below >= min) {
    out->top = rect(g.x, 0, g.w, above);
    out->bottom = rect(g.x, g.y + g.h, g.w, below);
  }
}

static void wings(struct view_layout *out, const struct view_state *s) {
  place_game(out, view_wing_scale(s->screenW, s->screenH, s->frameW, s->frameH, VIEW_WING_MIN_PERCENT), 0, 0,
             s->screenW, s->screenH, s->frameW, s->frameH);
  wings_around(out, s->screenW, s->screenH);
}

static void full(struct view_layout *out, const struct view_state *s) {
  place_game(out, view_fit_scale(s->screenW, s->screenH, s->frameW, s->frameH), 0, 0, s->screenW, s->screenH,
             s->frameW, s->frameH);
  out->expand = true;
  out->hud = true;
}

static void window_over(struct view_layout *out, const struct view_state *s) {
  out->companion = companion_window(s->screenW, s->screenH);
  out->dim = true;
  out->hud = false;
}

void view_layout(int view, const struct view_state *s, struct view_layout *out) {
  memset(out, 0, sizeof *out);
  int sw = s->screenW, sh = s->screenH;
  switch (view) {
  default:
  case POCKET_VIEW_BATTLE_WINGS:
    if (s->battle)
      wings(out, s);
    else
      full(out, s);
    if (s->companionOpen)
      window_over(out, s);
    break;
  case POCKET_VIEW_WINDOW:
    full(out, s);
    if (s->companionOpen)
      window_over(out, s);
    break;
  case POCKET_VIEW_SIDE_PANEL:
    if (s->panelLocked) {
      // The game in the left half, the panel keeping the right half.
      int half = sw / 2;
      place_game(out, view_fit_scale(half, sh, s->frameW, s->frameH), 0, 0, half, sh, s->frameW, s->frameH);
      out->companion = rect(half, 0, sw - half, sh);
      out->companionSlides = true;
    } else {
      full(out, s);
      if (s->companionOpen) {
        out->companion = rect(sw / 2, 0, sw - sw / 2, sh);
        out->companionSlides = true;
        out->hud = false;
      }
    }
    break;
  case POCKET_VIEW_WINGS:
    wings(out, s);
    if (s->companionOpen)
      window_over(out, s);
    break;
  case POCKET_VIEW_GLASS_DRAWER:
    full(out, s);
    if (s->companionOpen) {
      int h = sh * VIEW_DRAWER_PERCENT / 100;
      out->companion = rect(0, sh - h, sw, h);
      out->companionGlass = true;
      out->companionSlides = true;
      out->hud = false;
    }
    break;
  case POCKET_VIEW_STEP_BACK:
    if (!s->companionOpen) {
      full(out, s);
      break;
    }
    {
      int scale = view_fit_scale(sw, sh, s->frameW, s->frameH) - 2;
      place_game(out, scale < 1 ? 1 : scale, 0, 0, sw, sh, s->frameW, s->frameH);
      wings_around(out, sw, sh);
    }
    break;
  }
}

// Smoothstep on 0..1000.
static int ease(int t) {
  if (t <= 0)
    return 0;
  if (t >= 1000)
    return 1000;
  long long x = t;
  return (int)(x * x * (3000 - 2 * x) / 1000000);
}

static int mix(int a, int b, int t) {
  return a + (int)((long long)(b - a) * t / 1000);
}

struct view_rect view_rect_lerp(struct view_rect a, struct view_rect b, int t) {
  int e = ease(t);
  // A rectangle appearing or going grows from / shrinks to the other's middle: give it B's (or A's) place.
  if (a.w == 0 || a.h == 0)
    a = rect(b.x + b.w / 2, b.y + b.h / 2, 0, 0);
  if (b.w == 0 || b.h == 0)
    b = rect(a.x + a.w / 2, a.y + a.h / 2, 0, 0);
  struct view_rect r = {mix(a.x, b.x, e), mix(a.y, b.y, e), mix(a.w, b.w, e), mix(a.h, b.h, e)};
  return r;
}

void view_layout_lerp(const struct view_layout *a, const struct view_layout *b, int t, struct view_layout *out) {
  struct view_layout r = *b;
  r.game = view_rect_lerp(a->game, b->game, t);
  r.left = view_rect_lerp(a->left, b->left, t);
  r.right = view_rect_lerp(a->right, b->right, t);
  r.top = view_rect_lerp(a->top, b->top, t);
  r.bottom = view_rect_lerp(a->bottom, b->bottom, t);
  r.companion = view_rect_lerp(a->companion, b->companion, t);
  *out = r;
}
