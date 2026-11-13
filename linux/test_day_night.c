/* GPL-3.0. DAY AND NIGHT (day_night.h): the clock's tints, where it applies and its easing. */
#include "day_night.h"
#include <assert.h>
#include <stdio.h>

static bool eq(const int a[3], int r, int g, int b) { return a[0] == r && a[1] == g && a[2] == b; }

int main(void) {
  int t[3];
  day_night_at(2, t);
  assert(eq(t, 135, 150, 225)); // night
  day_night_at(6, t);
  assert(eq(t, 245, 220, 220)); // dawn
  day_night_at(12, t);
  assert(eq(t, 256, 256, 256)); // day: none
  day_night_at(18, t);
  assert(eq(t, 255, 200, 165)); // dusk
  day_night_at(23.99f, t);
  assert(eq(t, 135, 150, 225));
  day_night_at(6.75f, t); // halfway from dawn to day
  assert(eq(t, 250, 238, 238));
  day_night_at(-1, t); // 23:00
  assert(eq(t, 135, 150, 225));

  assert(day_night_outdoors(-1) && day_night_outdoors(1) && day_night_outdoors(3) && day_night_outdoors(6));
  assert(!day_night_outdoors(4) && !day_night_outdoors(8) && !day_night_outdoors(0));

  struct day_night d;
  day_night_reset(&d);
  int none[3] = {256, 256, 256}, night[3] = {135, 150, 225};
  assert(!day_night_step(&d, none)); // nothing to give
  int frames = 0;
  while (!eq(d.cur, 135, 150, 225)) {
    assert(day_night_step(&d, night)); // every step changes it
    frames++;
  }
  assert(frames == (256 - 135 + DAY_NIGHT_STEP - 1) / DAY_NIGHT_STEP);
  // Held there: given again once a second.
  int gives = 0;
  for (int i = 0; i < 120; i++)
    gives += day_night_step(&d, night);
  assert(gives == 2);
  // Back to none, then quiet.
  while (!eq(d.cur, 256, 256, 256))
    assert(day_night_step(&d, none));
  for (int i = 0; i < 120; i++)
    assert(!day_night_step(&d, none));
  printf("day_night ok\n");
  return 0;
}
