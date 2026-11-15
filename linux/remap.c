/* GAME BUTTONS: the keyboard key and the controller button for each of the game's buttons. GPL-3.0, see ../NOTICE. */
#include "remap.h"
#include "pocket_os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set(int *table, int button, int value) {
  if (button < 0 || button >= REMAP_MAX)
    return;
  int old = table[button];
  for (int i = 0; i < REMAP_MAX; i++)
    if (i != button && table[i] == value && value != REMAP_NONE)
      table[i] = old; // the swap: the other button takes this one's old key
  table[button] = value;
}
void remap_set_key(struct remap *r, int button, int key) {
  set(r->key, button, key);
  r->keyOwn = true;
}
void remap_set_pad(struct remap *r, int button, int pad) {
  set(r->pad, button, pad);
  r->padOwn = true;
}
static int from(const int *table, int value, int *out, int max) {
  int n = 0;
  if (value == REMAP_NONE)
    return 0;
  for (int i = 0; i < REMAP_MAX && n < max; i++)
    if (table[i] == value)
      out[n++] = i;
  return n;
}
int remap_from_key(const struct remap *r, int key, int *out, int max) { return from(r->key, key, out, max); }
int remap_from_pad(const struct remap *r, int pad, int *out, int max) { return from(r->pad, pad, out, max); }

static void path_of(const char *root, char *out, size_t size) { snprintf(out, size, "%s/controls.ini", root); }
void remap_load(struct remap *r, const char *root, const int *buttons, const char *const *names, int count) {
  char path[1200], line[256];
  path_of(root, path, sizeof path);
  FILE *f = fopen(path, "r");
  if (!f)
    return;
  int key[REMAP_MAX], pad[REMAP_MAX];
  memcpy(key, r->key, sizeof key);
  memcpy(pad, r->pad, sizeof pad);
  bool keyOwn = false, padOwn = false;
  while (fgets(line, sizeof line, f)) {
    bool isKey = !strncmp(line, "key.", 4), isPad = !strncmp(line, "pad.", 4);
    char *eq = strchr(line, '=');
    if ((!isKey && !isPad) || !eq)
      continue;
    *eq = 0;
    char *end;
    long v = strtol(eq + 1, &end, 10);
    if (end == eq + 1)
      continue;
    for (int i = 0; i < count; i++) {
      if (strcmp(line + 4, names[i]) || buttons[i] < 0 || buttons[i] >= REMAP_MAX)
        continue;
      (isKey ? key : pad)[buttons[i]] = v < 0 ? REMAP_NONE : (int)v;
      (isKey ? &keyOwn : &padOwn)[0] = true;
    }
  }
  fclose(f);
  if (keyOwn) { // a side in the file is the player's whole, else the defaults stay
    memcpy(r->key, key, sizeof key);
    r->keyOwn = true;
  }
  if (padOwn) {
    memcpy(r->pad, pad, sizeof pad);
    r->padOwn = true;
  }
}
bool remap_save(const struct remap *r, const char *root, const int *buttons, const char *const *names, int count) {
  char path[1200], tmp[1300];
  path_of(root, path, sizeof path);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  FILE *f = fopen(tmp, "w");
  if (!f)
    return false;
  fprintf(f, "# PokeDaisy GAME BUTTONS: SDL keycodes (key.) and controller buttons (pad.), -1 = none\n");
  for (int side = 0; side < 2; side++) {
    if (!(side ? r->padOwn : r->keyOwn))
      continue;
    for (int i = 0; i < count; i++)
      if (buttons[i] >= 0 && buttons[i] < REMAP_MAX)
        fprintf(f, "%s.%s=%d\n", side ? "pad" : "key", names[i], (side ? r->pad : r->key)[buttons[i]]);
  }
  bool ok = !ferror(f);
  ok = fclose(f) == 0 && ok;
  if (!ok || !(r->keyOwn || r->padOwn)) {
    pocket_remove(tmp);
    return ok && (pocket_remove(path) || !pocket_exists(path)); // both back to their defaults: no file
  }
  return pocket_replace(tmp, path);
}
