/* GPL-3.0, see ../NOTICE. */
#include "rom_list.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_name.h"
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
    char **r = realloc(l->names, cap * sizeof *r);
    if (!r) {
      free(path);
      return;
    }
    l->names = r;
    l->cap = cap;
  }
  l->paths[l->count] = path;
  l->labels[l->count] = path + strlen(folder) + 1;
  char shown[256];
  game_display_name(l->labels[l->count], game_header_is_pokemon(path), shown, sizeof shown);
  size_t len = strlen(shown) + 1;
  l->names[l->count] = malloc(len);
  if (!l->names[l->count]) {
    free(path);
    return;
  }
  memcpy(l->names[l->count], shown, len);
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
  if (!c)
    c = strcasecmp(x[1], y[1]);
  return c ? c : strcmp(x[2], y[2]);
}

int rom_folders_read(const char *root, char folders[][POCKET_FOLDER_MAX], int max) {
  char path[1100];
  snprintf(path, sizeof path, "%s/%s", root, POCKET_ROM_FOLDERS_FILE);
  FILE *f = fopen(path, "r");
  int n = 0;
  if (f) {
    char line[POCKET_FOLDER_MAX + 8];
    while (n < max && fgets(line, sizeof line, f)) {
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
      if (*s && *s != '#' && strlen(s) < POCKET_FOLDER_MAX)
        snprintf(folders[n++], POCKET_FOLDER_MAX, "%s", s);
    }
    fclose(f);
  }
  return n;
}

bool rom_folders_write(const char *root, char folders[][POCKET_FOLDER_MAX], int count) {
  char path[1100], tmp[1200];
  snprintf(path, sizeof path, "%s/%s", root, POCKET_ROM_FOLDERS_FILE);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *f = fopen(tmp, "w");
  if (!f)
    return false;
  fputs("# Game folders PokeDaisy lists, one a line (each searched one level deep).\n", f);
  for (int i = 0; i < count; i++)
    fprintf(f, "%s\n", folders[i]);
  bool ok = fclose(f) == 0 && pocket_replace(tmp, path);
  if (!ok)
    pocket_remove(tmp);
  return ok;
}

// PATH inside FOLDER: FOLDER, a separator, more.
static bool inside(const char *path, const char *folder) {
  size_t n = strlen(folder);
  if (!n || strlen(path) <= n + 1)
    return false;
  char *head = malloc(n + 1);
  if (!head)
    return false;
  memcpy(head, path, n);
  head[n] = 0;
  bool same = same_path(head, folder);
  free(head);
  char last = folder[n - 1], next = path[n];
  return same && (last == '/' || last == '\\' || next == '/' || next == '\\');
}

bool rom_in_added_folder(const char *root, const char *rom) {
  static char folders[POCKET_FOLDERS_MAX][POCKET_FOLDER_MAX];
  int n = rom_folders_read(root, folders, POCKET_FOLDERS_MAX);
  for (int i = 0; i < n; i++)
    if (inside(rom, folders[i]))
      return true;
  return false;
}

void rom_list_load(struct rom_list *l, const char *root, bool nds) {
  char path[1100];
  snprintf(path, sizeof path, "%s/roms", root);
  rom_list_scan(l, path, nds);
  static char folders[POCKET_FOLDERS_MAX][POCKET_FOLDER_MAX];
  int n = rom_folders_read(root, folders, POCKET_FOLDERS_MAX);
  for (int i = 0; i < n; i++)
    rom_list_scan(l, folders[i], nds);
  // RENAME: the player's own names (DATA/names.txt) over the made-up ones.
  for (int i = 0; i < l->count; i++) {
    char own[256];
    if (game_custom_name(root, l->labels[i], own, sizeof own)) {
      char *copy = strdup(own);
      if (copy) {
        free(l->names[i]);
        l->names[i] = copy;
      }
    }
  }
  // Sorted as triples (name, label, path), so the arrays keep in step.
  char **pairs = malloc(3 * (size_t)l->count * sizeof *pairs);
  if (!pairs)
    return;
  for (int i = 0; i < l->count; i++) {
    pairs[3 * i] = l->names[i];
    pairs[3 * i + 1] = l->labels[i];
    pairs[3 * i + 2] = l->paths[i];
  }
  qsort(pairs, l->count, 3 * sizeof *pairs, by_label);
  for (int i = 0; i < l->count; i++) {
    l->names[i] = pairs[3 * i];
    l->labels[i] = pairs[3 * i + 1];
    l->paths[i] = pairs[3 * i + 2];
  }
  free(pairs);
}

void rom_list_free(struct rom_list *l) {
  for (int i = 0; i < l->count; i++) {
    free(l->paths[i]);
    free(l->names[i]);
  }
  free(l->paths);
  free(l->labels);
  free(l->names);
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

static char folderList[POCKET_FOLDERS_MAX][POCKET_FOLDER_MAX];

bool rom_folders_add(const char *root, const char *folder) {
  int n = rom_folders_read(root, folderList, POCKET_FOLDERS_MAX);
  if (n >= POCKET_FOLDERS_MAX || !*folder || strlen(folder) >= POCKET_FOLDER_MAX)
    return false;
  for (int i = 0; i < n; i++)
    if (same_path(folderList[i], folder))
      return false;
  snprintf(folderList[n++], POCKET_FOLDER_MAX, "%s", folder);
  return rom_folders_write(root, folderList, n);
}

bool rom_folders_remove(const char *root, int index) {
  int n = rom_folders_read(root, folderList, POCKET_FOLDERS_MAX);
  if (index < 0 || index >= n)
    return false;
  memmove(folderList[index], folderList[index + 1], (size_t)(n - index - 1) * POCKET_FOLDER_MAX);
  return rom_folders_write(root, folderList, n - 1);
}

struct names {
  char **names;
  int count, cap;
  const char *dir;
  const char *ext; // files ending in it are listed too (NULL = folders only)
};
static void push(struct names *n, const char *s) {
  if (n->count == n->cap) {
    int cap = n->cap ? n->cap * 2 : 32;
    char **p = realloc(n->names, (size_t)cap * sizeof *p);
    if (!p)
      return;
    n->names = p;
    n->cap = cap;
  }
  char *copy = strdup(s);
  if (copy)
    n->names[n->count++] = copy;
}
static void root_each(const char *path, void *user) { push(user, path); }
static void dir_each(const char *name, void *user) {
  struct names *n = user;
  if (name[0] == '.')
    return;
  char path[POCKET_FOLDER_MAX + 300];
  rom_dir_join(path, sizeof path, n->dir, name);
  // EXT may list several, split by '|' (".png|.jpg").
  bool match = false;
  size_t len = strlen(name);
  for (const char *e = n->ext; e && *e && !match;) {
    size_t el = strcspn(e, "|");
    match = el && len > el && !strncasecmp(name + len - el, e, el);
    e += el + (e[el] == '|');
  }
  if (match || pocket_is_dir(path))
    push(n, name);
}
static int by_name(const void *a, const void *b) {
  return strcasecmp(*(char *const *)a, *(char *const *)b);
}

int rom_dir_entries(const char *dir, char ***out) { return rom_dir_entries_with(dir, NULL, out); }

int rom_dir_entries_with(const char *dir, const char *ext, char ***out) {
  struct names n = {.dir = dir, .ext = ext};
  if (!*dir)
    pocket_roots(root_each, &n);
  else {
    pocket_list_dir(dir, dir_each, &n);
    qsort(n.names, (size_t)n.count, sizeof *n.names, by_name);
  }
  *out = n.names;
  return n.count;
}

void rom_dir_entries_free(char **names, int count) {
  for (int i = 0; i < count; i++)
    free(names[i]);
  free(names);
}

static bool separator(char c) {
#ifdef _WIN32
  return c == '/' || c == '\\';
#else
  return c == '/';
#endif
}

void rom_dir_join(char *out, size_t size, const char *dir, const char *name) {
  size_t n = strlen(dir);
#ifdef _WIN32
  const char *sep = "\\";
#else
  const char *sep = "/";
#endif
  snprintf(out, size, "%s%s%s", dir, n && separator(dir[n - 1]) ? "" : sep, name);
}

void rom_dir_up(char *dir) {
  size_t n = strlen(dir);
  while (n > 1 && separator(dir[n - 1]) && dir[n - 2] != ':')
    dir[--n] = 0;
  // A root ("C:\", "/") goes back to the top.
  if ((n == 1 && separator(dir[0])) || (n == 3 && dir[1] == ':' && separator(dir[2])) || (n == 2 && dir[1] == ':')) {
    dir[0] = 0;
    return;
  }
  char *cut = NULL;
  for (char *p = dir; *p; p++)
    if (separator(*p))
      cut = p;
  if (!cut) {
    dir[0] = 0;
    return;
  }
  // Keep a root's separator: "C:\Games" -> "C:\", "/home" -> "/".
  if (cut == dir || (cut == dir + 2 && dir[1] == ':'))
    cut[1] = 0;
  else
    *cut = 0;
}
