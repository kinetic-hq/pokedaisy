#pragma once
/* BACKGROUND BLACK / DARK: the wings' windows in the theme's dark look with the card's near black as their fill, so a
 * wing reads as a dark card on the black round it, never a large light window. pocket_wing_look_begin swaps the
 * look in (keeping the old one in *was), pocket_wing_look_end puts it back. A Game Boy game's own windows stay. */
#include "backdrop.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "pd_style.h"
#ifdef __cplusplus
}
#endif
#include <stdbool.h>

struct pocket_wing_look {
  const struct pd_look *cur;
  bool dark;
  struct pd_look look;
};

static inline void pocket_wing_look_begin(int background, struct pocket_wing_look *w) {
  w->cur = pd_cur;
  w->dark = pd_dark;
  struct pocket_backdrop b = pocket_backdrop_for(background);
  if (!b.cards || pd_gb_look || !pd_cur)
    return;
  const struct pd_look *dark = pd_cur;
  for (int i = 0; i < pd_theme_count; i++)
    if (pd_cur == &pd_themes[i].look[0] || pd_cur == &pd_themes[i].look[1])
      dark = &pd_themes[i].look[1];
  w->look = *dark;
  w->look.listFill = b.card;
  w->look.titleFill = b.card;
  pd_cur = &w->look;
  pd_dark = true;
}

static inline void pocket_wing_look_end(struct pocket_wing_look *w) {
  pd_cur = w->cur;
  pd_dark = w->dark;
}
