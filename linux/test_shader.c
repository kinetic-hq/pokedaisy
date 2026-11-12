/* GPL-3.0. COLORS' table: ORIGINAL keeps every colour, HANDHELD and OLED keep black black and stay in range,
 * OLED dims peak white; and the setting names. */
#include "shader.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t xbgr(int r, int g, int b) { return (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r; }
static int red(uint32_t xrgb) { return (int)(xrgb >> 16 & 0xFF); }
static int green(uint32_t xrgb) { return (int)(xrgb >> 8 & 0xFF); }
static int blue(uint32_t xrgb) { return (int)(xrgb & 0xFF); }

int main(void) {
  assert(!strcmp(pocket_shader_name(POCKET_SHADER_XBRZ), "xBRZ"));
  assert(!strcmp(pocket_shader_name(99), "OFF"));
  assert(!strcmp(pocket_colors_name(POCKET_COLORS_OLED), "OLED"));

  pocket_shader_set(POCKET_SHADER_OFF, POCKET_COLORS_ORIGINAL, false);
  assert(!pocket_shader_active());
  // GBA colours (5 bits stretched to 8) come back as they went in, channels in XRGB order.
  for (int v = 0; v < 32; v++) {
    int c = v << 3 | v >> 2;
    uint32_t out = pocket_shader_color(xbgr(c, 255 - c, c / 2));
    assert(red(out) == c && green(out) == 255 - c && blue(out) == c / 2);
  }

  for (int ds = 0; ds < 2; ds++)
    for (int colors = POCKET_COLORS_HANDHELD; colors < POCKET_COLORS_COUNT; colors++) {
      pocket_shader_set(POCKET_SHADER_OFF, colors, ds);
      assert(pocket_shader_active());
      assert(pocket_shader_color(xbgr(0, 0, 0)) == 0); // true black
      uint32_t white = pocket_shader_color(xbgr(255, 255, 255));
      assert(red(white) > 200 && green(white) > 200 && blue(white) > 200);
      if (colors == POCKET_COLORS_OLED)
        assert(red(white) < 255); // a little less glare
      // A pure red is still the reddest channel.
      uint32_t r = pocket_shader_color(xbgr(255, 0, 0));
      assert(red(r) > green(r) && red(r) > blue(r));
    }
  // BRIGHTNESS: black and white stay, a mid grey goes up (110 and 150) or down (80), and only away from 100.
  pocket_shader_set(POCKET_SHADER_OFF, POCKET_COLORS_ORIGINAL, false);
  int last = -1;
  for (int pct = POCKET_BRIGHTNESS_MIN; pct <= POCKET_BRIGHTNESS_MAX; pct += POCKET_BRIGHTNESS_STEP) {
    pocket_shader_brightness(pct);
    assert(pocket_shader_active() == (pct != 100));
    assert(pocket_shader_color(xbgr(0, 0, 0)) == 0);
    assert(red(pocket_shader_color(xbgr(255, 255, 255))) == 255);
    int grey = red(pocket_shader_color(xbgr(132, 132, 132)));
    assert(grey > last);
    last = grey;
  }
  pocket_shader_brightness(100);
  pocket_shader_set(POCKET_SHADER_XBRZ, POCKET_COLORS_ORIGINAL, false);
  assert(pocket_shader_active());
  puts("PASS shader colours and names");
  return 0;
}
