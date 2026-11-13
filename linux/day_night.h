/* GPL-3.0. DAY AND NIGHT (SETTINGS > MAKEOVER), as the Android app's DayNight.kt and EmulatorEngine.stepTint: the
 * overworld follows the clock (a warm dawn, plain day, an orange dusk, a blue night) in any GBA game without a day and
 * night of its own. The tint is pk_wide.c's (pk_tint_set: the map's and the sprites' palettes as drawn, the text
 * windows' and the game's memory untouched), eased in and out by a few steps a frame, only outdoors and on the field. */
#ifndef POCKET_DAY_NIGHT_H
#define POCKET_DAY_NIGHT_H

#include <stdbool.h>

#define DAY_NIGHT_STEP 4 // per frame, of 256: about a second from day to night

struct day_night {
  int cur[3];   // the tint drawn now (256 = as is)
  int sent[3];  // the tint last given to the core
  int frames;   // since it was last given
};

/* The day's colours by hour; between two keys the tint goes linearly (DayNight.kt's KEYS). */
static inline void day_night_at(float hour, int out[3]) {
  static const float H[] = {0, 4.5f, 6, 7.5f, 16.5f, 18, 19.5f, 24};
  static const int C[][3] = {{135, 150, 225}, {135, 150, 225}, {245, 220, 220}, {256, 256, 256},
                             {256, 256, 256}, {255, 200, 165}, {135, 150, 225}, {135, 150, 225}};
  int n = (int)(sizeof H / sizeof H[0]);
  while (hour < 0)
    hour += 24;
  while (hour >= 24)
    hour -= 24;
  int i = 0;
  while (i < n - 2 && H[i + 1] <= hour)
    i++;
  float t = H[i + 1] > H[i] ? (hour - H[i]) / (H[i + 1] - H[i]) : 0;
  for (int k = 0; k < 3; k++)
    out[k] = (int)(C[i][k] + (C[i + 1][k] - C[i][k]) * t);
}

/* gMapHeader.mapType under the sky (town, city, route, ocean route); unknown (-1) counts as outdoors. */
static inline bool day_night_outdoors(int mapType) {
  return mapType < 0 || mapType == 1 || mapType == 2 || mapType == 3 || mapType == 6;
}

static inline void day_night_reset(struct day_night *d) {
  for (int k = 0; k < 3; k++)
    d->cur[k] = d->sent[k] = 256;
  d->frames = 0;
}

/* One frame toward [target]: true when the core should be given cur now (it changed, or once a second while tinted,
 * so a renderer set up again picks it back up). */
static inline bool day_night_step(struct day_night *d, const int target[3]) {
  bool tinted = false, changed = false;
  for (int k = 0; k < 3; k++) {
    int v = d->cur[k], t = target[k];
    v = v < t ? (v + DAY_NIGHT_STEP > t ? t : v + DAY_NIGHT_STEP) : (v - DAY_NIGHT_STEP < t ? t : v - DAY_NIGHT_STEP);
    d->cur[k] = v;
    changed |= v != d->sent[k];
    tinted |= v != 256;
  }
  if (changed || (tinted && ++d->frames >= 60)) {
    for (int k = 0; k < 3; k++)
      d->sent[k] = d->cur[k];
    d->frames = 0;
    return true;
  }
  return false;
}

#endif
