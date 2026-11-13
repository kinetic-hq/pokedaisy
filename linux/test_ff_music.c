// SPDX-License-Identifier: GPL-3.0-or-later
// FF MUSIC's STEADY (ff_music.c): the loop cut and the song table, on made-up data (no ROM bytes in the repo).
#include "ff_music.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  // A tune 3 s long that repeats, the loop's ends known only to a frame or so: the cut finds the exact period.
  const int rate = FFM_RATE, period = rate * 3 + 77;
  size_t frames = (size_t)rate * 12;
  int16_t *pcm = malloc(frames * 4);
  for (size_t i = 0; i < frames; i++) {
    double t = (double)(i % (size_t)period) / rate;
    int16_t v = (int16_t)(6000 * sin(t * 440 * 6.283) + 3000 * sin(t * 97 * 6.283) + 2000 * sin(t * t * 50));
    pcm[i * 2] = v;
    pcm[i * 2 + 1] = (int16_t)(v / 2);
  }
  size_t from, len;
  size_t start = (size_t)rate * 2, end = start + (size_t)period + rate / 60; // a frame off
  assert(ffm_splice(pcm, frames * 2, start * 2, end * 2, rate, &from, &len));
  assert(len == (size_t)period * 2);
  assert(from >= start * 2);
  // Too short a recording for a whole period past the cut: none.
  assert(!ffm_splice(pcm, (start + (size_t)period) * 2, start * 2, end * 2, rate, &from, &len));

  // m4aSongNumStart as agbcc built it, its literal pool holding gSongTable.
  static const uint8_t fn[32] = {0x00, 0xb5, 0x00, 0x04, 0x07, 0x4a, 0x08, 0x49, 0x40, 0x0b, 0x40, 0x18, 0x83, 0x88, 0x59, 0x00,
                                 0xc9, 0x18, 0x89, 0x00, 0x89, 0x18, 0x0a, 0x68, 0x01, 0x68, 0x10, 0x1c, 0x00, 0xf0, 0xa2, 0xfb};
  uint8_t *rom = calloc(1, 0x10000);
  memcpy(rom + 0x1000, fn, sizeof fn);
  // `ldr r2, [pc, #0x1C]` at +6: (0x1006 + 4) & ~3 + 0x1C = 0x1028.
  uint32_t table = 0x08004000;
  memcpy(rom + 0x1028, &table, 4);
  uint32_t songNumStart = 0, found = 0;
  assert(ffm_locate_table(rom, 0x10000, &songNumStart, &found));
  assert(songNumStart == 0x08001001 && found == table);
  memset(rom + 0x1000, 0, sizeof fn);
  assert(!ffm_locate_table(rom, 0x10000, &songNumStart, &found));
  free(rom);
  free(pcm);
  puts("ff music ok");
  return 0;
}
