/* SAVE EDITOR (save_edit.h): what it refuses, and, given real games, a whole round of changes on a copy of a save:
 * POCKET_SAVE_EDIT_GBA="<rom>|<save>" and / or POCKET_SAVE_EDIT_NDS="<rom>|<save>" (never committed, never the
 * originals: the test copies the save into a scratch folder first). */
#include "save_edit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                                                                       \
  do {                                                                                                                 \
    if (!(x)) {                                                                                                        \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                                          \
      exit(1);                                                                                                         \
    }                                                                                                                  \
  } while (0)

static void write_bytes(const char *path, const void *data, size_t n) {
  FILE *f = fopen(path, "wb");
  CHECK(f && fwrite(data, 1, n, f) == n);
  fclose(f);
}
static void copy_file(const char *from, const char *to) {
  FILE *f = fopen(from, "rb");
  CHECK(f);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *b = malloc((size_t)n);
  CHECK(b && fread(b, 1, (size_t)n, f) == (size_t)n);
  fclose(f);
  write_bytes(to, b, (size_t)n);
  free(b);
}

static void round_trip(const char *spec, const char *scratch) {
  char rom[1024], save[1100];
  const char *bar = strchr(spec, '|');
  CHECK(bar);
  snprintf(rom, sizeof rom, "%.*s", (int)(bar - spec), spec);
  snprintf(save, sizeof save, "%s/edit-test-%s.sav", scratch, strstr(rom, ".nds") ? "nds" : "gba");
  copy_file(bar + 1, save);
  char err[200] = "";
  struct save_edit *e = se_open(save, rom, err, sizeof err);
  if (!e) {
    fprintf(stderr, "%s\n", err);
    CHECK(0);
  }
  const struct se_ops *o = e->ops;
  printf("%s: money %ld\n", o->title(e), o->money(e));
  long money = o->money(e) == 1234 ? 4321 : 1234;
  CHECK(o->set_money(e, money, err, sizeof err));
  CHECK(!o->set_money(e, o->money_max(e) + 1, err, sizeof err));

  /* The party's first Pokemon: a new level, nature, IVs and a nickname; then refusals. */
  struct se_mon m, again;
  CHECK(o->mon(e, SE_PARTY, 0, &m));
  printf("  lead %s Lv%d %s\n", o->species_name(e, m.species), m.level, se_nature_name(m.nature));
  struct se_mon want = m;
  want.level = m.level < 100 ? m.level + 1 : 99;
  want.nature = (m.nature + 3) % 25;
  for (int i = 0; i < 6; i++)
    want.ivs[i] = 31;
  snprintf(want.nick, sizeof want.nick, "DAISY");
  CHECK(o->set_mon(e, SE_PARTY, 0, &want, err, sizeof err) || (fprintf(stderr, "%s\n", err), 0));
  CHECK(o->mon(e, SE_PARTY, 0, &again));
  CHECK(again.level == want.level && again.nature == want.nature && !strcmp(again.nick, "DAISY"));
  CHECK(again.ivs[5] == 31 && again.species == m.species && again.shiny == m.shiny);
  CHECK(again.maxHp > 0 && again.hp <= again.maxHp);
  struct se_mon bad = again;
  bad.moves[1] = bad.moves[0];
  CHECK(!o->set_mon(e, SE_PARTY, 0, &bad, err, sizeof err));
  bad = again;
  bad.evs[0] = bad.evs[1] = bad.evs[2] = 255;
  CHECK(!o->set_mon(e, SE_PARTY, 0, &bad, err, sizeof err));
  bad = again;
  bad.species = o->species_max(e) + 1;
  CHECK(!o->set_mon(e, SE_PARTY, 0, &bad, err, sizeof err));
  bad = again;
  bad.level = 101;
  CHECK(!o->set_mon(e, SE_PARTY, 0, &bad, err, sizeof err));

  /* A stack in the first pocket: one fewer (or one more). */
  struct se_item it;
  int slot = -1;
  for (int i = 0; i < o->pocket_slots(e, 0) && slot < 0; i++)
    if (o->pocket_item(e, 0, i, &it) && it.id)
      slot = i;
  int count = -1;
  if (slot >= 0) {
    count = it.count > 1 ? it.count - 1 : 2;
    CHECK(o->set_item(e, 0, slot, it.id, count, err, sizeof err));
    CHECK(!o->set_item(e, 0, slot, it.id, o->item_max_count(e, 0, it.id) + 1, err, sizeof err));
  }
  for (int p = 0; p < o->pockets(e); p++)
    if (o->pocket_locked(e, p)) {
      CHECK(!o->set_item(e, p, 0, 0, 0, err, sizeof err));
      break;
    }
  o->heal(e);

  char backup[1100];
  CHECK(se_save(e, backup, sizeof backup, err, sizeof err) || (fprintf(stderr, "%s\n", err), 0));
  se_close(e);
  FILE *b = fopen(backup, "rb");
  CHECK(b);
  fclose(b);

  e = se_open(save, rom, err, sizeof err);
  CHECK(e);
  o = e->ops;
  CHECK(o->money(e) == money);
  CHECK(o->mon(e, SE_PARTY, 0, &again));
  CHECK(again.level == want.level && again.nature == want.nature && !strcmp(again.nick, "DAISY"));
  CHECK(again.hp == again.maxHp);
  if (slot >= 0) {
    CHECK(o->pocket_item(e, 0, slot, &it) && it.count == count);
  }
  se_close(e);
  remove(backup);
  if (!getenv("POCKET_SAVE_EDIT_KEEP")) /* kept: a headless boot of the game checks it loads */
    remove(save);
  printf("PASS save editor round trip on %s\n", rom);
}

int main(void) {
#ifdef POCKET_SAVE_EDIT_DS
  se_gen4_opener = se_open_gen4;
#endif
  const char *scratch = getenv("TEMP") ? getenv("TEMP") : "/tmp";
  char rom[1100], save[1100], err[200];
  snprintf(rom, sizeof rom, "%s/not-a-game.gba", scratch);
  snprintf(save, sizeof save, "%s/not-a-game.sav", scratch);
  static unsigned char zeros[0x20000];
  write_bytes(rom, zeros, 0x10000);
  write_bytes(save, zeros, sizeof zeros);
  CHECK(!se_open(save, rom, err, sizeof err) && *err);
  char missing[1200];
  snprintf(missing, sizeof missing, "%s/no-such-save.sav", scratch);
  err[0] = 0;
  CHECK(!se_open(missing, rom, err, sizeof err) && strstr(err, "save"));
  snprintf(rom, sizeof rom, "%s/not-a-game.nds", scratch);
  write_bytes(rom, zeros, 0x10000);
  err[0] = 0;
  CHECK(!se_open(save, rom, err, sizeof err) && *err);
  remove(rom);
  remove(save);
  puts("PASS save editor refuses what it doesn't know");
  const char *gba = getenv("POCKET_SAVE_EDIT_GBA"), *nds = getenv("POCKET_SAVE_EDIT_NDS");
  if (gba && *gba)
    round_trip(gba, scratch);
  if (nds && *nds)
    round_trip(nds, scratch);
  return 0;
}
