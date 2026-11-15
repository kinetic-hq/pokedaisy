/* ADD TO DESKTOP: a shortcut on the desktop that starts PokeDaisy straight into one game.
 * GPL-3.0, see ../NOTICE. */
#ifndef POCKET_SHORTCUT_H
#define POCKET_SHORTCUT_H
#include <stdbool.h>
#include <stddef.h>

enum { SHORTCUT_MADE, SHORTCUT_THERE, SHORTCUT_FAILED };

/* Makes the shortcut, named NAME, that runs this program with --data ROOT and ROM. ICON (SIZE x SIZE RGBA pixels) is
 * kept in ROOT/shortcuts; with none, Windows takes ICON_PROGRAM's own icon (a PC game's .exe), else PokeDaisy's. A
 * file of that name already on the desktop is never replaced (SHORTCUT_THERE). OUT gets the shortcut's path. */
int shortcut_create(const char *root, const char *rom, const char *name, const unsigned char *icon, int size,
                    const char *iconProgram, char *out, size_t outSize);

/* The pieces, for the test. */
/* NAME as a file name: what Windows or a desktop refuses (\ / : * ? " < > | and control characters) dropped, spaces
 * and dots trimmed from the ends; "Game" when nothing is left. */
void shortcut_file_name(const char *name, char *out, size_t size);
/* A .desktop file's Exec value: each argument in double quotes, its " ` $ \ escaped, % doubled, and every backslash
 * doubled again for the file's own string escapes. */
void shortcut_exec_line(const char *const *args, int count, char *out, size_t size);
/* A PNG of SIZE x SIZE RGBA pixels (malloc'd, *LEN bytes); and the same PNG wrapped as an .ico (one image). */
unsigned char *shortcut_png(const unsigned char *rgba, int size, size_t *len);
unsigned char *shortcut_ico(const unsigned char *png, size_t pngLen, int size, size_t *len);
#endif
