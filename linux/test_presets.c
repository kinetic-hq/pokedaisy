/* PRESETS (presets.c): Showdown text in both spellings, CSV, both JSON shapes, one set kept once across files, and
 * (with POCKET_SAVE_EDIT_GBA="<rom>|<save>") a set filled into a copy of a real save's lead. */
#include "presets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pocket_os.h"
#include "stat_tools.h"

#define CHECK(c)                                                                                                       \
  do {                                                                                                                 \
    if (!(c)) {                                                                                                        \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                    \
      exit(1);                                                                                                         \
    }                                                                                                                  \
  } while (0)

static int parse(const char *name, const char *text, int gen, struct preset_list *out) {
  return presets_parse(name, text, strlen(text), gen, out);
}

static void write_file(const char *path, const char *text) {
  FILE *f = fopen(path, "wb");
  CHECK(f);
  fputs(text, f);
  fclose(f);
}

static const char *TEXT_SHOWDOWN = "Fluffy (Milotic) (F) @ Leftovers\r\n"
                                   "Ability: Marvel Scale\r\n"
                                   "EVs: 252 HP / 252 Def / 4 SpA\r\n"
                                   "Bold Nature\r\n"
                                   "IVs: 0 Atk\r\n"
                                   "- Surf\r\n"
                                   "- Recover / Toxic\r\n"
                                   "- Toxic\r\n"
                                   "- Hidden Power [Grass]\r\n"
                                   "\r\n"
                                   "Salamence @ Choice Band\n"
                                   "Adamant Nature\n"
                                   "- Dragon Claw\n"
                                   "- Earthquake\n";

static const char *TEXT_HEADERS = "=== [Gen 3 OU] Milotic [Standard] ===\n"
                                  "Milotic\n"
                                  "EVs: hp: 252; def: 252; spa: 4\n"
                                  "Bold Nature\n"
                                  "- Surf\n"
                                  "- Recover\n"
                                  "- Toxic\n"
                                  "- Refresh\n"
                                  "Item: Leftovers\n"
                                  "\n"
                                  "=== [Gen 4 UU] Milotic [Bulky] ===\n"
                                  "Milotic\n"
                                  "IVs: atk: 2; spa: 30\n"
                                  "Calm Nature\n"
                                  "- Surf\n"
                                  "Item: Leftovers\n";

static const char *CSV = "generation,format,pokemon,set_name,item,ability,tera_type,nature,evs,ivs,moves,source_url\n"
                         "3,Gen 3 OU,Milotic,Standard,Leftovers,,,Bold,hp: 252; def: 252; spa: 4,,"
                         "Surf; Recover; Toxic; Refresh,\n"
                         "9,Gen 9 OU,Milotic,\"Calm, Mind\",Leftovers,Competitive,Fairy,Bold,hp: 252,,\"Scald; Recover\",\n";

static const char *JSON_LIST = "[ {\"generation\": 3, \"format\": \"Gen 3 OU\", \"pokemon\": \"Milotic\", "
                               "\"set_name\": \"Standard\", \"item\": \"Leftovers\", \"ability\": \"\", \"nature\": \"Bold\","
                               " \"evs\": \"hp: 252; def: 252; spa: 4\", \"ivs\": \"\", "
                               "\"moves\": \"Surf; Recover; Toxic; Refresh\"} ]";

static const char *JSON_SMOGON = "{\"Ivysaur\": {\"pu\": {\"Specially Defensive\": {\"moves\": [\"Leech Seed\", "
                                 "[\"Sleep Powder\", \"Toxic\"], \"Synthesis\", \"Hidden Power Grass\"], \"item\": "
                                 "\"Leftovers\", \"nature\": \"Calm\", \"ivs\": {\"atk\": 2, \"spa\": 30}, \"evs\": "
                                 "{\"hp\": 252, \"spd\": 224, \"spe\": 32}}}}}";

static void parsers(void) {
  struct preset_list l = {0};
  CHECK(parse("paste.txt", TEXT_SHOWDOWN, 3, &l) == 2);
  const struct preset *p = &l.v[0];
  CHECK(!strcmp(p->species, "Milotic") && !strcmp(p->item, "Leftovers") && !strcmp(p->ability, "Marvel Scale"));
  CHECK(p->gen == 3 && !strcmp(p->nature, "Bold"));
  CHECK(p->evs[0] == 252 && p->evs[2] == 252 && p->evs[4] == 4 && p->evs[1] == 0);
  CHECK(p->ivs[1] == 0 && p->ivSet[1] && p->ivs[0] == 31 && !p->ivSet[0]);
  CHECK(!strcmp(p->moves[1], "Recover") && !strcmp(p->moves[3], "Hidden Power  Grass"));
  CHECK(!strcmp(l.v[1].species, "Salamence") && !strcmp(l.v[1].item, "Choice Band") && !l.v[1].moves[2][0]);
  presets_free(&l);

  CHECK(parse("sets.txt", TEXT_HEADERS, 0, &l) == 2);
  CHECK(l.v[0].gen == 3 && !strcmp(l.v[0].format, "Gen 3 OU") && !strcmp(l.v[0].name, "Standard"));
  CHECK(!strcmp(l.v[0].species, "Milotic") && !strcmp(l.v[0].item, "Leftovers") && !strcmp(l.v[0].moves[3], "Refresh"));
  CHECK(l.v[1].gen == 4 && l.v[1].ivs[1] == 2 && l.v[1].ivs[4] == 30 && l.v[1].ivs[5] == 31);
  presets_free(&l);

  CHECK(parse("sets.csv", CSV, 0, &l) == 2);
  CHECK(l.v[0].gen == 3 && !strcmp(l.v[0].moves[2], "Toxic") && l.v[0].evs[2] == 252);
  CHECK(l.v[1].gen == 9 && !strcmp(l.v[1].name, "Calm, Mind") && !strcmp(l.v[1].tera, "Fairy"));
  CHECK(!strcmp(l.v[1].moves[0], "Scald") && !strcmp(l.v[1].moves[1], "Recover"));
  presets_free(&l);

  CHECK(parse("sets.json", JSON_LIST, 0, &l) == 1);
  CHECK(l.v[0].gen == 3 && !strcmp(l.v[0].moves[3], "Refresh") && l.v[0].evs[4] == 4);
  presets_free(&l);

  CHECK(parse("gen3.json", JSON_SMOGON, 0, &l) == 1);
  p = &l.v[0];
  CHECK(p->gen == 3 && !strcmp(p->species, "Ivysaur") && !strcmp(p->format, "PU") &&
        !strcmp(p->name, "Specially Defensive"));
  CHECK(!strcmp(p->moves[1], "Sleep Powder") && p->ivs[1] == 2 && p->ivSet[4] && p->evs[5] == 224 && p->evs[3] == 32);
  presets_free(&l);
  /* GET SETS' files (the pkmn project's genN.json): the tera type as teraType, an item given as alternatives. */
  CHECK(parse("gen9.json",
              "{\"Ivysaur\": {\"ou\": {\"Bulky\": {\"moves\": [\"Synthesis\"], \"item\": [\"Leftovers\", \"Black "
              "Sludge\"], \"teraType\": \"Water\"}}}}",
              0, &l) == 1);
  CHECK(l.v[0].gen == 9 && !strcmp(l.v[0].tera, "Water") && !strcmp(l.v[0].item, "Leftovers"));
  presets_free(&l);
  printf("PASS presets read Showdown text (both spellings), CSV and both JSON shapes\n");
}

static void folder(const char *scratch) {
  char dir[1024], path[1100];
  snprintf(dir, sizeof dir, "%s/presets-test", scratch);
  CHECK(pocket_mkdir(dir));
  snprintf(path, sizeof path, "%s/a.txt", dir);
  write_file(path, TEXT_HEADERS);
  snprintf(path, sizeof path, "%s/b.csv", dir);
  write_file(path, CSV);
  snprintf(path, sizeof path, "%s/c.json", dir);
  write_file(path, JSON_LIST);
  struct preset_list l = {0};
  /* Gen 3 OU Standard in all three files: once. Gen 4 Bulky and Gen 9 Calm, Mind once each. */
  CHECK(presets_load_dir(dir, &l) == 3 || (fprintf(stderr, "%d sets\n", l.count), 0));
  presets_free(&l);

  char msg[160];
  char src[1100];
  snprintf(src, sizeof src, "%s/new-sets.txt", scratch);
  write_file(src, TEXT_SHOWDOWN);
  CHECK(presets_import(src, dir, msg, sizeof msg) || (fprintf(stderr, "%s\n", msg), 0));
  printf("  %s\n", msg);
  CHECK(presets_load_dir(dir, &l) == 5);
  presets_free(&l);
  CHECK(!presets_import(src, dir, msg, sizeof msg)); /* nothing new the second time */
  snprintf(path, sizeof path, "%s/new-sets.txt", dir);
  pocket_remove(path);
  remove(src);
  for (const char *f = "a.txt\0b.csv\0c.json\0"; *f; f += strlen(f) + 1) {
    snprintf(path, sizeof path, "%s/%s", dir, f);
    pocket_remove(path);
  }
  pocket_rmdir(dir);
  printf("PASS presets load a folder once per set and import a file\n");
}

static void fill(const char *spec, const char *scratch) {
  char rom[1024], save[1100], err[200] = "";
  const char *bar = strchr(spec, '|');
  CHECK(bar);
  snprintf(rom, sizeof rom, "%.*s", (int)(bar - spec), spec);
  snprintf(save, sizeof save, "%s/presets-test.sav", scratch);
  FILE *in = fopen(bar + 1, "rb"), *out = fopen(save, "wb");
  CHECK(in && out);
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, in)) > 0)
    fwrite(buf, 1, n, out);
  fclose(in);
  fclose(out);
  struct save_edit *e = se_open(save, rom, err, sizeof err);
  CHECK(e || (fprintf(stderr, "%s\n", err), 0));
  struct se_mon m;
  CHECK(e->ops->mon(e, SE_PARTY, 0, &m));
  char lead[32];
  snprintf(lead, sizeof lead, "%s", e->ops->species_name(e, m.species));
  /* A set for the lead: one move it can't learn, one this game doesn't have, Hidden Power Grass with open IVs. */
  char text[600];
  snprintf(text, sizeof text,
           "%s @ Leftovers\nAbility: Nonexistent\nTera Type: Fire\nEVs: 252 Atk / 4 Def / 252 Spe\nJolly Nature\n"
           "- Earthquake\n- Pound\n- Moonblast\n- Hidden Power [Grass]\n",
           lead);
  struct preset_list l = {0};
  CHECK(presets_parse("paste.txt", text, strlen(text), 3, &l) == 1);
  struct preset_notes notes;
  struct se_mon want = m;
  int filled = preset_apply(e, &l.v[0], &want, &notes);
  printf("  %s: %d filled, %d notes\n", lead, filled, notes.count);
  for (int i = 0; i < notes.count; i++)
    printf("    %s\n", notes.line[i]);
  CHECK(filled >= 5 && notes.count == 4); /* Pound, Moonblast, the ability, the Tera Type */
  CHECK(!strcmp(se_nature_name(want.nature), "JOLLY") || !strcmp(se_nature_name(want.nature), "Jolly"));
  CHECK(want.evs[1] == 252 && want.evs[3] == 252 && want.evs[2] == 4 && want.evs[0] == 0);
  int bits = 0;
  for (int i = 0; i < 6; i++)
    bits |= (want.ivs[i] & 1) << i;
  CHECK(bits * 15 / 63 == 10); /* Grass */
  bool quake = false, hidden = false;
  for (int i = 0; i < 4; i++) {
    const char *mv = want.moves[i] ? e->ops->move_name(e, want.moves[i]) : "";
    quake |= !strcmp(mv, "EARTHQUAKE");
    hidden |= !strcmp(mv, "HIDDEN POWER");
  }
  CHECK(quake && hidden);
  CHECK(e->ops->set_mon(e, SE_PARTY, 0, &want, err, sizeof err) || (fprintf(stderr, "%s\n", err), 0));
  presets_free(&l);
  se_close(e);
  remove(save);
  printf("PASS a preset fills a real save's lead and names what the game can't do\n");
}

int main(void) {
  {
    /* STATS: the game's formula, the natures' +/-, the roles and the suggested natures. */
    enum { JOLLY = 13, ADAMANT = 3, TIMID = 10, HARDY = 0, MODEST = 15 };
    CHECK(st_nature_up(JOLLY) == 3 && st_nature_down(JOLLY) == 4); // +Speed -Sp. Atk
    CHECK(st_nature_up(ADAMANT) == 1 && st_nature_down(ADAMANT) == 4);
    CHECK(st_nature_up(TIMID) == 3 && st_nature_down(TIMID) == 1);
    CHECK(!st_nature_up(HARDY) && st_nature_of(3, 4) == JOLLY);
    char fx[32];
    CHECK(!strcmp(st_nature_effect(JOLLY, fx, sizeof fx), "+SPE -SP. ATK"));
    const int base[6] = {108, 130, 95, 102, 80, 85}, ivs[6] = {31, 31, 31, 31, 31, 31}, evs[6] = {4, 252, 0, 252, 0, 0};
    int out[6];
    st_stats(base, ivs, evs, 100, JOLLY, out);
    CHECK(out[0] == 358 && out[1] == 359 && out[3] == 333 && out[4] == 176); // the numbers a damage calculator gives
    CHECK(st_stat_max(base, 3, 100, JOLLY) == 333 && st_stat_min(base, 3, 100, HARDY) == 209);
    const int shedinja[6] = {1, 90, 45, 40, 30, 30};
    st_stats(shedinja, ivs, evs, 100, HARDY, out);
    CHECK(out[0] == 1);
    CHECK(st_best_role(base) == ROLE_PHYS_SWEEPER);
    const int special[6] = {80, 50, 65, 110, 135, 95}, slow[6] = {100, 135, 120, 30, 60, 80}, wall[6] = {120, 60, 60, 40,
                                                                                                         60, 130};
    CHECK(st_best_role(special) == ROLE_SPEC_SWEEPER && st_best_role(slow) == ROLE_TRICK_ROOM);
    CHECK(st_best_role(wall) == ROLE_SPEC_TANK);
    int spread[6], nature;
    st_role_spread(ROLE_TRICK_ROOM, slow, spread, &nature);
    CHECK(spread[3] == 0 && spread[1] == 252 && nature == 2); // Brave: +Attack -Speed
    char text[64];
    st_role_spread(ROLE_PHYS_SWEEPER, base, spread, &nature);
    CHECK(!strcmp(st_spread_text(spread, text, sizeof text), "252 ATK / 252 SPE / 4 HP") && nature == JOLLY);
    for (int r = 0; r < ROLE_COUNT; r++) {
      st_role_spread(r, base, spread, &nature);
      int total = 0;
      for (int i = 0; i < 6; i++) {
        total += spread[i];
        CHECK(spread[i] <= 252);
      }
      CHECK(total <= 510 && nature >= 0 && nature < 25);
    }
    int rec[3];
    bool fromSets[3];
    CHECK(st_recommend(base, NULL, rec, fromSets) >= 2 && rec[0] == JOLLY && rec[1] == ADAMANT && !fromSets[0]);
    int counts[25] = {0};
    counts[MODEST] = 1, counts[ADAMANT] = 5;
    int n = st_recommend(base, counts, rec, fromSets);
    CHECK(n == 3 && rec[0] == ADAMANT && fromSets[0] && rec[1] == MODEST && rec[2] == JOLLY && !fromSets[2]);
    puts("PASS STATS: the formula, natures, roles and suggested natures");
  }
  {
    /* QUICK SET's Hidden Power: every type reachable from perfect IVs with each IV moved by one at most, at 70 power
     * where it can be; DARK is all 31s. */
    int all31[6] = {31, 31, 31, 31, 31, 31};
    CHECK(preset_hp_type(all31) == 15 && preset_hp_power(all31) == 70);
    CHECK(!strcmp(preset_hp_type_name(8), "FIRE"));
    for (int type = 0; type < 16; type++) {
      int iv[6] = {31, 31, 31, 31, 31, 31};
      preset_hp_set(iv, type);
      CHECK(preset_hp_type(iv) == type);
      for (int i = 0; i < 6; i++)
        CHECK(iv[i] == 30 || iv[i] == 31);
      CHECK(preset_hp_power(iv) == 70);
    }
    int low[6] = {31, 0, 31, 31, 31, 31}; // a special attacker's 0 Attack stays low
    preset_hp_set(low, 13);               // ICE
    CHECK(preset_hp_type(low) == 13 && low[1] <= 1);
    puts("PASS QUICK SET's Hidden Power picker");
  }
  {
    /* COPY WHAT?: STATS ONLY takes the set's nature, IVs and EVs and keeps the Pokemon's own moves and item. */
    struct se_mon m = {0}, next = {0};
    m.species = next.species = 25, m.nature = 3, m.held = 7, m.moves[0] = 85, m.ivs[0] = 1, m.evs[3] = 10;
    next.nature = 10, next.held = 9, next.moves[0] = 57, next.ivs[0] = 31, next.evs[3] = 252, next.ability = 1;
    struct se_mon a = m;
    preset_take(&a, &next, PART_STATS);
    CHECK(a.nature == 10 && a.ivs[0] == 31 && a.evs[3] == 252 && a.held == 7 && a.moves[0] == 85 && a.ability == 0);
    a = m;
    preset_take(&a, &next, PART_MOVES | PART_ITEM);
    CHECK(a.nature == 3 && a.moves[0] == 57 && a.held == 9 && a.ivs[0] == 1);
    a = m;
    preset_take(&a, &next, PART_ALL);
    CHECK(a.nature == 10 && a.moves[0] == 57 && a.ability == 1);
    puts("PASS COPY WHAT? takes only the parts picked");
  }
  const char *scratch = getenv("TEMP") ? getenv("TEMP") : "/tmp";
  parsers();
  folder(scratch);
  const char *gba = getenv("POCKET_SAVE_EDIT_GBA");
  if (gba && *gba)
    fill(gba, scratch);
  return 0;
}
