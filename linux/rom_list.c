/* GPL-3.0, see ../NOTICE. */
#include "rom_list.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pocket_os.h"

bool rom_list_playable(const char *name, bool nds) {
  const char *ext = strrchr(name, '.');
  return ext && (!strcasecmp(ext, ".gba") || !strcasecmp(ext, ".gbc") || !strcasecmp(ext, ".gb") ||
                 (nds && !strcasecmp(ext, ".nds")));
}

static bool same_path(const char *a, const char *b) {
#ifdef _WIN32
  // Either separator, any case: Windows' own rules.
  for (; *a && *b; a++, b++) {
    char x = *a == '\\' ? '/' : (char)tolower((unsigned char)*a), y = *b == '\\' ? '/' : (char)tolower((unsigned char)*b);
    if (x != y)
      return false;
  }
  return *a == *b;
#else
  return !strcmp(a, b);
#endif
}

static void add(struct rom_list *l, const char *folder, const char *name) {
  if (l->count >= POCKET_ROM_LIST_MAX)
    return;
  size_t n = strlen(folder) + strlen(name) + 2;
  char *path = malloc(n);
  if (!path)
    return;
  snprintf(path, n, "%s/%s", folder, name);
  for (int i = 0; i < l->count; i++)
    if (same_path(l->paths[i], path)) {
      free(path);
      return;
    }
  if (l->count == l->cap) {
    int cap = l->cap ? l->cap * 2 : 64;
    char **p = realloc(l->paths, cap * sizeof *p);
    if (!p) {
      free(path);
      return;
    }
    l->paths = p;
    char **q = realloc(l->labels, cap * sizeof *q);
    if (!q) {
      free(path);
      return;
    }
    l->labels = q;
    l->cap = cap;
  }
  l->paths[l->count] = path;
  l->labels[l->count] = path + strlen(folder) + 1;
  l->count++;
}

struct scan {
  struct rom_list *list;
  const char *folder;
  bool nds, deeper;
};
static void each(const char *name, void *user) {
  struct scan *s = user;
  if (rom_list_playable(name, s->nds)) {
    add(s->list, s->folder, name);
    return;
  }
  // Not a game: a folder one level down, or nothing (a file can't be listed). Hidden ones (.stfolder, .stversions:
  // a synced folder's) are skipped.
  if (!s->deeper || name[0] == '.')
    return;
  size_t n = strlen(s->folder) + strlen(name) + 2;
  char *sub = malloc(n);
  if (!sub)
    return;
  snprintf(sub, n, "%s/%s", s->folder, name);
  struct scan inner = {s->list, sub, s->nds, false};
  pocket_list_dir(sub, each, &inner);
  free(sub);
}

void rom_list_scan(struct rom_list *l, const char *folder, bool nds) {
  struct scan s = {l, folder, nds, true};
  pocket_list_dir(folder, each, &s);
}

static int by_label(const void *a, const void *b) {
  const char *const *x = a, *const *y = b;
  int c = strcasecmp(x[0], y[0]);
  return c ? c : strcmp(x[1], y[1]);
}

void rom_list_load(struct rom_list *l, const char *root, bool nds) {
  char path[1100];
  snprintf(path, sizeof path, "%s/roms", root);
  rom_list_scan(l, path, nds);
  snprintf(path, sizeof path, "%s/%s", root, POCKET_ROM_FOLDERS_FILE);
  FILE *f = fopen(path, "r");
  if (f) {
    char line[1100];
    while (fgets(line, sizeof line, f)) {
      char *s = line, *e;
      if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        s += 3; // Notepad's UTF-8 mark
      while (isspace((unsigned char)*s))
        s++;
      e = s + strlen(s);
      while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
      if (*s == '"' && e > s + 1 && e[-1] == '"') {
        *--e = 0;
        s++;
      }
      while (e > s + 1 && (e[-1] == '/' || e[-1] == '\\') && e[-2] != ':')
        *--e = 0;
      if (*s && *s != '#')
        rom_list_scan(l, s, nds);
    }
    fclose(f);
  }
  // Sorted as pairs (label, path), so both arrays keep in step.
  char **pairs = malloc(2 * (size_t)l->count * sizeof *pairs);
  if (!pairs)
    return;
  for (int i = 0; i < l->count; i++) {
    pairs[2 * i] = l->labels[i];
    pairs[2 * i + 1] = l->paths[i];
  }
  qsort(pairs, l->count, 2 * sizeof *pairs, by_label);
  for (int i = 0; i < l->count; i++) {
    l->labels[i] = pairs[2 * i];
    l->paths[i] = pairs[2 * i + 1];
  }
  free(pairs);
}

void rom_list_free(struct rom_list *l) {
  for (int i = 0; i < l->count; i++)
    free(l->paths[i]);
  free(l->paths);
  free(l->labels);
  *l = (struct rom_list){0};
}

void rom_save_beside(char *out, size_t size, const char *rom) {
  const char *file = strrchr(rom, '/');
#ifdef _WIN32
  const char *back = strrchr(rom, '\\');
  if (back && (!file || back > file))
    file = back;
#endif
  file = file ? file + 1 : rom;
  const char *dot = strrchr(file, '.');
  int keep = dot && dot != file ? (int)(dot - rom) : (int)strlen(rom);
  snprintf(out, size, "%.*s.sav", keep, rom);
}

bool rom_copy_file(const char *from, const char *to) {
  FILE *in = fopen(from, "rb");
  if (!in)
    return false;
  char tmp[1200];
  snprintf(tmp, sizeof tmp, "%s.tmp", to);
  FILE *out = fopen(tmp, "wb");
  bool ok = out != NULL;
  char buf[65536];
  size_t n;
  while (ok && (n = fread(buf, 1, sizeof buf, in)) > 0)
    ok = fwrite(buf, 1, n, out) == n;
  ok = ok && !ferror(in);
  fclose(in);
  if (out && fclose(out) != 0)
    ok = false;
  if (ok)
    ok = pocket_replace(tmp, to);
  if (!ok)
    pocket_remove(tmp);
  return ok;
}
