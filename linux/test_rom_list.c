/* The game list's folders and SAVES BESIDE GAMES' save name; empty files only, no game data. */
#include "rom_list.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pocket_os.h"

static void touch(const char *dir, const char *name) {
  char path[1200];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  assert(f);
  fputs("x", f);
  fclose(f);
}
static void folder(char *out, size_t size, const char *parent, const char *name) {
  snprintf(out, size, "%s/%s", parent, name);
  assert(pocket_mkdir(out));
}
static int find(const struct rom_list *l, const char *label) {
  for (int i = 0; i < l->count; i++)
    if (!strcmp(l->labels[i], label))
      return i;
  return -1;
}

int main(void) {
  char tmpl[1100];
  snprintf(tmpl, sizeof tmpl, "%s/pd-romlist-XXXXXX", pocket_temp_root());
  char *root = pocket_mkdtemp(tmpl);
  assert(root);
  char roms[1200], gbaSub[1200], extra[1200], gba[1200], nds[1200], deep[1200], hidden[1200], other[1200];
  folder(roms, sizeof roms, root, "roms");
  touch(roms, "b game.gba");
  touch(roms, "notes.txt");
  folder(gbaSub, sizeof gbaSub, roms, "hacks");
  touch(gbaSub, "A hack.GBA");

  // No roms-folders.txt: DATA/roms and its subfolders, as before plus one level.
  struct rom_list l = {0};
  rom_list_load(&l, root, false);
  assert(l.count == 2);
  assert(!strcmp(l.labels[0], "A hack.GBA") && !strcmp(l.labels[1], "b game.gba"));
  rom_list_free(&l);

  // Another tree like D:\Emulation\roms: gba/, nds/, a folder two levels down and a hidden one.
  folder(extra, sizeof extra, root, "Emulation roms");
  folder(gba, sizeof gba, extra, "gba");
  touch(gba, "c red.gb");
  touch(gba, "c red.sav");
  folder(nds, sizeof nds, extra, "nds");
  touch(nds, "d ds game.nds");
  folder(deep, sizeof deep, gba, "deeper");
  touch(deep, "too deep.gba");
  folder(hidden, sizeof hidden, extra, ".stversions");
  touch(hidden, "old copy.gba");
  folder(other, sizeof other, root, "single");
  touch(other, "e color.gbc");

  char list[1200];
  snprintf(list, sizeof list, "%s/%s", root, POCKET_ROM_FOLDERS_FILE);
  FILE *f = fopen(list, "wb");
  assert(f);
  // A UTF-8 mark, a comment, a blank line, quotes and a trailing separator, the same folder twice.
  fprintf(f, "\xEF\xBB\xBF# my games\r\n\r\n  \"%s\"  \r\n%s/\r\n%s\n", extra, extra, other);
  fclose(f);
  rom_list_load(&l, root, false);
  assert(l.count == 4);
  assert(find(&l, "c red.gb") >= 0 && find(&l, "e color.gbc") >= 0);
  assert(find(&l, "d ds game.nds") < 0 && find(&l, "too deep.gba") < 0 && find(&l, "old copy.gba") < 0);
  int red = find(&l, "c red.gb");
  char want[1300];
  snprintf(want, sizeof want, "%s/c red.gb", gba);
  assert(!strcmp(l.paths[red], want));
  for (int i = 1; i < l.count; i++)
    assert(strcasecmp(l.labels[i - 1], l.labels[i]) <= 0);
  rom_list_free(&l);

  rom_list_load(&l, root, true);
  assert(l.count == 5 && find(&l, "d ds game.nds") >= 0);
  rom_list_free(&l);

  // SAVES BESIDE GAMES' name.
  char sav[1300];
  rom_save_beside(sav, sizeof sav, "/x/Pokemon - FireRed.gba");
  assert(!strcmp(sav, "/x/Pokemon - FireRed.sav"));
  rom_save_beside(sav, sizeof sav, "/x.y/game");
  assert(!strcmp(sav, "/x.y/game.sav"));
  rom_save_beside(sav, sizeof sav, "/x/.hidden");
  assert(!strcmp(sav, "/x/.hidden.sav"));
#ifdef _WIN32
  rom_save_beside(sav, sizeof sav, "D:\\a.b\\Game v1.2.gbc");
  assert(!strcmp(sav, "D:\\a.b\\Game v1.2.sav"));
#endif

  // The copy keeps the source and writes the whole file.
  char from[1300], to[1300];
  snprintf(from, sizeof from, "%s/c red.sav", gba);
  snprintf(to, sizeof to, "%s/copy.sav", other);
  assert(rom_copy_file(from, to));
  assert(pocket_exists(from) && pocket_exists(to));
  snprintf(from, sizeof from, "%s/missing.sav", gba);
  assert(!rom_copy_file(from, to));
  puts("rom list ok");
  return 0;
}
