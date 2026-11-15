/* GPL-3.0, see ../NOTICE. ROMs kept in a .zip or .7z: the game inside unpacked once into a folder of DATA's, and that
 * copy listed and played. The archive itself is only ever read. */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* NAME ends in .zip or .7z. */
bool rom_archive_is(const char *name);

/* The first game in ARCHIVE (a .gba, .gbc, .gb, or .nds with NDS) unpacked into DIR, made if it's missing, as its own
 * file name; OUT its path. A copy already there is kept while the archive still holds the same file (its CRC and size,
 * noted beside it in DIR/archive.txt); else it is written again, through a temporary file. Nothing in DIR is ever
 * deleted: a DS game's save sits beside its ROM. False: no game in it, or it couldn't be read or written. */
bool rom_archive_unpack(const char *archive, const char *dir, bool nds, char *out, size_t size);
