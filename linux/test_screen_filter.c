/* GPL-3.0. FILTER's masks (screen_filter.c): OFF and SUBPIXEL leave the game alone, LCD's lines fall on the first
 * third of each game pixel and follow GRID, SCANLINES halves the top of each row, LCD PAPER and CRT stay in range and
 * are lighter in a pixel's middle than at its edge, and nothing is drawn while a game pixel is too small for it. */
#include "pd_ui.h"
#include "screen_filter.h"
#include <assert.h>
#include <stdio.h>

static int red(uint32_t p) { return p >> 16 & 255; }
static int sum(uint32_t p) { return (p >> 16 & 255) + (p >> 8 & 255) + (p & 255); }

int main(void) {
  // A 240x160 game at 6x.
  enum { SW = 240, SH = 160, DW = 1440, DH = 960 };
  assert(!screen_filter_on_game(PD_FILTER_OFF) && !screen_filter_on_game(PD_FILTER_SUBPIXEL));
  assert(screen_filter_on_game(PD_FILTER_LCD) && screen_filter_on_game(PD_FILTER_CRT));
  assert(screen_filter_pixel(PD_FILTER_OFF, 1, 7, 7, DW, DH, SW, SH) == 0xFFFFFFFFu);

  // LCD: window pixels 0-1 of each 6 are the line (the first third), 2-5 are not; GRID makes it darker.
  assert(red(screen_filter_pixel(PD_FILTER_LCD, 1, 0, 3, DW, DH, SW, SH)) < 255);
  assert(red(screen_filter_pixel(PD_FILTER_LCD, 1, 3, 3, DW, DH, SW, SH)) == 255);
  assert(red(screen_filter_pixel(PD_FILTER_LCD, 1, 3, 1, DW, DH, SW, SH)) < 255);
  assert(red(screen_filter_pixel(PD_FILTER_LCD, 0, 0, 3, DW, DH, SW, SH)) >
         red(screen_filter_pixel(PD_FILTER_LCD, 2, 0, 3, DW, DH, SW, SH)));
  // At 1x there's no room for lines.
  assert(screen_filter_pixel(PD_FILTER_LCD, 2, 0, 0, SW, SH, SW, SH) == 0xFFFFFFFFu);

  // SCANLINES: rows 0-2 of each 6 at half, 3-5 as they are.
  assert(red(screen_filter_pixel(PD_FILTER_SCANLINES, 1, 5, 1, DW, DH, SW, SH)) == 128);
  assert(red(screen_filter_pixel(PD_FILTER_SCANLINES, 1, 5, 4, DW, DH, SW, SH)) == 255);

  // LCD PAPER and CRT: lighter in the middle of a game pixel than at its edge.
  assert(sum(screen_filter_pixel(PD_FILTER_LCD_PAPER, 2, 3, 3, DW, DH, SW, SH)) >
         sum(screen_filter_pixel(PD_FILTER_LCD_PAPER, 2, 0, 0, DW, DH, SW, SH)));
  assert(sum(screen_filter_pixel(PD_FILTER_CRT, 1, 3, 3, DW, DH, SW, SH)) >
         sum(screen_filter_pixel(PD_FILTER_CRT, 1, 3, 0, DW, DH, SW, SH)));
  // CRT's stripes: each window column brightest in its own colour.
  uint32_t c0 = screen_filter_pixel(PD_FILTER_CRT, 1, 0, 3, DW, DH, SW, SH);
  uint32_t c1 = screen_filter_pixel(PD_FILTER_CRT, 1, 1, 3, DW, DH, SW, SH);
  assert((c0 >> 16 & 255) > (c0 >> 8 & 255) && (c1 >> 8 & 255) > (c1 >> 16 & 255));
  puts("PASS screen filter masks");
  return 0;
}
