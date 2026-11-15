/* GPL-3.0, see ../NOTICE. remap: GAME BUTTONS' swaps, lookups, and DATA/controls.ini. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pocket_os.h"
#include "remap.h"

static void defaults(struct remap *r) {
  memset(r, 0, sizeof *r);
  for (int i = 0; i < REMAP_MAX; i++)
    r->key[i] = r->pad[i] = REMAP_NONE;
  r->key[0] = 'z', r->key[1] = 'x', r->key[2] = 8;
  r->pad[0] = 0, r->pad[1] = 1, r->pad[2] = REMAP_PAD_LT;
}

int main(void) {
  struct remap r;
  int out[4];
  defaults(&r);
  assert(!r.keyOwn && !r.padOwn);
  assert(remap_from_key(&r, 'z', out, 4) == 1 && out[0] == 0);
  assert(remap_from_key(&r, 'q', out, 4) == 0 && remap_from_key(&r, REMAP_NONE, out, 4) == 0);

  // A gets X's key: B, which had it, takes A's old one (a swap, nothing lost).
  remap_set_key(&r, 0, 'x');
  assert(r.keyOwn && !r.padOwn && r.key[0] == 'x' && r.key[1] == 'z');
  // A new key: nobody else loses theirs.
  remap_set_key(&r, 2, 'p');
  assert(r.key[2] == 'p' && r.key[0] == 'x' && r.key[1] == 'z');
  // The trigger moves to B; B's button goes to the one that had the trigger.
  remap_set_pad(&r, 1, REMAP_PAD_LT);
  assert(r.pad[1] == REMAP_PAD_LT && r.pad[2] == 1 && r.padOwn);
  assert(remap_from_pad(&r, REMAP_PAD_LT, out, 4) == 1 && out[0] == 1);
  remap_set_key(&r, 99, 'k'); // out of range: nothing
  assert(remap_from_key(&r, 'k', out, 4) == 0);

  // The file: written, read back over the defaults; a side never changed stays at the defaults.
  char tmpl[1024];
  snprintf(tmpl, sizeof tmpl, "%s/pd-remap-XXXXXX", pocket_temp_root());
  char *root = pocket_mkdtemp(tmpl);
  assert(root);
  const int buttons[] = {0, 1, 2};
  const char *const names[] = {"A", "B", "SELECT"};
  struct remap keysOnly;
  defaults(&keysOnly);
  remap_set_key(&keysOnly, 1, 'y');
  assert(remap_save(&keysOnly, root, buttons, names, 3));
  struct remap back;
  defaults(&back);
  back.pad[0] = 7; // a default that a file without pad. lines leaves alone
  remap_load(&back, root, buttons, names, 3);
  assert(back.keyOwn && !back.padOwn && back.key[1] == 'y' && back.key[0] == 'z' && back.pad[0] == 7);

  assert(remap_save(&r, root, buttons, names, 3));
  defaults(&back);
  remap_load(&back, root, buttons, names, 3);
  assert(back.keyOwn && back.padOwn && !memcmp(back.key, r.key, 3 * sizeof(int)) &&
         !memcmp(back.pad, r.pad, 3 * sizeof(int)));

  // Both back to the defaults: the file goes.
  struct remap none;
  defaults(&none);
  assert(remap_save(&none, root, buttons, names, 3));
  char path[1200];
  snprintf(path, sizeof path, "%s/controls.ini", root);
  assert(!pocket_exists(path));
  pocket_rmdir(root);
  puts("remap tests passed");
  return 0;
}
