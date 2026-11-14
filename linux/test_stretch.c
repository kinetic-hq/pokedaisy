/* GAME SPEED's sound (stretch.h): at each speed a tone keeps its pitch, and the sound comes out SPEED times shorter,
 * without clicks where the windows meet. */
#include "stretch.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define RATE 32768

static struct stretch s;

// Rising zero crossings a second in L.
static double pitch(const int16_t *x, int n) {
  int up = 0;
  for (int i = 1; i < n; i++)
    if (x[2 * (i - 1)] < 0 && x[2 * i] >= 0)
      up++;
  return up * (double)RATE / n;
}

int main(void) {
  static const double speeds[] = {1.15, 1.25, 1.5, 1.75, 2, 2.5, 3, 4, 0.5, 0.25};
  static const double tones[] = {220, 440, 1000};
  int inN = RATE * 4;
  int16_t *in = malloc(sizeof(int16_t) * 2 * inN), *out = malloc(sizeof(int16_t) * 2 * RATE * 20);
  assert(in && out);
  for (size_t t = 0; t < sizeof tones / sizeof *tones; t++) {
    for (int i = 0; i < inN; i++)
      in[2 * i] = in[2 * i + 1] = (int16_t)(12000 * sin(6.283185307 * tones[t] * i / RATE));
    for (size_t k = 0; k < sizeof speeds / sizeof *speeds; k++) {
      double sp = speeds[k];
      stretch_reset(&s, sp);
      int made = 0;
      // Fed as the game posts it, a few hundred frames at a time.
      for (int at = 0; at < inN; at += 547) {
        int n = inN - at < 547 ? inN - at : 547;
        made += stretch_run(&s, in + 2 * at, n, out + 2 * made, RATE * 20 - made);
      }
      double want = inN / sp, got = made;
      double f = pitch(out + 2 * 2048, made - 4096);
      // The largest step between two samples: a sine at this tone never steps more than about 2 pi f A / RATE.
      int jump = 0;
      for (int i = 2049; i < made - 2048; i++) {
        int d = abs(out[2 * i] - out[2 * (i - 1)]);
        if (d > jump)
          jump = d;
      }
      double limit = 6.283185307 * tones[t] * 12000 / RATE * 1.6 + 200;
      printf("%5.0f Hz at %.2fx: %d frames (want %.0f), pitch %.1f Hz, largest step %d (limit %.0f)\n", tones[t], sp,
             made, want, f, jump, limit);
      assert(fabs(got - want) < want * 0.03 + 3 * STRETCH_WIN);
      assert(fabs(f - tones[t]) < tones[t] * 0.03);
      assert(jump < limit);
    }
  }
  free(in);
  free(out);
  return 0;
}
