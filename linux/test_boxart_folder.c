/* BOX ART LOCATION's art folders (boxart.h): names, the next free number, swaps, ONE FOLDER, the move of older art.
 * Only made-up pictures in a temporary folder. */
#include "boxart.h"
#include "pocket_os.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define E "\xC3\xA9"

/* A PNG's signature, IHDR and the start of IDAT: enough for the size check (stb_image's info); WIDTH tells two
 * apart. */
static void png(const char *path, int width) {
  unsigned char d[41] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0,
                         (unsigned char)width, 0, 0, 0, 1, 8, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'I', 'D', 'A', 'T'};
  FILE *f = fopen(path, "wb");
  assert(f && fwrite(d, 1, sizeof d, f) == sizeof d);
  fclose(f);
}
static int width_of(const char *path) {
  unsigned char d[41] = {0};
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;
  size_t n = fread(d, 1, sizeof d, f);
  fclose(f);
  return n == sizeof d ? d[19] : -1;
}
static int count;
static void count_each(const char *name, void *user) {
  (void)name;
  (void)user;
  count++;
}
static int files_in(const char *dir) {
  count = 0;
  pocket_list_dir(dir, count_each, NULL);
  return count;
}
/* The temporary folder, gone again. */
static void remove_each(const char *name, void *dir) {
  char path[1200];
  snprintf(path, sizeof path, "%s/%s", (const char *)dir, name);
  if (pocket_is_dir(path)) {
    pocket_list_dir(path, remove_each, path);
    pocket_rmdir(path);
  } else
    pocket_remove(path);
}

int main(void) {
  int num = -1;
  // Names: the list's name, any case, "Pokemon" for "Pokémon", .png / .jpg / .jpeg; a number after a space.
  assert(boxart_art_name_matches("Pok" E "mon Emerald.png", "Pok" E "mon Emerald", &num) && num == 0);
  assert(boxart_art_name_matches("pokemon emerald.JPG", "Pok" E "mon Emerald", &num) && num == 0);
  assert(boxart_art_name_matches("Pokemon Emerald 2.jpeg", "Pok" E "mon Emerald", &num) && num == 2);
  assert(!boxart_art_name_matches("Pokemon Emerald.webp.txt", "Pok" E "mon Emerald", &num));
  assert(!boxart_art_name_matches("Pokemon Emerald 0.png", "Pok" E "mon Emerald", &num));
  assert(!boxart_art_name_matches("Pokemon Emerald2.png", "Pok" E "mon Emerald", &num));
  assert(!boxart_art_name_matches("Pokemon Emerald X.png", "Pok" E "mon Emerald", &num));
  assert(!boxart_art_name_matches("Pokemon Emerald Plus.png", "Pok" E "mon Emerald", &num));
  assert(boxart_art_name_matches("Pokemon_ Ash Gray.png", "Pok" E "mon: Ash Gray", &num) && num == 0);
  // The tile's name ("Pokemon " left off) works too.
  assert(boxart_art_name_matches("emerald 3.png", "Pok" E "mon Emerald", &num) && num == 3);
  assert(!boxart_art_name_matches("Pokemon.png", "Pok" E "mon Emerald", &num));

  char tmpl[400];
  snprintf(tmpl, sizeof tmpl, "%s/pd-boxart-XXXXXX", pocket_temp_root());
  char *top = pocket_mkdtemp(tmpl);
  assert(top);
  char data[500], roms[500], gbRoms[500], art[600], one[600], pic[900], pic2[900], path[1000];
  snprintf(data, sizeof data, "%s/DATA", top);
  snprintf(roms, sizeof roms, "%s/roms", top);
  snprintf(gbRoms, sizeof gbRoms, "%s/gb", top);
  snprintf(one, sizeof one, "%s/art", top);
  assert(pocket_mkdir(data) && pocket_mkdir(roms) && pocket_mkdir(gbRoms) && pocket_mkdir(one));
  boxart_init(data);

  struct boxart_game games[2];
  memset(games, 0, sizeof games);
  snprintf(games[0].stem, sizeof games[0].stem, "Pokemon - Emerald Version (USA, Europe)");
  snprintf(games[0].title, sizeof games[0].title, "Pokemon Emerald"); // (a test has no UTF-8 manifest)
  snprintf(games[0].dir, sizeof games[0].dir, "%s", roms);
  games[0].system = BOXART_GBA;
  // Another console's game under the same name (for ONE FOLDER's top level).
  snprintf(games[1].stem, sizeof games[1].stem, "Emerald (GB hack)");
  snprintf(games[1].title, sizeof games[1].title, "Pokemon Emerald");
  snprintf(games[1].dir, sizeof games[1].dir, "%s", gbRoms);
  games[1].system = BOXART_GB;
  const char *stem = games[0].stem;

  // Older art in DATA/boxart/custom is copied into the game's folder once, and stays where it was.
  snprintf(path, sizeof path, "%s/boxart/custom/%s.img", data, stem);
  png(path, 9);
  snprintf(art, sizeof art, "%s/boxart", roms);
  assert(!pocket_exists(art)); // no folder until a picture is saved
  boxart_games(games, 2);
  snprintf(pic, sizeof pic, "%s/Pokemon Emerald.png", art);
  assert(width_of(pic) == 9 && width_of(path) == 9);
  assert(boxart_has_custom(stem) && !boxart_is_base(stem));
  snprintf(path, sizeof path, "%s/boxart/%s.moved", data, stem);
  assert(pocket_exists(path));
  boxart_games(games, 2); // once: nothing more
  assert(files_in(art) == 1);

  // FROM FILE twice: each new picture is the box art, the one before it the next free alternate.
  char a[600], b[600];
  snprintf(a, sizeof a, "%s/a.png", top);
  snprintf(b, sizeof b, "%s/b.png", top);
  png(a, 1);
  png(b, 2);
  char err[160];
  assert(boxart_set_custom(stem, a, err, sizeof err));
  snprintf(pic2, sizeof pic2, "%s/Pokemon Emerald 1.png", art);
  assert(width_of(pic) == 1 && width_of(pic2) == 9);
  assert(boxart_set_custom(stem, b, err, sizeof err));
  snprintf(path, sizeof path, "%s/Pokemon Emerald 2.png", art);
  assert(width_of(pic) == 2 && width_of(pic2) == 9 && width_of(path) == 1);
  // Using an alternate swaps names with the box art.
  assert(boxart_art_use(stem, 1));
  assert(width_of(pic) == 9 && width_of(pic2) == 2);
  assert(!boxart_art_use(stem, 7)); // there's no 7
  assert(files_in(art) == 3);
  // The source files are untouched, and the game's own folder holds nothing but its boxart folder.
  assert(width_of(a) == 1 && width_of(b) == 2);
  assert(files_in(roms) == 1);

  // ONE FOLDER: by console; a name two consoles share isn't taken from the top level.
  assert(boxart_location_set(BOXART_ONE_FOLDER, one));
  boxart_games(games, 2);
  assert(boxart_name_shared(stem) && boxart_name_shared(games[1].stem));
  snprintf(path, sizeof path, "%s/Pokemon Emerald.png", one);
  png(path, 5); // the top level: ignored for both
  assert(!boxart_has_custom(games[1].stem));
  assert(boxart_set_custom(games[1].stem, a, err, sizeof err));
  snprintf(path, sizeof path, "%s/gb/Pokemon Emerald.png", one);
  assert(width_of(path) == 1);
  snprintf(path, sizeof path, "%s/Pokemon Emerald.png", one);
  assert(width_of(path) == 5); // never touched
  // Alone, a game takes its picture from the top level when its console's folder has none.
  boxart_games(games, 1);
  assert(!boxart_name_shared(stem));
  char folder[1100];
  assert(boxart_art_folder(stem, folder, sizeof folder) && !strcmp(folder, one));
  assert(boxart_has_custom(stem));
  assert(boxart_location_set(BOXART_BESIDE_GAMES, NULL));
  assert(boxart_art_folder(stem, folder, sizeof folder) && !strcmp(folder, art));

  boxart_quit();
  pocket_list_dir(top, remove_each, top);
  pocket_rmdir(top);
  puts("PASS box art folders: names, next free number, swaps, ONE FOLDER, the move of older art");
  return 0;
}
