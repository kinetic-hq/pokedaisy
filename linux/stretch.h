/* GAME SPEED's sound at its own pitch: the game's samples, made at SPEED times the real rate, are cut into windows
 * and laid back together HOP apart (WSOLA). Each window is taken SPEED x HOP on from the last, moved up to SEEK
 * samples to where it best matches the sound the last one ran into, so the waves meet without a click. 1.5x plays
 * two thirds of the sound at the pitch it was made at, instead of all of it a fifth higher. Stereo int16 in and
 * out. */
#ifndef POCKET_STRETCH_H
#define POCKET_STRETCH_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define STRETCH_HOP 512
#define STRETCH_WIN (2 * STRETCH_HOP)
#define STRETCH_SEEK 256
#define STRETCH_CAP 16384 // frames held, a half second at 32768 Hz

struct stretch {
  double speed;      // 1: the samples go straight through
  float in[STRETCH_CAP][2];
  int len;           // frames in `in`
  double next;       // where the next window would start, if nothing moved it
  int last;          // where the last one did start
  bool started;
  float tail[STRETCH_HOP][2]; // the last window's second half, faded, waiting for the next one's first
  float win[STRETCH_WIN];
};

static inline void stretch_reset(struct stretch *s, double speed) {
  s->speed = speed;
  s->len = 0;
  s->next = 0;
  s->last = 0;
  s->started = false;
  memset(s->tail, 0, sizeof s->tail);
  for (int i = 0; i < STRETCH_WIN; i++)
    s->win[i] = 0.5f - 0.5f * cosf(6.28318531f * i / STRETCH_WIN); // two halves overlapping add up to 1
}

/* A new SPEED: starts over (the sound held is dropped, a few hundredths of a second) when it changes. */
static inline void stretch_speed(struct stretch *s, double speed) {
  if (fabs(speed - s->speed) > 1e-6)
    stretch_reset(s, speed);
}

/* How well the window at P lines up with the sound the last window ran into (at REF): their product over a hop,
 * over the window's own loudness, so a loud stretch doesn't win only by being loud. */
static inline float stretch_match(const struct stretch *s, int p, int ref) {
  float dot = 0, energy = 1e-3f;
  for (int i = 0; i < STRETCH_HOP; i += 2) {
    float a = s->in[p + i][0] + s->in[p + i][1], b = s->in[ref + i][0] + s->in[ref + i][1];
    dot += a * b;
    energy += a * a;
  }
  return dot / sqrtf(energy);
}

/* Feeds N frames (interleaved L, R) in and writes what's ready to OUT (room for MAX frames); returns the frames
 * written. */
static inline int stretch_run(struct stretch *s, const int16_t *samples, int n, int16_t *out, int max) {
  if (n > STRETCH_CAP - s->len)
    n = STRETCH_CAP - s->len; // only if nothing is read for a half second: the rest is dropped
  for (int i = 0; i < n; i++) {
    s->in[s->len + i][0] = samples[2 * i];
    s->in[s->len + i][1] = samples[2 * i + 1];
  }
  s->len += n;
  int made = 0;
  while (made + STRETCH_HOP <= max) {
    int p;
    if (!s->started) {
      if (s->len < STRETCH_WIN)
        break;
      p = 0;
      s->started = true;
    } else {
      int at = (int)s->next, lo = at - STRETCH_SEEK, hi = at + STRETCH_SEEK, ref = s->last + STRETCH_HOP;
      if (lo < 0)
        lo = 0;
      if (hi + STRETCH_WIN > s->len)
        break;
      // A coarse look every 8 samples, then every one around the best.
      int best = at < lo ? lo : at;
      float bestScore = -INFINITY;
      for (int q = lo; q <= hi; q += 8) {
        float m = stretch_match(s, q, ref);
        if (m > bestScore) {
          bestScore = m;
          best = q;
        }
      }
      int c = best;
      for (int q = c - 7; q <= c + 7; q++) {
        if (q < lo || q > hi)
          continue;
        float m = stretch_match(s, q, ref);
        if (m > bestScore) {
          bestScore = m;
          best = q;
        }
      }
      p = best;
    }
    for (int i = 0; i < STRETCH_HOP; i++)
      for (int ch = 0; ch < 2; ch++) {
        float v = s->tail[i][ch] + s->win[i] * s->in[p + i][ch];
        out[2 * (made + i) + ch] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        s->tail[i][ch] = s->win[STRETCH_HOP + i] * s->in[p + STRETCH_HOP + i][ch];
      }
    made += STRETCH_HOP;
    s->last = p;
    s->next += STRETCH_HOP * s->speed;
    // Drop what no window can reach any more.
    int keep = s->last + STRETCH_HOP;
    if ((int)s->next - STRETCH_SEEK < keep)
      keep = (int)s->next - STRETCH_SEEK;
    if (keep > 0) {
      if (keep > s->len)
        keep = s->len;
      memmove(s->in, s->in + keep, (size_t)(s->len - keep) * sizeof s->in[0]);
      s->len -= keep;
      s->last -= keep;
      s->next -= keep;
    }
  }
  return made;
}

#endif
