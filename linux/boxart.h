/* GPL-3.0, see ../NOTICE. The game list's box art: kept in DATA/boxart, the player's own in DATA/boxart/custom (it wins),
 * looked for in the background - the libretro thumbnails first, then SteamGridDB with the player's own key
 * (DATA/steamgriddb-key.txt) - and drawn as SDL textures over the list. */
#ifndef POCKET_BOXART_H
#define POCKET_BOXART_H
#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>

enum boxart_system { BOXART_GBA, BOXART_GBC, BOXART_GB, BOXART_NDS };

struct boxart_game {
  char stem[256];  // the ROM's file name without its extension: the art's name too
  char title[256]; // what the list calls it (for SteamGridDB's search)
  int system;      // enum boxart_system
};

/* DATA, once at start (starts the background worker). */
void boxart_init(const char *root);
void boxart_quit(void);
/* The SteamGridDB key ("" = none): read from / written to DATA/steamgriddb-key.txt. */
void boxart_key(char *out, size_t size);
bool boxart_key_save(const char *key); // false: not a key (letters and digits, 16 to 64 of them)
/* The games the worker looks for art for, in order: those with none yet and not tried before. */
void boxart_queue(const struct boxart_game *games, int count);
/* FIND BOX ART: this game now, tried again even if it was before. */
void boxart_find(const struct boxart_game *game);
/* A FIND that finished: 1 found, 0 none found, -1 none finished since the last call. */
int boxart_find_result(void);
bool boxart_finding(void);
/* SET BOX ART: a local image file or an https URL, kept as the player's own. err: why not. */
bool boxart_set_custom(const char *stem, const char *source, char *err, size_t errSize);
bool boxart_clear_custom(const char *stem);
bool boxart_has_custom(const char *stem);
/* The art as a texture (cached; NULL while there is none). */
SDL_Texture *boxart_texture(SDL_Renderer *r, const char *stem, int *w, int *h);
void boxart_textures_free(void);
/* The libretro thumbnail name for a file stem: &*\/:`<>?\| become _ (their rule). */
void boxart_libretro_name(const char *stem, char *out, size_t size);
#endif
