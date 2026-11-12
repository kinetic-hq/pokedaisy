/* The game list's names: a ROM's file name made into the game's own name ("pokemon - ash gray (v3.6).gba" ->
 * "Pokémon Ash Gray"), as the Android app's library shows them. Only what the list draws: files keep their names.
 * GPL-3.0, see ../NOTICE. */
#ifndef POCKET_GAME_NAME_H
#define POCKET_GAME_NAME_H
#include <stdbool.h>
#include <stddef.h>

/* FILE (a file name, no folder) as a name: no extension, dump numbering ("1636 - "), (tags) / [tags], versions
 * (v1.2.3), "_" or " - " between words; "Pokémon" spelled the games' way and put first when the name has it or
 * POKEMON says the cart is one (its header); the games' own casing (FireRed, HeartGold, ...), else each word
 * capitalised. UTF-8. */
void game_display_name(const char *file, bool pokemon, char *out, size_t size);
/* The cart at PATH is a Pokémon game, by its header: a GBA game code, a Game Boy title, a DS game code. */
bool game_header_is_pokemon(const char *path);
/* The grid's version line from FILE: "(Rev 1)" as "Rev 1", else the first version word ("v4.5.3", "1.2.6" as
 * "v1.2.6"); "" when it has none. */
void game_version(const char *file, char *out, size_t size);
/* FILE looks like a retail dump (a No-Intro region tag, no hack / patch words): ORIGINAL, else ROM HACK. */
bool game_is_original(const char *file);
/* RENAME: the player's own name for FILE (DATA/names.txt), and setting one (NULL or "": back to the file's). */
bool game_custom_name(const char *root, const char *file, char *out, size_t size);
bool game_custom_name_set(const char *root, const char *file, const char *name);

#endif
