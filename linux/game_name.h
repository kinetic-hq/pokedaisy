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

#endif
