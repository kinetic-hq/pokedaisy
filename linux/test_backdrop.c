/* BACKGROUND's looks (backdrop.h): true black round BLACK's cards, cards that never read as black on black, text
 * off white, the burn-in dim and shift. */
#include "backdrop.h"
#include <assert.h>
#include <string.h>

static int channel(uint32_t c, int i) {
  return (int)(c >> (16 - 8 * i) & 0xff);
}

int main(void) {
  // Unset: COLORS decides (OLED = BLACK); a pick stays.
  assert(pocket_background_resolve(POCKET_BACKGROUND_AUTO, true) == POCKET_BACKGROUND_BLACK);
  assert(pocket_background_resolve(POCKET_BACKGROUND_AUTO, false) == POCKET_BACKGROUND_THEME);
  assert(pocket_background_resolve(POCKET_BACKGROUND_DARK, true) == POCKET_BACKGROUND_DARK);
  assert(pocket_background_resolve(POCKET_BACKGROUND_THEME, true) == POCKET_BACKGROUND_THEME);
  assert(pocket_background_resolve(7, false) == POCKET_BACKGROUND_THEME);
  assert(!strcmp(pocket_background_name(POCKET_BACKGROUND_BLACK), "BLACK"));

  struct pocket_backdrop black = pocket_backdrop_for(POCKET_BACKGROUND_BLACK);
  assert(black.outside == 0x000000 && black.cards);
  for (int bg = POCKET_BACKGROUND_BLACK; bg <= POCKET_BACKGROUND_DARK; bg++) {
    struct pocket_backdrop b = pocket_backdrop_for(bg);
    for (int i = 0; i < 3; i++) {
      // A near-black card (0x0E..0x1B), lighter than what's round it, with a faint edge and a gutter in between.
      assert(channel(b.card, i) >= 0x0e && channel(b.card, i) <= 0x1b);
      assert(channel(b.card, i) > channel(b.outside, i));
      assert(channel(b.gutter, i) > channel(b.card, i) && channel(b.gutter, i) < 0x30);
    }
    assert(b.borderAlpha > 0 && b.borderAlpha < 48);
  }
  struct pocket_backdrop theme = pocket_backdrop_for(POCKET_BACKGROUND_THEME);
  assert(!theme.cards && theme.card == 0x101418);

  // Text off white on the cards, as drawn on THEME; DIM COMPANION darker; idle a minute: down to 60% in two seconds.
  assert(pocket_wing_shade(POCKET_BACKGROUND_THEME, false, 0) == 255);
  assert(pocket_wing_shade(POCKET_BACKGROUND_BLACK, false, 0) == 230);
  assert(pocket_wing_shade(POCKET_BACKGROUND_BLACK, true, 0) < 150);
  assert(pocket_wing_shade(POCKET_BACKGROUND_BLACK, false, POCKET_WING_IDLE_MS) == 230);
  assert(pocket_wing_shade(POCKET_BACKGROUND_BLACK, false, POCKET_WING_IDLE_MS + 1000) < 230);
  assert(pocket_wing_shade(POCKET_BACKGROUND_BLACK, false, POCKET_WING_IDLE_MS + 5000) == 230 * 60 / 100);

  // The shift: still at first, then at most 2 px each way; never on THEME.
  int dx, dy, moved = 0;
  pocket_wing_shift(POCKET_BACKGROUND_BLACK, 0, 2, &dx, &dy);
  assert(!dx && !dy);
  for (uint32_t t = 0; t < 4 * POCKET_WING_SHIFT_MS; t += POCKET_WING_SHIFT_MS) {
    pocket_wing_shift(POCKET_BACKGROUND_BLACK, t, 5, &dx, &dy);
    assert(dx >= 0 && dx <= 2 && dy >= 0 && dy <= 2);
    moved += dx || dy;
  }
  assert(moved == 3);
  pocket_wing_shift(POCKET_BACKGROUND_THEME, POCKET_WING_SHIFT_MS, 2, &dx, &dy);
  assert(!dx && !dy);
  return 0;
}
