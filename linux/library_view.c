/* GPL-3.0, see ../NOTICE. */
#include "library_view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "boxart.h"
#include "pocket_os.h"

const char *const LIBRARY_SORT_BAR[LIBRARY_SORTS] = {"SORT: A-Z", "SORT: Z-A", "SORT: ADDED", "SORT: PLAYED",
                                                      "SORT: CONSOLE"};

const char *const LIBRARY_COLLECTION_NAMES[LIBRARY_COLLECTIONS] = {
    "ALL", "GBA", "GBC", "GB", "NDS", "PC", "ROM HACKS", "ORIGINALS", "FAVORITES", "HIDDEN"};

bool library_in_collection(int collection, int system, bool hack, bool favorite, bool hidden) {
  if (hidden || collection == LIBRARY_HIDDEN)
    return hidden && collection == LIBRARY_HIDDEN;
  switch (collection) {
  case LIBRARY_ALL:
    return true;
  case LIBRARY_GBA:
    return system == BOXART_GBA;
  case LIBRARY_GBC:
    return system == BOXART_GBC;
  case LIBRARY_GB:
    return system == BOXART_GB;
  case LIBRARY_NDS:
    return system == BOXART_NDS;
  case LIBRARY_PC:
    return system == BOXART_PC;
  case LIBRARY_HACKS:
    return system != BOXART_PC && hack;
  case LIBRARY_ORIGINALS:
    return system != BOXART_PC && !hack;
  case LIBRARY_FAVORITES:
    return favorite;
  default:
    return false;
  }
}

static const struct library_entry *sortEntries;
static int sortBy;
// CONSOLE: in the order they came out, the PC fan games last.
static int console_rank(int system) {
  switch (system) {
  case BOXART_GB:
    return 0;
  case BOXART_GBC:
    return 1;
  case BOXART_GBA:
    return 2;
  case BOXART_NDS:
    return 3;
  default:
    return 4;
  }
}
static int by_sort(const void *pa, const void *pb) {
  int ia = *(const int *)pa, ib = *(const int *)pb;
  const struct library_entry *a = &sortEntries[ia], *b = &sortEntries[ib];
  int c = 0;
  switch (sortBy) {
  case LIBRARY_SORT_ADDED: // newest first; not known last
    c = a->added < b->added ? 1 : a->added > b->added ? -1 : 0;
    break;
  case LIBRARY_SORT_PLAYED: // last played first; never played after, by name
    c = a->played < b->played ? 1 : a->played > b->played ? -1 : 0;
    break;
  case LIBRARY_SORT_CONSOLE:
    c = console_rank(a->system) - console_rank(b->system);
    break;
  default:
    break;
  }
  if (!c)
    c = strcasecmp(a->name ? a->name : "", b->name ? b->name : "");
  if (sortBy == LIBRARY_SORT_NAME_DESC)
    c = -c;
  return c ? c : ia - ib;
}
void library_sort(int *order, int n, const struct library_entry *e, int sort) {
  for (int i = 0; i < n; i++)
    order[i] = i;
  sortEntries = e;
  sortBy = sort;
  if (n > 1)
    qsort(order, (size_t)n, sizeof *order, by_sort);
}

void library_stamps_load(struct library_stamps *s, const char *root, const char *name) {
  *s = (struct library_stamps){0};
  char path[1100];
  snprintf(path, sizeof path, "%s/%s", root, name);
  FILE *f = fopen(path, "r");
  if (!f)
    return;
  char line[1200];
  while (fgets(line, sizeof line, f)) {
    size_t n = strlen(line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
      line[--n] = 0;
    char *tab = strrchr(line, '\t');
    if (!tab || tab == line)
      continue;
    *tab = 0;
    long long v = strtoll(tab + 1, NULL, 10);
    if (v > 0)
      library_stamps_put(s, line, v);
  }
  fclose(f);
}
static int find(const struct library_stamps *s, const char *file) {
  for (int i = 0; i < s->count; i++)
    if (!strcmp(s->files[i], file))
      return i;
  return -1;
}
long long library_stamps_get(const struct library_stamps *s, const char *file) {
  int i = find(s, file);
  return i >= 0 ? s->values[i] : 0;
}
void library_stamps_put(struct library_stamps *s, const char *file, long long value) {
  if (strchr(file, '\t') || strchr(file, '\n'))
    return;
  int i = find(s, file);
  if (i >= 0) {
    s->values[i] = value;
    return;
  }
  if (s->count == s->cap) {
    int cap = s->cap ? s->cap * 2 : 64;
    char **f = realloc(s->files, (size_t)cap * sizeof *f);
    if (!f)
      return;
    s->files = f;
    long long *v = realloc(s->values, (size_t)cap * sizeof *v);
    if (!v)
      return;
    s->values = v;
    s->cap = cap;
  }
  char *copy = strdup(file);
  if (!copy)
    return;
  s->files[s->count] = copy;
  s->values[s->count++] = value;
}
void library_stamps_remove(struct library_stamps *s, const char *file) {
  int i = find(s, file);
  if (i < 0)
    return;
  free(s->files[i]);
  s->count--;
  memmove(s->files + i, s->files + i + 1, (size_t)(s->count - i) * sizeof *s->files);
  memmove(s->values + i, s->values + i + 1, (size_t)(s->count - i) * sizeof *s->values);
}
bool library_stamps_save(const struct library_stamps *s, const char *root, const char *name) {
  char path[1100], tmp[1200];
  snprintf(path, sizeof path, "%s/%s", root, name);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *f = fopen(tmp, "w");
  if (!f)
    return false;
  bool ok = true;
  for (int i = 0; i < s->count && ok; i++)
    ok = fprintf(f, "%s\t%lld\n", s->files[i], s->values[i]) > 0;
  if (fclose(f) != 0)
    ok = false;
  if (ok)
    ok = pocket_replace(tmp, path);
  if (!ok)
    pocket_remove(tmp);
  return ok;
}
void library_stamps_free(struct library_stamps *s) {
  for (int i = 0; i < s->count; i++)
    free(s->files[i]);
  free(s->files);
  free(s->values);
  *s = (struct library_stamps){0};
}
bool library_stamp(const char *root, const char *name, const char *file, long long value) {
  struct library_stamps s;
  library_stamps_load(&s, root, name);
  if (value)
    library_stamps_put(&s, file, value);
  else
    library_stamps_remove(&s, file);
  bool ok = library_stamps_save(&s, root, name);
  library_stamps_free(&s);
  return ok;
}
