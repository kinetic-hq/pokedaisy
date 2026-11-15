/* GPL-3.0, see ../NOTICE. rom_archive: a game in a .zip unpacked once, kept, written again when the archive changes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pocket_os.h"
#include "rom_archive.h"
#include "zlib.h"

static void put16(FILE *f, unsigned v) { fputc(v & 255, f), fputc(v >> 8 & 255, f); }
static void put32(FILE *f, unsigned long v) { put16(f, v & 0xFFFF), put16(f, v >> 16 & 0xFFFF); }

/* A zip of N files stored as they are (no compression). */
static void write_zip(const char *path, int n, const char *const *names, const char *const *data) {
  FILE *f = fopen(path, "wb");
  assert(f);
  unsigned long offs[8], crcs[8];
  for (int i = 0; i < n; i++) {
    size_t nl = strlen(names[i]), dl = strlen(data[i]);
    offs[i] = (unsigned long)ftell(f);
    crcs[i] = crc32(0, (const unsigned char *)data[i], (unsigned)dl);
    put32(f, 0x04034b50), put16(f, 10), put16(f, 0), put16(f, 0), put16(f, 0), put16(f, 0);
    put32(f, crcs[i]), put32(f, dl), put32(f, dl), put16(f, (unsigned)nl), put16(f, 0);
    fwrite(names[i], 1, nl, f), fwrite(data[i], 1, dl, f);
  }
  unsigned long cd = (unsigned long)ftell(f);
  for (int i = 0; i < n; i++) {
    size_t nl = strlen(names[i]), dl = strlen(data[i]);
    put32(f, 0x02014b50), put16(f, 20), put16(f, 10), put16(f, 0), put16(f, 0), put16(f, 0), put16(f, 0);
    put32(f, crcs[i]), put32(f, dl), put32(f, dl), put16(f, (unsigned)nl), put16(f, 0), put16(f, 0), put16(f, 0);
    put16(f, 0), put32(f, 0), put32(f, offs[i]);
    fwrite(names[i], 1, nl, f);
  }
  unsigned long end = (unsigned long)ftell(f);
  put32(f, 0x06054b50), put16(f, 0), put16(f, 0), put16(f, (unsigned)n), put16(f, (unsigned)n);
  put32(f, end - cd), put32(f, cd), put16(f, 0);
  fclose(f);
}
static void read_all(const char *path, char *out, size_t size) {
  FILE *f = fopen(path, "rb");
  assert(f);
  size_t n = fread(out, 1, size - 1, f);
  out[n] = 0;
  fclose(f);
}

int main(void) {
  char dir[1024], zip[1100], into[1100], out[1400], got[64];
  snprintf(dir, sizeof dir, "%s/pd-archive-XXXXXX", pocket_temp_root());
  assert(pocket_mkdtemp(dir));
  snprintf(zip, sizeof zip, "%s/Game (USA).zip", dir);
  snprintf(into, sizeof into, "%s/unpacked", dir);

  assert(rom_archive_is("a.zip") && rom_archive_is("B.7Z") && !rom_archive_is("a.gba") && !rom_archive_is("zip"));

  // The game is found past a text file and a Mac's shadow copy, and unpacked by its own name.
  const char *names[] = {"readme.txt", "__MACOSX/._Game.gba", "roms/Game.gba"};
  const char *data[] = {"hello", "x", "GBA-DATA-1"};
  write_zip(zip, 3, names, data);
  assert(rom_archive_unpack(zip, into, false, out, sizeof out));
  assert(strstr(out, "/Game.gba") && !strstr(out, "roms/"));
  read_all(out, got, sizeof got);
  assert(!strcmp(got, "GBA-DATA-1"));

  // Unchanged: kept (a change made to the copy stays, so it wasn't written again).
  FILE *f = fopen(out, "r+b");
  assert(f);
  fputc('g', f);
  fclose(f);
  assert(rom_archive_unpack(zip, into, false, out, sizeof out));
  read_all(out, got, sizeof got);
  assert(!strcmp(got, "gBA-DATA-1"));

  // A new game in the archive: written again; a file beside it (a save) is left alone.
  char save[1500];
  snprintf(save, sizeof save, "%s/Game.sav", into);
  f = fopen(save, "wb");
  fputs("SAVE", f);
  fclose(f);
  const char *data2[] = {"hello", "x", "GBA-DATA-2!"};
  write_zip(zip, 3, names, data2);
  assert(rom_archive_unpack(zip, into, false, out, sizeof out));
  read_all(out, got, sizeof got);
  assert(!strcmp(got, "GBA-DATA-2!"));
  read_all(save, got, sizeof got);
  assert(!strcmp(got, "SAVE"));

  // No game: nothing. A DS game counts only with NDS.
  const char *none[] = {"notes.txt"}, *noneData[] = {"hi"};
  write_zip(zip, 1, none, noneData);
  assert(!rom_archive_unpack(zip, into, true, out, sizeof out));
  const char *ds[] = {"Game.nds"}, *dsData[] = {"NDS"};
  write_zip(zip, 1, ds, dsData);
  assert(!rom_archive_unpack(zip, into, false, out, sizeof out));
  assert(rom_archive_unpack(zip, into, true, out, sizeof out));

  // Not an archive at all.
  assert(!rom_archive_unpack(save, into, true, out, sizeof out));
  puts("rom archive tests passed");
  return 0;
}
