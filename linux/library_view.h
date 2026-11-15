/* GPL-3.0, see ../NOTICE. The game list's SORT: by name either way, by when a game was added or last played, or by
 * console; and the per-game times it sorts by, kept in DATA as "file<TAB>seconds" lines (added.txt, played.txt). */
#ifndef POCKET_LIBRARY_VIEW_H
#define POCKET_LIBRARY_VIEW_H
#include <stdbool.h>
#include <stddef.h>

enum { LIBRARY_SORT_NAME, LIBRARY_SORT_NAME_DESC, LIBRARY_SORT_ADDED, LIBRARY_SORT_PLAYED, LIBRARY_SORT_CONSOLE,
       LIBRARY_SORTS };
/* The bar button's words for each ("SORT: A-Z"...). */
extern const char *const LIBRARY_SORT_BAR[LIBRARY_SORTS];

struct library_entry {
  const char *name;      // as the list shows it
  int system;            // boxart.h's enum boxart_system
  long long added;       // seconds; 0 = not known
  long long played;      // seconds; 0 = never
};
/* ORDER[0..N) = the entries' indices in SORT's order (ties by name, then index, so it never shuffles). */
void library_sort(int *order, int n, const struct library_entry *e, int sort);

/* A "file<TAB>seconds" table read from DATA/<name>. */
struct library_stamps {
  char **files;
  long long *values;
  int count, cap;
};
void library_stamps_load(struct library_stamps *s, const char *root, const char *name);
long long library_stamps_get(const struct library_stamps *s, const char *file); // 0 when not there
void library_stamps_put(struct library_stamps *s, const char *file, long long value);
void library_stamps_remove(struct library_stamps *s, const char *file);
/* Written through a temporary file and a rename. */
bool library_stamps_save(const struct library_stamps *s, const char *root, const char *name);
void library_stamps_free(struct library_stamps *s);
/* One game's stamp set (VALUE 0: taken off) and the file saved (PLAY: played.txt's "now"). */
bool library_stamp(const char *root, const char *name, const char *file, long long value);

/* COLLECTIONS: the game list's tabs, in their order. A game is in ALL, its console's, ROM HACKS or ORIGINALS (not a
 * PC fan game: PC), and FAVORITES when starred. */
enum { LIBRARY_ALL, LIBRARY_GBA, LIBRARY_GBC, LIBRARY_GB, LIBRARY_NDS, LIBRARY_PC, LIBRARY_HACKS, LIBRARY_ORIGINALS,
       LIBRARY_FAVORITES, LIBRARY_COLLECTIONS };
extern const char *const LIBRARY_COLLECTION_NAMES[LIBRARY_COLLECTIONS];
/* SYSTEM is boxart.h's enum boxart_system. */
bool library_in_collection(int collection, int system, bool hack, bool favorite);
#endif
