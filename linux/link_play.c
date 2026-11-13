/* GPL-3.0, see ../NOTICE. LINK PLAY's saves and layout (link_play.h). */
#include "link_play.h"
#include <stdio.h>
#include <string.h>

void link_copy_save(const char *own, int copy, char *out, size_t size) {
  if (copy <= 0) {
    snprintf(out, size, "%s", own);
    return;
  }
  // The extension is the last dot after the last separator: "saves/My.Game.sav" -> "saves/My.Game (link 2).sav".
  const char *name = own;
  for (const char *s = own; *s; s++)
    if (*s == '/' || *s == '\\')
      name = s + 1;
  const char *dot = strrchr(name, '.');
  if (!dot || dot == name)
    snprintf(out, size, "%s (link %d)", own, copy + 1);
  else
    snprintf(out, size, "%.*s (link %d)%s", (int)(dot - own), own, copy + 1, dot);
}

int link_copy_index(const char *const *roms, int i, bool (*same)(const char *, const char *)) {
  int copy = 0;
  for (int k = 0; k < i; k++)
    if (same(roms[k], roms[i]))
      copy++;
  return copy;
}

void link_layout(int n, int ow, int oh, int gw, int gh, struct link_rect *out) {
  if (n < 1 || gw <= 0 || gh <= 0)
    return;
  int cols = 2, rows = 2;
  if (n == 1)
    cols = rows = 1;
  else if (n == 2) {
    // Side by side unless one over the other leaves the games bigger.
    double side = (ow / 2.0) / gw < (double)oh / gh ? (ow / 2.0) / gw : (double)oh / gh;
    double stack = (double)ow / gw < (oh / 2.0) / gh ? (double)ow / gw : (oh / 2.0) / gh;
    if (side >= stack)
      cols = 2, rows = 1;
    else
      cols = 1, rows = 2;
  }
  const int gap = 12; // the frame's room around each game
  int cw = ow / cols, ch = oh / rows;
  for (int i = 0; i < n; i++) {
    int cx = (i % cols) * cw, cy = (i / cols) * ch;
    double k = (double)(cw - 2 * gap) / gw < (double)(ch - 2 * gap) / gh ? (double)(cw - 2 * gap) / gw
                                                                         : (double)(ch - 2 * gap) / gh;
    if (k < 0.25)
      k = 0.25;
    int w = (int)(gw * k), h = (int)(gh * k);
    out[i] = (struct link_rect){cx + (cw - w) / 2, cy + (ch - h) / 2, w, h};
  }
  if (n == 3) {
    // The third game alone on its row: in the middle of it.
    out[2].x = (ow - out[2].w) / 2;
  }
}

int link_hit(const struct link_rect *at, int n, int x, int y) {
  for (int i = 0; i < n; i++)
    if (x >= at[i].x - 12 && y >= at[i].y - 12 && x < at[i].x + at[i].w + 12 && y < at[i].y + at[i].h + 12)
      return i;
  return -1;
}
