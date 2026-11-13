/* GPL-3.0, see ../NOTICE. The game list's box art. A game's own pictures live in its art folder (BOX ART LOCATION:
 * BESIDE GAMES, a "boxart" folder in the game's own folder, or ONE FOLDER with gba / gbc / gb / nds folders in it),
 * named by the name the list shows: "Name.png" is its box art, "Name 1.png", "Name 2.jpg" its alternates. Nothing in
 * there is ever overwritten or deleted: a new picture pushes the one it replaces to the next free number. What the
 * search finds goes there too; a hack's base-game box (and the marks of what was tried) stay in DATA/boxart, as did
 * the art of 1.18.1 and older (DATA/boxart, the player's own in DATA/boxart/custom), copied to the folder once.
 * Box art is
 * looked for in the background - the libretro thumbnails by the file's name, then (an original under another name)
 * by the game the ROM's header names, then SteamGridDB with the player's own key (DATA/steamgriddb-key.txt), and last
 * (a ROM hack nobody has art for) the box of the game it was made from, marked so the list puts the hack's name on a
 * banner across it - and drawn as SDL textures over the list. */
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
  char base[128];  // the libretro name of the game the header names ("" unknown), and its system
  int baseSystem;
  bool hack;       // a ROM hack: the base game's box is the last resort, with a banner
  char dir[1024];  // the ROM's folder: BESIDE GAMES keeps its art in dir/boxart
};

/* BOX ART LOCATION (DATA/boxart-location.txt). */
enum { BOXART_BESIDE_GAMES, BOXART_ONE_FOLDER };
void boxart_location(int *where, char *folder, size_t size);
bool boxart_location_set(int where, const char *folder);
/* The list's games, each time it loads: their art folders, and the art of older versions copied into them (once). */
void boxart_games(const struct boxart_game *games, int count);
/* ONE FOLDER: another game on another console has this one's name, so a picture in the folder's top level is
 * ignored for both (their console's folder holds theirs). */
bool boxart_name_shared(const char *stem);
/* The game's art folder (it may not exist yet). */
bool boxart_art_folder(const char *stem, char *out, size_t size);
/* FILE is the art of a game called NAME: "Name.png" (*num 0, its box art) or "Name 2.jpg" (*num 2, an alternate);
 * any case, "Pokemon" for "Pokémon", .png / .jpg / .jpeg. */
bool boxart_art_name_matches(const char *file, const char *name, int *num);
/* Alternate NUM becomes the game's box art, and its box art takes NUM (the two swap names). */
bool boxart_art_use(const char *stem, int num);

/* The game a ROM's header names (GBA and DS game codes, Game Boy titles): its libretro (No-Intro) name and system. */
bool boxart_base_of_rom(const char *path, char *name, size_t size, int *system);
/* SYNC BOX ART: every game looked for again; all = also those with art found before (the player's own stays). */
void boxart_sync(const struct boxart_game *games, int count, bool all);
/* The background search's progress: games done of total; false when it has nothing left to do. */
bool boxart_progress(int *done, int *total);
/* The art is the base game's box (a ROM hack's last resort): the list draws the hack's name across it. */
bool boxart_is_base(const char *stem);
/* Bumped whenever art is found or cleared: the list redraws (and rechecks its banners) when it changes. */
int boxart_generation(void);

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
/* FROM URL / FROM FILE: a local image file or an https URL, as the game's box art in its folder (the one it had
 * becomes an alternate). err: why not. */
bool boxart_set_custom(const char *stem, const char *source, char *err, size_t errSize);
bool boxart_clear_custom(const char *stem);
bool boxart_has_custom(const char *stem);
/* The art as a texture (cached; NULL while there is none). */
SDL_Texture *boxart_texture(SDL_Renderer *r, const char *stem, int *w, int *h);
void boxart_textures_free(void);
/* SteamGridDB's name for an entry is this title (the same words, "Pokemon" and "Version" aside). */
bool boxart_title_matches(const char *want, const char *got);
/* BOX ART's picker: every cover found for one game, to choose from. libretro's own cover and the base game's come
 * first, then every grid SteamGridDB has for its entry of the game's name (fetched a page at a time as the picker
 * nears the end, the thumbnails near the one in view first). Each candidate's thumbnail is box art named by
 * boxart_pick_stem (boxart_texture draws it). */
enum { BOXART_PICK_STEAMGRIDDB, BOXART_PICK_LIBRETRO, BOXART_PICK_BASE, BOXART_PICK_LOCAL };
enum { BOXART_PICK_LOADING, BOXART_PICK_DONE, BOXART_PICK_NO_KEY };
struct boxart_pick_item {
  int source, w, h;
  int local;          // BOXART_PICK_LOCAL: 0 the game's box art, n its alternate n
  bool ready, failed; // its thumbnail is there / couldn't be fetched
};
void boxart_pick_open(const struct boxart_game *g);
/* SEARCH AGAIN: SteamGridDB's games for a term; boxart_pick_names lists them (-1 while looking), use_entry loads
 * one's grids in place of the current entry's. */
void boxart_pick_search(const char *term);
int boxart_pick_names(char names[][128], int max);
void boxart_pick_use_entry(int i);
int boxart_pick_count(void);
bool boxart_pick_item(int i, struct boxart_pick_item *out);
void boxart_pick_stem(int i, char *out, size_t size);
/* The candidate in view: thumbnails near it come first, and the next page when it nears the end. */
void boxart_pick_focus(int i);
/* BOXART_PICK_*; entry: SteamGridDB's name for the game shown ("" none). */
int boxart_pick_state(char *entry, size_t size);
/* The candidate as the game's box art: a picture from elsewhere is saved in its folder (the one it had becomes an
 * alternate), an alternate swaps names with it. Blocks while it downloads. */
bool boxart_pick_choose(const char *stem, int i, char *err, size_t errSize);
void boxart_pick_close(void);
/* A SteamGridDB grids page's pictures (url, thumbnail, size), in order: exposed for the tests. */
struct boxart_grid {
  char url[512], thumb[512];
  int w, h;
};
int boxart_parse_grids(const char *json, struct boxart_grid *out, int max);
/* The string value of "key" in JSON, unescaped (\uXXXX to UTF-8): exposed for the tests. */
bool boxart_json_string(const char *json, const char *key, char *out, size_t size);
/* The libretro thumbnail name for a file stem: &*\/:`<>?\| become _ (their rule). */
void boxart_libretro_name(const char *stem, char *out, size_t size);
#endif
