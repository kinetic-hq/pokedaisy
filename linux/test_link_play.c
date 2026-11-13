/* GPL-3.0. LINK PLAY's saves and layout (link_play.c): a second copy of a game gets "(link 2)" next to the first's save,
 * the games fill the window without overlapping, and a click finds its game. */
#include "link_play.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool same(const char *a, const char *b) { return !strcmp(a, b); }

static bool overlap(struct link_rect a, struct link_rect b) {
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

int main(void) {
  char out[256];
  link_copy_save("D:/data/saves/Emerald.gba.sav", 0, out, sizeof out);
  assert(!strcmp(out, "D:/data/saves/Emerald.gba.sav"));
  link_copy_save("D:/data/saves/Emerald.gba.sav", 1, out, sizeof out);
  assert(!strcmp(out, "D:/data/saves/Emerald.gba (link 2).sav"));
  link_copy_save("C:\\games\\My.Game.sav", 2, out, sizeof out);
  assert(!strcmp(out, "C:\\games\\My.Game (link 3).sav"));
  link_copy_save("/saves.d/save", 1, out, sizeof out);
  assert(!strcmp(out, "/saves.d/save (link 2)"));

  const char *roms[] = {"em.gba", "fr.gba", "em.gba", "em.gba"};
  assert(link_copy_index(roms, 0, same) == 0 && link_copy_index(roms, 1, same) == 0);
  assert(link_copy_index(roms, 2, same) == 1 && link_copy_index(roms, 3, same) == 2);

  struct link_rect at[4];
  for (int n = 2; n <= 4; n++) {
    link_layout(n, 1920, 1080, 240, 160, at);
    for (int i = 0; i < n; i++) {
      assert(at[i].x >= 0 && at[i].y >= 0 && at[i].x + at[i].w <= 1920 && at[i].y + at[i].h <= 1080);
      assert(at[i].w * 160 / at[i].h >= 239 && at[i].w * 160 / at[i].h <= 241); // the game's own shape
      for (int k = 0; k < i; k++)
        assert(!overlap(at[i], at[k]));
      assert(link_hit(at, n, at[i].x + at[i].w / 2, at[i].y + at[i].h / 2) == i);
    }
  }
  link_layout(2, 1920, 1080, 240, 160, at);
  assert(at[0].y == at[1].y && at[0].x < at[1].x); // a wide window: side by side
  link_layout(2, 720, 1280, 240, 160, at);
  assert(at[0].x == at[1].x && at[0].y < at[1].y); // a tall one: one over the other
  assert(link_hit(at, 2, 0, 0) == -1 || link_hit(at, 2, 0, 0) == 0);
  puts("PASS link play saves and layout");
  return 0;
}
