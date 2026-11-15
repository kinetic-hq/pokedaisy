/* The game list's SORT (library_sort) and the times it sorts by (added.txt / played.txt). */
#include "boxart.h"
#include "library_view.h"
#include "pocket_os.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct library_entry games[] = {
    {"Pokemon Emerald", BOXART_GBA, 300, 0},
    {"pokemon crystal", BOXART_GBC, 100, 900},
    {"Pokemon Black", BOXART_NDS, 200, 500},
    {"Pokemon Red", BOXART_GB, 0, 0},
    {"Pokemon Fire Ash", BOXART_PC, 400, 0},
};
#define N (int)(sizeof games / sizeof games[0])

static void check(int sort, const int *want) {
  int order[N];
  library_sort(order, N, games, sort);
  for (int i = 0; i < N; i++)
    if (order[i] != want[i]) {
      fprintf(stderr, "sort %d: place %d is %d, want %d\n", sort, i, order[i], want[i]);
      assert(0);
    }
}

int main(void) {
  check(LIBRARY_SORT_NAME, (const int[]){2, 1, 0, 4, 3});      // A-Z, case ignored
  check(LIBRARY_SORT_NAME_DESC, (const int[]){3, 4, 0, 1, 2}); // Z-A
  check(LIBRARY_SORT_ADDED, (const int[]){4, 0, 2, 1, 3});     // newest first, not known last
  check(LIBRARY_SORT_PLAYED, (const int[]){1, 2, 0, 4, 3});    // last played first, the rest by name
  check(LIBRARY_SORT_CONSOLE, (const int[]){3, 1, 0, 2, 4});   // GB, GBC, GBA, NDS, PC

  assert(library_in_collection(LIBRARY_ALL, BOXART_PC, false, false, false));
  assert(library_in_collection(LIBRARY_GBC, BOXART_GBC, true, false, false));
  assert(!library_in_collection(LIBRARY_GBA, BOXART_GBC, true, false, false));
  assert(library_in_collection(LIBRARY_HACKS, BOXART_GBA, true, false, false));
  assert(!library_in_collection(LIBRARY_HACKS, BOXART_PC, true, false, false)); // a fan game is PC's, not a ROM hack
  assert(!library_in_collection(LIBRARY_ORIGINALS, BOXART_PC, false, false, false));
  assert(library_in_collection(LIBRARY_ORIGINALS, BOXART_NDS, false, false, false));
  assert(library_in_collection(LIBRARY_FAVORITES, BOXART_GB, false, true, false));
  assert(!library_in_collection(LIBRARY_FAVORITES, BOXART_GB, false, false, false));
  assert(!library_in_collection(LIBRARY_ALL, BOXART_GBA, false, true, true)); // hidden: out of ALL and the rest
  assert(!library_in_collection(LIBRARY_FAVORITES, BOXART_GBA, false, true, true));
  assert(library_in_collection(LIBRARY_HIDDEN, BOXART_GBA, false, false, true));
  assert(!library_in_collection(LIBRARY_HIDDEN, BOXART_GBA, false, false, false));
  // HIDE FOLDER
  {
    struct library_stamps s = {0};
    char key[64];
    library_folder_key("D:\\roms\\old", key, sizeof key);
    assert(!strcmp(key, "D:\\roms\\old/"));
    library_folder_key("D:/roms/new/", key, sizeof key);
    assert(!strcmp(key, "D:/roms/new/"));
    library_stamps_put(&s, "D:\\roms\\old/", 5);
    library_stamps_put(&s, "D:/roms/a.gba", 5);
    assert(library_hidden_folder(&s, "D:/roms/old/x.gba"));
    assert(library_hidden_folder(&s, "d:\\ROMS\\old\\deeper\\x.gba"));
    assert(!library_hidden_folder(&s, "D:/roms/older/x.gba"));
    assert(!library_hidden_folder(&s, "D:/roms/a.gba")); // a file's stamp is not a folder's
    assert(!library_hidden_folder(&s, "D:/roms/old/"));
    library_stamps_free(&s);
  }
  // SEARCH
  assert(library_matches("Pok\xC3\xA9mon Fire Ash", ""));
  assert(library_matches("Pok\xC3\xA9mon Fire Ash", "fire"));
  assert(library_matches("Pok\xC3\xA9mon Fire Ash", "ASH  fi"));
  assert(library_matches("Pok\xC3\xA9mon Fire Ash", "pokemon"));
  assert(library_matches("POK\xC3\x89MON Fire Ash", "pok\xC3\xA9"));
  assert(!library_matches("Pok\xC3\xA9mon Fire Ash", "fire red"));
  assert(!library_matches(NULL, "a"));

  char tmpl[] = "library-view-XXXXXX";
  char *dir = pocket_mkdtemp(tmpl);
  assert(dir);
  struct library_stamps s;
  library_stamps_load(&s, dir, "played.txt"); // no file yet: empty
  assert(s.count == 0);
  library_stamps_free(&s);
  assert(library_stamp(dir, "played.txt", "Emerald.gba", 1000));
  assert(library_stamp(dir, "played.txt", "sub/Crystal.gbc", 2000));
  assert(library_stamp(dir, "played.txt", "Emerald.gba", 3000)); // replaced, not added twice
  library_stamps_load(&s, dir, "played.txt");
  assert(s.count == 2);
  library_stamps_free(&s);
  assert(library_stamp(dir, "played.txt", "Gone.gba", 5));
  assert(library_stamp(dir, "played.txt", "Gone.gba", 0)); // 0 takes it off
  library_stamps_load(&s, dir, "played.txt");
  assert(s.count == 2 && library_stamps_get(&s, "Gone.gba") == 0);
  assert(library_stamps_get(&s, "Emerald.gba") == 3000);
  assert(library_stamps_get(&s, "sub/Crystal.gbc") == 2000);
  assert(library_stamps_get(&s, "Missing.gba") == 0);
  library_stamps_free(&s);
  char path[200];
  snprintf(path, sizeof path, "%s/played.txt", dir);
  assert(pocket_file_time(path) > 0);
  pocket_remove(path);
  pocket_rmdir(dir);
  puts("library view ok");
  return 0;
}
