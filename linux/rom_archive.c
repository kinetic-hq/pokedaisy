/* GPL-3.0, see ../NOTICE. .zip through minizip (zlib), .7z through the LZMA SDK's reader: both as mGBA bundles them. */
#include "rom_archive.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pocket_os.h"
#include "rom_list.h"
#include "unzip.h"

#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zFile.h"

bool rom_archive_is(const char *name) {
  const char *ext = strrchr(name, '.');
  return ext && (!strcasecmp(ext, ".zip") || !strcasecmp(ext, ".7z"));
}

/* NAME's last part ("roms/Game.gba" -> "Game.gba"). */
static const char *base_name(const char *name) {
  const char *b = name;
  for (const char *s = name; *s; s++)
    if (*s == '/' || *s == '\\')
      b = s + 1;
  return b;
}
/* An entry worth unpacking: a game by its name, not a Mac's "__MACOSX/._Game.gba" shadow, nothing hidden. */
static bool wanted(const char *name, bool nds) {
  const char *b = base_name(name);
  return *b && b[0] != '.' && !strstr(name, "__MACOSX") && rom_list_playable(b, nds);
}

/* DIR/archive.txt: the CRC and size of what was unpacked last. */
static bool stamp_same(const char *dir, uint32_t crc, unsigned long long size) {
  char path[1400];
  snprintf(path, sizeof path, "%s/archive.txt", dir);
  FILE *f = fopen(path, "r");
  if (!f)
    return false;
  unsigned long c = 0;
  unsigned long long s = 0;
  bool same = fscanf(f, "%lx %llu", &c, &s) == 2 && (uint32_t)c == crc && s == size;
  fclose(f);
  return same;
}
static void stamp_write(const char *dir, uint32_t crc, unsigned long long size) {
  char path[1400];
  snprintf(path, sizeof path, "%s/archive.txt", dir);
  FILE *f = fopen(path, "w");
  if (f) {
    fprintf(f, "%08lx %llu\n", (unsigned long)crc, size);
    fclose(f);
  }
}
/* OUT already the game the archive holds: there, its size, and the note says the same file. */
static bool up_to_date(const char *dir, const char *out, uint32_t crc, unsigned long long size) {
  return pocket_exists(out) && pocket_file_size(out) == (long long)size && stamp_same(dir, crc, size);
}

static bool unpack_zip(const char *archive, const char *dir, bool nds, char *out, size_t size) {
  unzFile z = unzOpen64(archive);
  if (!z)
    return false;
  bool ok = false, found = false;
  char name[1024];
  unz_file_info64 info;
  for (int r = unzGoToFirstFile(z); r == UNZ_OK && !found; r = unzGoToNextFile(z)) {
    if (unzGetCurrentFileInfo64(z, &info, name, sizeof name, NULL, 0, NULL, 0) != UNZ_OK)
      break;
    if (wanted(name, nds))
      found = true;
  }
  if (found) {
    snprintf(out, size, "%s/%s", dir, base_name(name));
    if (up_to_date(dir, out, (uint32_t)info.crc, info.uncompressed_size))
      ok = true;
    else if (pocket_mkdir(dir) && unzOpenCurrentFile(z) == UNZ_OK) {
      char tmp[1500];
      snprintf(tmp, sizeof tmp, "%s.tmp", out);
      FILE *f = fopen(tmp, "wb");
      ok = f != NULL;
      static unsigned char buf[1 << 16];
      int n;
      while (ok && (n = unzReadCurrentFile(z, buf, sizeof buf)) > 0)
        ok = fwrite(buf, 1, (size_t)n, f) == (size_t)n;
      if (n < 0)
        ok = false;
      if (unzCloseCurrentFile(z) != UNZ_OK) // UNZ_CRCERROR: a damaged archive
        ok = false;
      if (f && fclose(f))
        ok = false;
      ok = ok && pocket_replace(tmp, out);
      if (ok)
        stamp_write(dir, (uint32_t)info.crc, info.uncompressed_size);
      else
        pocket_remove(tmp);
    }
  }
  unzClose(z);
  return ok;
}

/* A 7z entry's name (UTF-16) as UTF-8. */
static void utf16_to_8(const UInt16 *w, char *out, size_t size) {
  size_t n = 0;
  for (; *w && n + 4 < size; w++) {
    uint32_t c = *w;
    if (c >= 0xD800 && c < 0xDC00 && w[1] >= 0xDC00 && w[1] < 0xE000)
      c = 0x10000 + ((c - 0xD800) << 10) + (w[1] - 0xDC00), w++;
    if (c < 0x80)
      out[n++] = (char)c;
    else if (c < 0x800)
      out[n++] = (char)(0xC0 | c >> 6), out[n++] = (char)(0x80 | (c & 63));
    else if (c < 0x10000)
      out[n++] = (char)(0xE0 | c >> 12), out[n++] = (char)(0x80 | ((c >> 6) & 63)), out[n++] = (char)(0x80 | (c & 63));
    else
      out[n++] = (char)(0xF0 | c >> 18), out[n++] = (char)(0x80 | ((c >> 12) & 63)),
      out[n++] = (char)(0x80 | ((c >> 6) & 63)), out[n++] = (char)(0x80 | (c & 63));
  }
  out[n] = 0;
}

static bool unpack_7z(const char *archive, const char *dir, bool nds, char *out, size_t size) {
  static const ISzAlloc alloc = {SzAlloc, SzFree};
  static bool crcTable;
  if (!crcTable) {
    CrcGenerateTable();
    crcTable = true;
  }
  CFileInStream stream;
  CLookToRead2 look;
  if (InFile_Open(&stream.file, archive))
    return false;
  FileInStream_CreateVTable(&stream);
  LookToRead2_CreateVTable(&look, False);
  enum { BUF = 1 << 18 };
  look.buf = malloc(BUF);
  if (!look.buf) {
    File_Close(&stream.file);
    return false;
  }
  look.bufSize = BUF;
  look.realStream = &stream.vt;
  LookToRead2_Init(&look);
  CSzArEx db;
  SzArEx_Init(&db);
  bool ok = false;
  if (SzArEx_Open(&db, &look.vt, &alloc, &alloc) == SZ_OK) {
    int pick = -1;
    char name[1024];
    UInt16 wname[1024];
    for (UInt32 i = 0; i < db.NumFiles && pick < 0; i++) {
      if (SzArEx_IsDir(&db, i) || SzArEx_GetFileNameUtf16(&db, i, NULL) > sizeof wname / sizeof *wname)
        continue;
      SzArEx_GetFileNameUtf16(&db, i, wname);
      utf16_to_8(wname, name, sizeof name);
      if (wanted(name, nds))
        pick = (int)i;
    }
    if (pick >= 0) {
      uint32_t crc = SzBitWithVals_Check(&db.CRCs, pick) ? db.CRCs.Vals[pick] : 0;
      unsigned long long bytes = SzArEx_GetFileSize(&db, pick);
      snprintf(out, size, "%s/%s", dir, base_name(name));
      if (crc && up_to_date(dir, out, crc, bytes))
        ok = true;
      else if (pocket_mkdir(dir)) {
        UInt32 block = 0xFFFFFFFF;
        Byte *data = NULL;
        size_t dataSize = 0, offset = 0, got = 0;
        if (SzArEx_Extract(&db, &look.vt, (UInt32)pick, &block, &data, &dataSize, &offset, &got, &alloc, &alloc) ==
                SZ_OK &&
            got == bytes) {
          char tmp[1500];
          snprintf(tmp, sizeof tmp, "%s.tmp", out);
          FILE *f = fopen(tmp, "wb");
          ok = f && fwrite(data + offset, 1, got, f) == got;
          if (f && fclose(f))
            ok = false;
          ok = ok && pocket_replace(tmp, out);
          if (ok)
            stamp_write(dir, crc, bytes);
          else
            pocket_remove(tmp);
        }
        ISzAlloc_Free(&alloc, data);
      }
    }
  }
  SzArEx_Free(&db, &alloc);
  free(look.buf);
  File_Close(&stream.file);
  return ok;
}

bool rom_archive_unpack(const char *archive, const char *dir, bool nds, char *out, size_t size) {
  const char *ext = strrchr(archive, '.');
  if (!ext)
    return false;
  if (!strcasecmp(ext, ".zip"))
    return unpack_zip(archive, dir, nds, out, size);
  if (!strcasecmp(ext, ".7z"))
    return unpack_7z(archive, dir, nds, out, size);
  return false;
}
