/* GPL-3.0, see ../NOTICE. shortcut: ADD TO DESKTOP's file name, its .desktop Exec line, and its icon's PNG / ICO. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shortcut.h"

int main(void) {
  char out[512];
  shortcut_file_name("Pokemon: Emerald? <v1.1>", out, sizeof out);
  assert(!strcmp(out, "Pokemon Emerald v1.1"));
  shortcut_file_name(" ..Game.. ", out, sizeof out);
  assert(!strcmp(out, "Game"));
  shortcut_file_name("a/b\\c|d", out, sizeof out);
  assert(!strcmp(out, "abcd"));
  shortcut_file_name("?*", out, sizeof out);
  assert(!strcmp(out, "Game"));

  // Each argument quoted; " ` $ \ escaped for the quotes, then backslashes doubled for the file; % doubled.
  const char *args[] = {"/opt/Poke Daisy/pokedaisy", "--data", "/home/a/DATA", "/g/50% \"x\" $y\\z.gba"};
  shortcut_exec_line(args, 4, out, sizeof out);
  assert(!strcmp(out, "\"/opt/Poke Daisy/pokedaisy\" \"--data\" \"/home/a/DATA\" "
                      "\"/g/50%% \\\\\"x\\\\\" \\\\$y\\\\\\\\z.gba\""));
  char tiny[8];
  shortcut_exec_line(args, 4, tiny, sizeof tiny);
  assert(strlen(tiny) == 7);

  // A 2x2 PNG: signature, IHDR of 2x2 RGBA; the ICO points at it right after its 22-byte header.
  unsigned char px[16];
  for (int i = 0; i < 16; i++)
    px[i] = (unsigned char)(i * 16);
  size_t len = 0, icoLen = 0;
  unsigned char *png = shortcut_png(px, 2, &len);
  assert(png && len > 8 + 25 + 12 + 12);
  assert(!memcmp(png, "\x89PNG\r\n\x1a\n", 8) && !memcmp(png + 12, "IHDR", 4));
  assert(png[19] == 2 && png[23] == 2 && png[24] == 8 && png[25] == 6);
  assert(!memcmp(png + len - 8, "IEND", 4));
  unsigned char *ico = shortcut_ico(png, len, 256, &icoLen);
  assert(ico && icoLen == len + 22);
  assert(ico[2] == 1 && ico[4] == 1 && ico[6] == 0 && ico[7] == 0 && ico[18] == 22);
  assert((size_t)(ico[14] | ico[15] << 8 | ico[16] << 16) == len && !memcmp(ico + 22, png, len));
  free(ico);
  free(png);
  puts("shortcut tests passed");
  return 0;
}
