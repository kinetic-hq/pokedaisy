/* SAVE EDITOR (save_edit.h): what it refuses, and, given real games, a whole round of changes on a copy of a save:
 * POCKET_SAVE_EDIT_GBA="<rom>|<save>" and / or POCKET_SAVE_EDIT_NDS="<rom>|<save>" (never committed, never the
 * originals: the test copies the save into a scratch folder first). */
#include "save_edit.h"
#include "pocket_os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

  /* Gen 3's extras: friendship, the first move's PP Ups, and EXP one past the level's least; then refusals. */
  if (again.extras && again.level < 100) {
    long lo = again.exp;
    want = again;
    want.friendship = again.friendship == 200 ? 201 : 200;
    if (want.moves[0])
      want.ppUps[0] = 3;
    want.exp = lo + 1;
    CHECK(o->set_mon(e, SE_PARTY, 0, &want, err, sizeof err) || (fprintf(stderr, "%s\n", err), 0));
    CHECK(o->mon(e, SE_PARTY, 0, &again));
    CHECK(again.friendship == want.friendship && again.ppUps[0] == want.ppUps[0] && again.level == want.level);
    CHECK(again.exp == lo + 1 || (fprintf(stderr, "exp %ld, wanted %ld\n", again.exp, lo + 1), 0));
    bad = again;
    bad.exp = 0;
    CHECK(!o->set_mon(e, SE_PARTY, 0, &bad, err, sizeof err));
    printf("  refused: %s\n", err);
    bad = again;
    bad.friendship = 256;
    CHECK(!o->set_mon(e, SE_PARTY, 0, &bad, err, sizeof err));
    bad = again;
    bad.ppUps[0] = 4;
    CHECK(!o->set_mon(e, SE_PARTY, 0, &bad, err, sizeof err));
  }

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

  /* The game's own pockets (BAG's ADD ANY ITEM): every item it names goes in one pocket or none; one of the third
   * pocket's (the balls in Gen 3) not in the bag yet goes in, and into the first pocket it's refused. */
  int addId = -1, addPocket = o->pockets(e) > 2 ? 2 : 0, addSlot = -1;
  if (o->item_pocket) {
    int sorted = 0;
    for (int id = 1; id <= o->item_max(e); id++) {
      int p = o->item_pocket(e, id);
      CHECK(p >= -1 && p < o->pockets(e));
      if (p < 0)
        continue;
      sorted++;
      bool inBag = false;
      for (int i = 0; i < o->pocket_slots(e, p) && !inBag; i++)
        inBag = o->pocket_item(e, p, i, &it) && it.id == id;
      if (p == addPocket && !inBag && addId < 0)
        addId = id;
    }
    printf("  %d items sorted into pockets\n", sorted);
    CHECK(sorted > 100);
    for (int i = 0; i < o->pocket_slots(e, addPocket) && addSlot < 0; i++)
      if (o->pocket_item(e, addPocket, i, &it) && !it.id)
        addSlot = i;
    if (addId > 0 && addSlot >= 0) {
      int empty0 = -1;
      for (int i = 0; i < o->pocket_slots(e, 0) && empty0 < 0; i++)
        if (o->pocket_item(e, 0, i, &it) && !it.id)
          empty0 = i;
      if (addPocket != 0 && empty0 >= 0)
        CHECK(!o->set_item(e, 0, empty0, addId, 1, err, sizeof err));
      CHECK(o->set_item(e, addPocket, addSlot, addId, 3, err, sizeof err) || (fprintf(stderr, "%s\n", err), 0));
      CHECK(o->pocket_item(e, addPocket, addSlot, &it) && it.id == addId && it.count == 3);
      printf("  added %s x3 to %s\n", o->item_name(e, addId), o->pocket_name(e, addPocket));
    }
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
  if (want.extras && want.level < 100)
    CHECK(again.friendship == want.friendship && again.ppUps[0] == want.ppUps[0] && again.exp == want.exp);
  if (slot >= 0) {
    CHECK(o->pocket_item(e, 0, slot, &it) && it.count == count);
  }
  if (addId > 0 && addSlot >= 0)
    CHECK(o->pocket_item(e, addPocket, addSlot, &it) && it.id == addId && it.count == 3);
  se_close(e);
  remove(backup);
  if (!getenv("POCKET_SAVE_EDIT_KEEP")) /* kept: a headless boot of the game checks it loads */
    remove(save);
  printf("PASS save editor round trip on %s\n", rom);
}

/* TRADE SAVES on copies of two real games' saves (POCKET_SAVE_TRADE="<romA>|<saveA>|<romB>|<saveB>"): A's first
 * Pokemon in its boxes sent to B's first empty slot, both written, read back, then sent home again. */
static bool first_slot(struct save_edit *e, int want, int *box, int *slot) {
  for (int b = 0; b < 14; b++)
    for (int s = 0; s < 30; s++)
      if (se_trade_slot(e, b, s) == want) {
        *box = b, *slot = s;
        return true;
      }
  return false;
}
static void trade_round(const char *spec, const char *scratch) {
  char part[4][1024];
  const char *p = spec;
  for (int i = 0; i < 4; i++) {
    const char *bar = i < 3 ? strchr(p, '|') : p + strlen(p);
    CHECK(bar);
    snprintf(part[i], sizeof part[i], "%.*s", (int)(bar - p), p);
    p = bar + 1;
  }
  char saveA[1100], saveB[1100], err[200] = "";
  snprintf(saveA, sizeof saveA, "%s/trade-test-a.sav", scratch);
  snprintf(saveB, sizeof saveB, "%s/trade-test-b.sav", scratch);
  copy_file(part[1], saveA);
  copy_file(part[3], saveB);
  struct save_edit *a = se_open(saveA, part[0], err, sizeof err), *b = se_open(saveB, part[2], err, sizeof err);
  if (!a || !b)
    fprintf(stderr, "%s\n", err);
  CHECK(a && b && se_trade_ready(a) && se_trade_ready(b));
  CHECK(!se_trade(a, 0, 0, a, 0, 1, true, err, sizeof err));
  int ab, as, bb, bs;
  CHECK(first_slot(a, 1, &ab, &as) && first_slot(b, 0, &bb, &bs));
  char why[200];
  const char *block = se_trade_blocker(a, ab, as, b, why, sizeof why);
  printf("trade: %s box %d slot %d -> %s box %d slot %d%s%s\n", a->ops->title(a), ab + 1, as + 1,
         b->ops->title(b), bb + 1, bs + 1, block ? ": " : "", block ? block : "");
  if (block) {
    CHECK(!se_trade(a, ab, as, b, bb, bs, false, err, sizeof err));
    CHECK(se_trade_slot(a, ab, as) == 1 && se_trade_slot(b, bb, bs) == 0);
  } else {
    struct se_mon sent, got;
    CHECK(a->ops->mon(a, ab, as, &sent));
    CHECK(se_trade(a, ab, as, b, bb, bs, false, err, sizeof err));
    CHECK(se_trade_slot(a, ab, as) == 0 && se_trade_slot(b, bb, bs) == 1);
    CHECK(se_save_both(a, b, err, sizeof err));
    se_close(a);
    se_close(b);
    a = se_open(saveA, part[0], err, sizeof err);
    b = se_open(saveB, part[2], err, sizeof err);
    CHECK(a && b && se_trade_slot(a, ab, as) == 0 && b->ops->mon(b, bb, bs, &got));
    CHECK(got.species == sent.species && got.level == sent.level && !strcmp(got.nick, sent.nick) &&
          !memcmp(got.ivs, sent.ivs, sizeof got.ivs));
    /* Home again. */
    CHECK(se_trade(b, bb, bs, a, ab, as, false, err, sizeof err));
    CHECK(se_save_both(a, b, err, sizeof err));
    printf("PASS trade there and back (%s)\n", got.nick);
  }
  /* A save written behind the trade's back: refused. */
  CHECK(first_slot(a, 1, &ab, &as) && first_slot(b, 0, &bb, &bs));
  if (!se_trade_blocker(a, ab, as, b, why, sizeof why)) {
    CHECK(se_trade(a, ab, as, b, bb, bs, false, err, sizeof err));
    copy_file(part[1], saveA);
    struct save_edit *fresh = se_open(saveA, part[0], err, sizeof err);
    CHECK(fresh);
    fresh->dirty = true;
    se_close(fresh);
    FILE *f = fopen(saveA, "r+b");
    CHECK(f);
    fseek(f, 0x1FFFF, SEEK_SET);
    fputc(0x5A, f);
    fclose(f);
    CHECK(!se_save_both(a, b, err, sizeof err) && strstr(err, "changed"));
    puts("PASS a save changed underneath is left alone");
  }
  se_close(a);
  se_close(b);
  remove(saveA);
  remove(saveB);
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

  /* An edit sets the game's resume states aside (renamed, kept); other games' and the slot states stay. */
  char states[1100], path[1300];
  snprintf(states, sizeof states, "%s/edit-test-states", scratch);
  pocket_mkdir(states);
  const char *files[] = {"My Game.gba.resume.ss", "My Game.nds.melonds.resume.ss", "My Game.gba.ss",
                         "Other.gba.resume.ss"};
  for (int i = 0; i < 4; i++) {
    snprintf(path, sizeof path, "%s/%s", states, files[i]);
    write_bytes(path, "state", 5);
  }
  CHECK(se_set_resume_aside(states, "My Game.gba", 0) == 1);
  CHECK(se_set_resume_aside(states, "My Game.nds", 0) == 1);
  CHECK(se_set_resume_aside(states, "My Game.gba", 0) == 0);
  snprintf(path, sizeof path, "%s/My Game.gba.resume.ss", states);
  CHECK(!pocket_exists(path));
  snprintf(path, sizeof path, "%s/My Game.nds.melonds.resume.ss", states);
  CHECK(!pocket_exists(path));
  snprintf(path, sizeof path, "%s/My Game.gba.ss", states);
  CHECK(pocket_exists(path));
  snprintf(path, sizeof path, "%s/Other.gba.resume.ss", states);
  CHECK(pocket_exists(path));
  int kept = 0;
  for (int i = 0; i < 2; i++) {
    char aside[1400];
    struct tm tm;
    time_t zero = 0;
    pocket_localtime(&zero, &tm);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
    snprintf(aside, sizeof aside, "%s/%s.before-edit-%s", states, files[i], stamp);
    kept += pocket_exists(aside);
    remove(aside);
  }
  CHECK(kept == 2);
  for (int i = 2; i < 4; i++) {
    snprintf(path, sizeof path, "%s/%s", states, files[i]);
    remove(path);
  }
  pocket_rmdir(states);
  CHECK(se_set_resume_aside(states, "My Game.gba", 0) == 0);
  puts("PASS an edit sets the game's resume state aside");
  const char *gba = getenv("POCKET_SAVE_EDIT_GBA"), *nds = getenv("POCKET_SAVE_EDIT_NDS");
  if (gba && *gba)
    round_trip(gba, scratch);
  if (nds && *nds)
    round_trip(nds, scratch);
  const char *trade = getenv("POCKET_SAVE_TRADE");
  if (trade && *trade)
    trade_round(trade, scratch);
  return 0;
}
