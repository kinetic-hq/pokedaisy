/* GPL-3.0, see ../NOTICE. LINK PLAY's pieces that don't need a window (main.c runs the games on the app's pk_link.c):
 * where each game's save is, and where each game goes on the screen. */
#ifndef LINK_PLAY_H
#define LINK_PLAY_H
#include <stdbool.h>
#include <stddef.h>

#define LINK_PLAY_MAX 4

struct link_rect {
  int x, y, w, h;
};

/* Game COPY's save for a game whose own save is OWN: copy 0 (the first time the game is in the link) plays OWN; a second
 * copy of the same game plays "<own's name> (link 2).<ext>" next to it, as the app names it. */
void link_copy_save(const char *own, int copy, char *out, size_t size);

/* Which copy of its game entry I is (0 for the first): how many entries before it name the same ROM. */
int link_copy_index(const char *const *roms, int i, bool (*same)(const char *, const char *));

/* Where N games of GW x GH go in an OW x OH window: two side by side (one over the other in a tall window), three or
 * four in a two by two grid, each as large as its cell allows with its own shape and room for its frame. */
void link_layout(int n, int ow, int oh, int gw, int gh, struct link_rect *out);

/* The game at window point (X, Y), or -1. */
int link_hit(const struct link_rect *at, int n, int x, int y);

#endif
