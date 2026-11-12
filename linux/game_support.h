/* GPL-3.0, see ../NOTICE. How well the companion supports a game, for the game grid's third tag: FULL (every
 * companion tab: PARTY, BAG, DEX, GUIDE, BATTLE with the foe's team), PARTLY (a companion, some of that missing) or
 * PLAY ONLY (no companion: the game runs). Worked out by the companion's own detection (pd_game_detect /
 * pd_game_identify, the DS frontend's carts), so it can't fall behind it; cached in DATA by file size and time. */
#ifndef POCKET_GAME_SUPPORT_H
#define POCKET_GAME_SUPPORT_H
#include <stdbool.h>
#include <stddef.h>

enum { GAME_SUPPORT_UNKNOWN, GAME_SUPPORT_FULL, GAME_SUPPORT_PARTLY, GAME_SUPPORT_PLAY };

/* The tag's word ("FULL", "PARTLY", "PLAY ONLY"; NULL for UNKNOWN). */
const char *game_support_word(int level);
/* One ROM, now (reads it): the level and what's missing or why ("No DEX, GUIDE"). */
int game_support_of_rom(const char *path, char *detail, size_t size);
/* The library's games in the background (stamp: the program's version; a new one looks again). */
void game_support_start(const char *root, const char *const *paths, int count, const char *stamp);
/* Game i's level so far (UNKNOWN until it's looked at) and its detail. */
int game_support_get(int i, char *detail, size_t size);
/* Bumped as results come in. */
int game_support_generation(void);
void game_support_stop(void);
#endif
