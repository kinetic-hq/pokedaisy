/* SAVE EDITOR's Gen 3 part and the file handling every game shares (save_edit.h). The save format is the Android
 * app's Gen3SaveFile.kt / Gen3BoxMon.kt (pokeruby / pokeemerald / pokefirered's save.c and global.h); the tables are
 * the companion's (3ds/core), read for the ROM the save belongs to. */
#include "save_edit.h"
#include "save_dex.h"
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pd_dex.h"
#include "pd_experience.h"
#include "pd_game.h"
#include "pd_snapshot.h"
#include "pd_tables.h"
#include "pocket_os.h"
#include "save_tables_gen.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* ---- shared ---- */

static void say(char *err, size_t n, const char *why) {
  if (err && n)
    snprintf(err, n, "%s", why);
}

const char *se_nature_name(int nature) {
  return nature >= 0 && nature < pd_nature_names_count ? pd_nature_names[nature] : "?";
}

static uint8_t *read_file(const char *path, size_t *size, size_t max) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  uint8_t *data = NULL;
  if (!fseek(f, 0, SEEK_END)) {
    long n = ftell(f);
    if (n > 0 && (size_t)n <= max && !fseek(f, 0, SEEK_SET) && (data = malloc((size_t)n))) {
      if (fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        data = NULL;
      } else
        *size = (size_t)n;
    }
  }
  fclose(f);
  return data;
}

static bool write_file(const char *path, const uint8_t *data, size_t size) {
  FILE *f = fopen(path, "wb");
  if (!f)
    return false;
  bool ok = fwrite(data, 1, size, f) == size;
  ok = fflush(f) == 0 && ok;
  return fclose(f) == 0 && ok;
}

static bool replace_file(const char *tmp, const char *path) {
#ifdef _WIN32
  wchar_t wt[1100], wp[1100];
  if (!MultiByteToWideChar(CP_UTF8, 0, tmp, -1, wt, 1100) || !MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, 1100))
    return false;
  return MoveFileExW(wt, wp, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return rename(tmp, path) == 0;
#endif
}

static bool ends_with(const char *s, const char *suffix) {
  size_t a = strlen(s), b = strlen(suffix);
  if (b > a)
    return false;
  for (size_t i = 0; i < b; i++) {
    char x = s[a - b + i], y = suffix[i];
    if (x >= 'A' && x <= 'Z')
      x = (char)(x - 'A' + 'a');
    if (x != y)
      return false;
  }
  return true;
}

struct save_edit *se_open(const char *save, const char *rom, char *err, size_t errSize) {
  size_t saveSize = 0, romSize = 0;
  uint8_t *bytes = read_file(save, &saveSize, 2 * 1024 * 1024);
  if (!bytes) {
    say(err, errSize, "There's no save for this game yet: play it and save once first");
    return NULL;
  }
  const bool ds = ends_with(rom, ".nds");
  uint8_t *romBytes = read_file(rom, &romSize, (ds ? 512u : 64u) * 1024 * 1024);
  if (!romBytes) {
    free(bytes);
    say(err, errSize, "Couldn't read the game file");
    return NULL;
  }
  struct save_edit *e = NULL;
  if (!ds)
    e = se_open_gen3(bytes, saveSize, romBytes, romSize, err, errSize);
  else if (se_gen4_opener)
    e = se_gen4_opener(bytes, saveSize, romBytes, romSize, err, errSize);
  else
    say(err, errSize, "This build has no DS games");
  free(bytes);
  if (!e) {
    free(romBytes);
    return NULL;
  }
  snprintf(e->path, sizeof e->path, "%s", save);
  return e;
}

void se_close(struct save_edit *e) {
  if (e && e->ops)
    e->ops->free(e);
}

struct resume_scan {
  const char *prefix;
  char names[16][512];
  int count;
};
static void resume_each(const char *name, void *user) {
  struct resume_scan *s = user;
  size_t n = strlen(name), p = strlen(s->prefix), tail = strlen(".resume.ss");
  if (s->count < 16 && n >= p + tail && n < sizeof s->names[0] && !strncmp(name, s->prefix, p) &&
      !strcmp(name + n - tail, ".resume.ss"))
    snprintf(s->names[s->count++], sizeof s->names[0], "%s", name);
}
int se_set_resume_aside(const char *statesDir, const char *romFile, time_t now) {
  struct resume_scan s = {romFile, {{0}}, 0};
  if (!pocket_list_dir(statesDir, resume_each, &s))
    return 0; /* no states folder: nothing to set aside */
  struct tm tm;
  pocket_localtime(&now, &tm);
  char stamp[32];
  strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
  int moved = 0;
  for (int i = 0; i < s.count; i++) {
    char from[1100], to[1200];
    snprintf(from, sizeof from, "%s/%s", statesDir, s.names[i]);
    snprintf(to, sizeof to, "%s.before-edit-%s", from, stamp);
    if (!pocket_replace(from, to))
      return -1;
    moved++;
  }
  return moved;
}

bool se_save(struct save_edit *e, char *backup, size_t backupSize, char *err, size_t errSize) {
  uint8_t *out = NULL;
  size_t size = 0;
  if (!e->ops->build(e, &out, &size, err, errSize))
    return false;
  size_t oldSize = 0;
  uint8_t *old = read_file(e->path, &oldSize, 2 * 1024 * 1024);
  if (!old) {
    free(out);
    say(err, errSize, "Couldn't read the save again; nothing written");
    return false;
  }
  char stamp[32], bak[1100], tmp[1100];
  time_t now = time(NULL);
  struct tm tm;
#ifdef _WIN32
  localtime_s(&tm, &now);
#else
  localtime_r(&now, &tm);
#endif
  strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
  snprintf(bak, sizeof bak, "%s.before-edit-%s.sav", e->path, stamp);
  size_t backSize = 0;
  uint8_t *back = NULL;
  bool backed = write_file(bak, old, oldSize) && (back = read_file(bak, &backSize, 2 * 1024 * 1024)) &&
                backSize == oldSize && !memcmp(back, old, oldSize);
  free(back);
  free(old);
  if (!backed) {
    free(out);
    say(err, errSize, "Couldn't back the save up; nothing written");
    return false;
  }
  snprintf(tmp, sizeof tmp, "%s.tmp", e->path);
  bool ok = write_file(tmp, out, size) && replace_file(tmp, e->path);
  free(out);
  if (!ok) {
    remove(tmp);
    say(err, errSize, "Couldn't write the save; the old one is untouched");
    return false;
  }
  size_t againSize = 0;
  uint8_t *again = read_file(e->path, &againSize, 2 * 1024 * 1024);
  ok = again && e->ops->verify(e, again, againSize, err, errSize);
  free(again);
  if (!ok) {
    if (!err || !*err)
      say(err, errSize, "The written save didn't read back");
    /* Put the backup back: a write that can't be proven isn't kept. */
    size_t bs = 0;
    uint8_t *b = read_file(bak, &bs, 2 * 1024 * 1024);
    if (b) {
      write_file(tmp, b, bs);
      replace_file(tmp, e->path);
      free(b);
    }
    return false;
  }
  e->dirty = false;
  if (backup && backupSize)
    snprintf(backup, backupSize, "%s", bak);
  return true;
}

/* ---- Gen 3 ---- */

#define SECTOR 0x1000
#define DATA 0xF80
#define SECTORS 14
#define ID_OFF 0xFF4
#define SUM_OFF 0xFF6
#define SIG_OFF 0xFF8
#define COUNT_OFF 0xFFC
#define SIGNATURE 0x08012025u
#define STORAGE_SIZE 0x83D0
#define BOXES 14
#define BOX_SLOTS 30
#define BOX_MON 80
#define PARTY_MON 100
#define FIRST_HM 339
#define SPECIES_UNOWN 201
#define SPECIES_SHEDINJA 303

enum { P_ITEMS, P_KEY, P_BALLS, P_TMHM, P_BERRIES, P_COUNT };

struct g3_format {
  const char *label;
  int sb2Size, sb1Size, partyCountOff, partyOff, moneyOff, keyOff, coinsOff, pocketsOff;
  int slots[P_COUNT];
  int stackMax;
};

static const struct g3_format G3_RS = {"RUBY / SAPPHIRE", 0x890, 0x3AC0, 0x234, 0x238, 0x490, -1, 0x494, 0x560,
                                       {20, 20, 16, 64, 46}, 99};
static const struct g3_format G3_E = {"EMERALD", 0xF2C, 0x3D88, 0x234, 0x238, 0x490, 0xAC, 0x494, 0x560,
                                      {30, 30, 16, 64, 46}, 99};
static const struct g3_format G3_FRLG = {"FIRERED / LEAFGREEN", 0xF24, 0x3D68, 0x34, 0x38, 0x290, 0xF20, 0x294, 0x310,
                                         {42, 30, 13, 58, 43}, 999};

/* The game's pocket number (struct Item.pocket) for each of SaveBlock1's pockets. */
static const int POCKET_ID[P_COUNT] = {1, 5, 2, 3, 4};
static const char *const POCKET_NAME[P_COUNT] = {"ITEMS", "KEY ITEMS", "POKé BALLS", "TMs & HMs", "BERRIES"};

/* gSubstructOrders: where Growth (0), Attacks (1), EVs (2) and Misc (3) are stored, by personality % 24. */
static const char ORDERS[24][5] = {"ABCD", "ABDC", "ACBD", "ACDB", "ADBC", "ADCB", "BACD", "BADC",
                                   "BCAD", "BCDA", "BDAC", "BDCA", "CABD", "CADB", "CBAD", "CBDA",
                                   "CDAB", "CDBA", "DABC", "DACB", "DBAC", "DBCA", "DCAB", "DCBA"};

struct g3 {
  const struct g3_format *f;
  struct pd_game game;
  uint8_t *rom;
  size_t romSize;
  uint8_t *raw; /* the file as read */
  size_t rawSize;
  int slot;
  uint32_t counter;
  int rotation;
  uint8_t *sb2, *sb1, *storage; /* the loaded slot's blocks, changed in place */
  uint32_t baseStats;           /* gBaseStats (28 bytes a species), 0 = not found */
  char title[96];
  char nameBuf[64];
  char ability[2][24];
  int tables; /* ST_*: the retail game whose move lists hold (retail_tables), ST_NONE = the ROM's own only */
};

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)rd16(p) | (uint32_t)rd16(p + 2) << 16; }
static void wr16(uint8_t *p, unsigned v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static void wr32(uint8_t *p, uint32_t v) {
  wr16(p, v & 0xFFFF);
  wr16(p + 2, v >> 16);
}

static int chunk(const struct g3_format *f, int id) {
  int size, n;
  if (id == 0)
    return f->sb2Size;
  if (id <= 4)
    size = f->sb1Size, n = id - 1;
  else
    size = STORAGE_SIZE, n = id - 5;
  int left = size - n * DATA;
  return left < 0 ? 0 : left > DATA ? DATA : left;
}

static uint16_t sector_sum(const uint8_t *p, int size) {
  uint32_t sum = 0;
  for (int i = 0; i + 4 <= size; i += 4)
    sum += rd32(p + i);
  return (uint16_t)((sum >> 16) + sum);
}

enum slot_status { SLOT_OK, SLOT_EMPTY, SLOT_CORRUPT };
struct slot_read {
  enum slot_status status;
  uint32_t counter;
  bool sameCounter;
  int base[SECTORS];
};

static struct slot_read read_slot(const struct g3_format *f, const uint8_t *b, int slot) {
  struct slot_read s = {SLOT_EMPTY, 0, true, {0}};
  bool any = false, first = true;
  unsigned valid = 0;
  for (int i = 0; i < SECTORS; i++)
    s.base[i] = -1;
  for (int i = 0; i < SECTORS; i++) {
    int off = (slot * SECTORS + i) * SECTOR;
    if (rd32(b + off + SIG_OFF) != SIGNATURE)
      continue;
    any = true;
    int id = rd16(b + off + ID_OFF);
    if (id >= SECTORS || rd16(b + off + SUM_OFF) != sector_sum(b + off, chunk(f, id)))
      continue;
    if (s.base[id] >= 0) {
      s.status = SLOT_CORRUPT;
      return s;
    }
    s.base[id] = off;
    valid |= 1u << id;
    uint32_t c = rd32(b + off + COUNT_OFF);
    if (!first && c != s.counter)
      s.sameCounter = false;
    s.counter = c;
    first = false;
  }
  s.status = !any ? SLOT_EMPTY : valid == (1u << SECTORS) - 1 ? SLOT_OK : SLOT_CORRUPT;
  return s;
}

/* Parses B as F: which slot the game loads, its blocks copied into the editor (when G is given). */
static const char *g3_parse(const struct g3_format *f, const uint8_t *b, size_t size, struct g3 *g, int *slotOut,
                            uint32_t *counterOut, enum slot_status *otherOut) {
  if (size < 2 * SECTORS * SECTOR)
    return "This file is too small for a GBA Pokemon save";
  struct slot_read s[2] = {read_slot(f, b, 0), read_slot(f, b, 1)};
  int pick;
  if (s[0].status != SLOT_OK && s[1].status != SLOT_OK)
    return s[0].status == SLOT_EMPTY && s[1].status == SLOT_EMPTY
               ? "The save is empty: save once in the game first"
               : "This save isn't laid out like this game's own saves (a hack with its own save format?), so the "
                 "editor won't change it";
  if (s[0].status == SLOT_OK && s[1].status == SLOT_OK) {
    uint32_t c0 = s[0].counter, c1 = s[1].counter;
    bool wrap = (c0 == 0xFFFFFFFFu && c1 == 0) || (c0 == 0 && c1 == 0xFFFFFFFFu);
    pick = wrap ? (c0 == 0xFFFFFFFFu ? 1 : 0) : c0 < c1 ? 1 : 0;
  } else
    pick = s[0].status == SLOT_OK ? 0 : 1;
  if (!s[pick].sameCounter)
    return "The save's sectors disagree on its save counter";
  *slotOut = pick;
  *counterOut = s[pick].counter;
  *otherOut = s[1 - pick].status;
  if (g) {
    for (int id = 0; id < SECTORS; id++) {
      const uint8_t *src = b + s[pick].base[id];
      int n = chunk(f, id);
      if (id == 0)
        memcpy(g->sb2, src, (size_t)n);
      else if (id <= 4)
        memcpy(g->sb1 + (id - 1) * DATA, src, (size_t)n);
      else
        memcpy(g->storage + (id - 5) * DATA, src, (size_t)n);
    }
    g->rotation = (s[pick].base[0] / SECTOR) % SECTORS;
  }
  return NULL;
}

static struct g3 *G(struct save_edit *e) { return (struct g3 *)e->impl; }

static uint32_t key32(struct g3 *g) { return g->f->keyOff >= 0 ? rd32(g->sb2 + g->f->keyOff) : 0; }

static int pocket_off(struct g3 *g, int p) {
  int off = g->f->pocketsOff;
  for (int i = 0; i < p; i++)
    off += g->f->slots[i] * 4;
  return off;
}

/* -- tables -- */

static bool rom_at(struct g3 *g, uint32_t addr, void *out, size_t n) { return pd_rom_read(&g->game, addr, out, n); }

static const char *g3_species_name(struct save_edit *e, int id) {
  struct g3 *g = G(e);
  if (id <= 0 || id >= PD_SPECIES_EGG)
    return NULL;
  const struct pd_text *t = g->game.cfg->text;
  if (!(t && t->species) && (id >= 412 || (id >= 252 && id <= 276)))
    return NULL; /* retail's numbering: 252-276 are the unused slots, 412 the egg */
  if (t && t->species && id == (g->game.cfg->eggSpecies ? g->game.cfg->eggSpecies : 412))
    return NULL;
  const char *n = pd_species_name(&g->game, id, g->nameBuf, sizeof g->nameBuf);
  if (!n || !*n || *n == '#' || *n == '?' || *n == '-')
    return NULL;
  return n;
}
static int g3_species_max(struct save_edit *e) {
  const struct pd_text *t = G(e)->game.cfg->text;
  return t && t->species ? t->speciesCount - 1 : 411;
}
static const char *g3_move_name(struct save_edit *e, int id) {
  struct g3 *g = G(e);
  if (id <= 0)
    return NULL;
  const char *n = pd_move_name(&g->game, id, g->nameBuf, sizeof g->nameBuf);
  return n && *n && strncmp(n, "MOVE#", 5) && *n != '-' ? n : NULL;
}
static int g3_move_max(struct save_edit *e) {
  const struct pd_text *t = G(e)->game.cfg->text;
  return (t && t->moves ? t->movesCount : pd_moves_count) - 1;
}
static const char *g3_item_name(struct save_edit *e, int id) {
  struct g3 *g = G(e);
  if (id <= 0)
    return NULL;
  const char *n = pd_item_name(&g->game, id);
  return n && *n && *n != '?' ? n : NULL;
}
static int g3_item_max(struct save_edit *e) {
  const struct pd_text *t = G(e)->game.cfg->text;
  if (t && t->items)
    return t->itemsCount - 1;
  return (G(e)->game.kind == PD_GAME_EMERALD ? pd_item_names_emerald_count : pd_item_names_firered_count) - 1;
}

/* struct Item.pocket in the ROM's gItems, -1 when it can't be read. The record is retail's from its itemId on (price,
 * holdEffect, holdEffectParam, description, importance, registrability, pocket): the pocket 12 bytes after the id,
 * the description 6 after it (44-byte records with the id at 14, 40-byte ones with it at 10); a record whose id isn't
 * the item's own isn't trusted. A game numbering its items as retail does and without such a table falls back to
 * retail's ranges. */
static int item_pocket(struct g3 *g, int item) {
  const struct pd_item_descs *t = g->game.cfg->itemDescs;
  if (item <= 0)
    return -1;
  if (t && !t->first && t->idOff >= 0 && t->descOff == t->idOff + 6 && t->stride >= t->idOff + 13 &&
      t->stride <= 64 && (!t->count || item < (int)t->count)) {
    uint8_t rec[64];
    if (rom_at(g, t->base + (uint32_t)t->stride * (uint32_t)item, rec, (size_t)t->stride) &&
        rd16(rec + t->idOff) == item) {
      int p = rec[t->idOff + 12];
      return p >= 1 && p <= 5 ? p : -1;
    }
    return -1;
  }
  const struct pd_text *names = g->game.cfg->text;
  if (names && names->items)
    return -1; /* its own numbering: retail's ranges would be wrong */
  if (item <= 12)
    return 2;
  if (item >= 133 && item <= 175)
    return 4;
  if ((item >= 259 && item <= 288) || (item >= 349 && item <= 376))
    return 5;
  if (item >= 289 && item <= 346)
    return 3;
  if (item < 259)
    return 1;
  return -1;
}
/* The editor's pocket (P_*) for an item, -1 when it has none. */
static int g3_item_pocket(struct save_edit *e, int id) {
  if (!g3_item_name(e, id))
    return -1;
  int game = item_pocket(G(e), id);
  for (int p = 0; p < P_COUNT; p++)
    if (POCKET_ID[p] == game)
      return p;
  return -1;
}

static bool base_stats(struct g3 *g, int species, uint8_t out[28]) {
  return g->baseStats && rom_at(g, g->baseStats + (uint32_t)species * 28, out, 28);
}

static int growth_rate(struct g3 *g, int species) {
  uint8_t b[28];
  if (base_stats(g, species, b))
    return b[19] <= 5 ? b[19] : -1;
  if (!g->game.cfg->text && species > 0 && species < pd_growth_rates_count)
    return pd_growth_rates[species];
  return -1;
}

static int gender_ratio(struct g3 *g, int species) {
  uint8_t b[28];
  if (base_stats(g, species, b))
    return b[16];
  return pd_gender_ratio(&g->game, species);
}

static int level_for(int rate, uint32_t exp) {
  int level = 1;
  while (level < 100 && (long)exp >= pd_exp_at(rate, level + 1))
    level++;
  return level;
}

/* gBaseStats where the config doesn't say (FireRed rev 0 ...): BULBASAUR's and IVYSAUR's records, retail's numbering. */
static uint32_t find_base_stats(struct g3 *g) {
  if (g->game.cfg->speciesInfo && !g->game.cfg->expansion)
    return g->game.cfg->speciesInfo;
  if (g->game.cfg->expansion)
    return 0;
  static const uint8_t bulba[] = {45, 49, 49, 45, 65, 65, 12, 3}, ivy[] = {60, 62, 63, 60, 80, 80, 12, 3};
  for (size_t i = 28; i + 56 <= g->romSize; i += 4)
    if (!memcmp(g->rom + i, bulba, 8) && !memcmp(g->rom + i + 28, ivy, 8)) {
      bool zero = true;
      for (int k = 0; k < 28; k++)
        zero = zero && !g->rom[i - 28 + k];
      if (zero)
        return 0x08000000u + (uint32_t)(i - 28);
    }
  return 0;
}

static const char *g3_ability_name(struct g3 *g, int id, char *buf) {
  const struct pd_config *c = g->game.cfg;
  if (c->abilityNames && !c->abilityNamePtr && id > 0) {
    int len = c->abilityNameLength ? c->abilityNameLength : 13;
    int stride = c->abilityNameStride ? c->abilityNameStride : len;
    uint8_t raw[32];
    if (len < 32 && rom_at(g, c->abilityNames + (uint32_t)(id * stride), raw, (size_t)len)) {
      pd_gen3_text(raw, (size_t)len, buf, 24);
      if (*buf)
        return buf;
    }
  }
  snprintf(buf, 24, "ABILITY %d", id);
  return buf;
}

static bool g3_abilities(struct save_edit *e, int species, const char **first, const char **second) {
  struct g3 *g = G(e);
  uint8_t b[28];
  if (!base_stats(g, species, b))
    return false;
  *first = g3_ability_name(g, b[22], g->ability[0]);
  *second = b[23] && b[23] != b[22] ? g3_ability_name(g, b[23], g->ability[1]) : NULL;
  return true;
}

/* The Western charset (pd_gen3_text's table) the other way round: a byte per character, 0 = none. */
static int encode_char(const char *s, int *used) {
  static char table[256][8];
  static bool built;
  if (!built) {
    for (int b = 0; b < 0xF7; b++) {
      uint8_t src[2] = {(uint8_t)b, 0xFF};
      pd_gen3_text(src, 2, table[b], sizeof table[b]);
    }
    built = true;
  }
  if (*s == ' ') {
    *used = 1;
    return 0x00;
  }
  int best = -1, bestLen = 0;
  for (int b = 1; b < 0xF7; b++) {
    int l = (int)strlen(table[b]);
    if (l > bestLen && !strncmp(s, table[b], (size_t)l))
      best = b, bestLen = l;
  }
  *used = bestLen;
  return best;
}

static bool encode_name(const char *s, uint8_t *out, int max) {
  memset(out, 0xFF, (size_t)max);
  int n = 0;
  while (*s) {
    int used = 0, b = encode_char(s, &used);
    if (b < 0 || n >= max)
      return false;
    out[n++] = (uint8_t)b;
    s += used;
  }
  return n > 0;
}

/* -- one record -- */

struct box_mon {
  uint8_t head[32];
  uint8_t data[48]; /* Growth, Attacks, EVs, Misc in logical order */
  bool ok;
};

static void decode_box(const uint8_t *r, struct box_mon *m) {
  memcpy(m->head, r, 32);
  uint32_t pid = rd32(r), key = pid ^ rd32(r + 4);
  const char *order = ORDERS[pid % 24];
  uint16_t sum = 0;
  for (int slot = 0; slot < 4; slot++) {
    int logical = order[slot] - 'A';
    for (int w = 0; w < 3; w++) {
      uint32_t word = rd32(r + 32 + slot * 12 + w * 4) ^ key;
      wr32(m->data + logical * 12 + w * 4, word);
      sum = (uint16_t)(sum + (word & 0xFFFF) + (word >> 16));
    }
  }
  m->ok = sum == rd16(r + 0x1C);
}

static void encode_box(const struct box_mon *m, uint8_t *r) {
  memcpy(r, m->head, 32);
  uint16_t sum = 0;
  for (int i = 0; i < 24; i++)
    sum = (uint16_t)(sum + rd16(m->data + i * 2));
  wr16(r + 0x1C, sum);
  uint32_t pid = rd32(r), key = pid ^ rd32(r + 4);
  const char *order = ORDERS[pid % 24];
  for (int slot = 0; slot < 4; slot++) {
    int logical = order[slot] - 'A';
    for (int w = 0; w < 3; w++)
      wr32(r + 32 + slot * 12 + w * 4, rd32(m->data + logical * 12 + w * 4) ^ key);
  }
}

static uint8_t *record(struct g3 *g, int box, int slot) {
  if (box == SE_PARTY) {
    if (slot < 0 || slot >= 6 || (uint32_t)slot >= rd32(g->sb1 + g->f->partyCountOff))
      return NULL;
    return g->sb1 + g->f->partyOff + slot * PARTY_MON;
  }
  if (box < 0 || box >= BOXES || slot < 0 || slot >= BOX_SLOTS)
    return NULL;
  return g->storage + 4 + (box * BOX_SLOTS + slot) * BOX_MON;
}

static bool is_shiny(uint32_t pid, uint32_t ot) {
  return ((ot >> 16) ^ (ot & 0xFFFF) ^ (pid >> 16) ^ (pid & 0xFFFF)) < 8;
}
static int unown_letter(uint32_t p) {
  return (int)((((p >> 24) & 3) << 6 | ((p >> 16) & 3) << 4 | ((p >> 8) & 3) << 2 | (p & 3)) % 28);
}

static bool g3_mon(struct save_edit *e, int box, int slot, struct se_mon *out) {
  struct g3 *g = G(e);
  const uint8_t *r = record(g, box, slot);
  if (!r)
    return false;
  struct box_mon m;
  decode_box(r, &m);
  int species = rd16(m.data);
  if (!m.ok || !species || (m.head[0x13] & 1))
    return false; /* empty, or a Bad Egg (never shown as editable) */
  memset(out, 0, sizeof *out);
  out->species = species;
  out->held = rd16(m.data + 2);
  uint32_t exp = rd32(m.data + 4), iv = rd32(m.data + 36 + 4);
  int rate = growth_rate(g, species);
  out->level = box == SE_PARTY ? r[0x54] : rate >= 0 ? level_for(rate, exp) : 0;
  for (int i = 0; i < 4; i++)
    out->moves[i] = rd16(m.data + 12 + i * 2);
  for (int i = 0; i < 6; i++) {
    out->evs[i] = m.data[24 + i];
    out->ivs[i] = (int)(iv >> (i * 5) & 31);
  }
  out->egg = (iv >> 30) & 1;
  out->ability = (int)(iv >> 31);
  out->extras = true;
  out->friendship = m.data[9];
  out->exp = (long)exp;
  for (int i = 0; i < 4; i++)
    out->ppUps[i] = out->moves[i] ? (m.data[8] >> (i * 2)) & 3 : 0;
  uint32_t pid = rd32(m.head);
  out->nature = (int)(pid % 25);
  out->shiny = is_shiny(pid, rd32(m.head + 4));
  pd_gen3_text(m.head + 8, 10, out->nick, sizeof out->nick);
  if (box == SE_PARTY) {
    out->hp = rd16(r + 0x56);
    out->maxHp = rd16(r + 0x58);
  }
  return true;
}

/* A personality with NATURE and SHINY that keeps OLD's gender (and an Unown's letter). */
static bool new_personality(struct g3 *g, int species, uint32_t old, uint32_t ot, int nature, bool shiny,
                            uint32_t *out) {
  int ratio = gender_ratio(g, species);
  bool gendered = ratio > 0 && ratio < 254;
  uint32_t seed = old ^ (uint32_t)time(NULL) ^ 0x9E3779B9u;
  for (long tries = 0; tries < 50000000L; tries++) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    uint32_t p = seed;
    if (shiny) {
      uint32_t lo = p & 0xFFFF, hi = ((ot >> 16) ^ (ot & 0xFFFF) ^ lo ^ ((p >> 16) & 7)) & 0xFFFF;
      p = hi << 16 | lo;
    } else if (is_shiny(p, ot))
      continue;
    if ((int)(p % 25) != nature)
      continue;
    if (gendered && (((p & 0xFF) < (uint32_t)ratio) != ((old & 0xFF) < (uint32_t)ratio)))
      continue;
    if (species == SPECIES_UNOWN && unown_letter(p) != unown_letter(old))
      continue;
    *out = p;
    return true;
  }
  return false;
}

static int nature_up(int n) { return n / 5 == n % 5 ? -1 : n / 5 + 1; }
static int nature_down(int n) { return n / 5 == n % 5 ? -1 : n % 5 + 1; }

static bool party_stats(struct g3 *g, int species, int level, int nature, const int ivs[6], const int evs[6],
                        int stats[6]) {
  uint8_t b[28];
  if (!base_stats(g, species, b))
    return false;
  for (int s = 0; s < 6; s++) {
    int n = (2 * b[s] + ivs[s] + evs[s] / 4) * level / 100;
    if (s == 0)
      stats[0] = species == SPECIES_SHEDINJA ? 1 : n + level + 10;
    else {
      int v = n + 5;
      if (s == nature_up(nature))
        v = v * 110 / 100;
      if (s == nature_down(nature))
        v = v * 90 / 100;
      stats[s] = v;
    }
  }
  return true;
}

/* What a retail Gen 3 Pokemon may learn (save_tables_gen.h, the decomps' tables, as the app's Gen3Legal). */
enum { ST_NONE, ST_RS, ST_E, ST_FRLG };
enum { SPECIES_SMEARGLE = 235, MOVE_STRUGGLE = 165, LAST_G3_MOVE = 354 };

static void st_level(int t, const uint16_t **lv, const uint16_t **start) {
  *lv = t == ST_RS ? ST_LEVEL_RS : t == ST_E ? ST_LEVEL_E : ST_LEVEL_FRLG;
  *start = t == ST_RS ? ST_LEVEL_START_RS : t == ST_E ? ST_LEVEL_START_E : ST_LEVEL_START_FRLG;
}

/* The tables stand for this game only when it's retail: no names of its own, not randomized, and (where the config
 * maps them) every species' level-up moves in the ROM as the tables have them. */
static int retail_tables(struct g3 *g) {
  const struct pd_config *c = g->game.cfg;
  int t = g->f == &G3_RS ? ST_RS : g->f == &G3_E ? ST_E : ST_FRLG;
  if (!c || c->text || g->game.randomized || (c->speciesCount && c->speciesCount != 412))
    return ST_NONE;
  if (!c->learnsets)
    return t;
  const uint16_t *lv, *start;
  st_level(t, &lv, &start);
  struct pd_level_move m[64];
  for (int s = 1; s < 412; s++) {
    int count = pd_level_moves(&g->game, s, m, 64);
    if (count != start[s + 1] - start[s])
      return ST_NONE;
    for (int k = 0; k < count; k++) {
      unsigned w = lv[start[s] + k];
      if (m[k].move != (int)(w & 0x1FF) || m[k].level != (int)(w >> 9))
        return ST_NONE;
    }
  }
  return t;
}

static int add_move(uint16_t *out, int count, int cap, int move) {
  if (move <= 0 || count >= cap)
    return count;
  for (int i = 0; i < count; i++)
    if (out[i] == move)
      return count;
  out[count] = (uint16_t)move;
  return count + 1;
}

static int g3_learnable(struct save_edit *e, int species, int level, uint16_t *out, int cap, bool *complete) {
  struct g3 *g = G(e);
  int count = 0;
  *complete = false;
  if (g->tables) {
    *complete = true;
    if (species == SPECIES_SMEARGLE) { /* Sketch: any move but Struggle */
      for (int mv = 1; mv <= LAST_G3_MOVE; mv++)
        if (mv != MOVE_STRUGGLE)
          count = add_move(out, count, cap, mv);
      return count;
    }
    const uint16_t *lv, *start;
    st_level(g->tables, &lv, &start);
    const uint64_t *tm = g->tables == ST_RS ? ST_TMHM_RS : g->tables == ST_E ? ST_TMHM_E : ST_TMHM_FRLG;
    const uint32_t *tu = g->tables == ST_RS ? ST_TUTOR_RS : g->tables == ST_E ? ST_TUTOR_E : ST_TUTOR_FRLG;
    const uint16_t *tuMoves = g->tables == ST_RS ? ST_TUTOR_MOVES_RS
                              : g->tables == ST_E ? ST_TUTOR_MOVES_E
                                                  : ST_TUTOR_MOVES_FRLG;
    int tuCount = g->tables == ST_RS ? ST_TUTOR_COUNT_RS : g->tables == ST_E ? ST_TUTOR_COUNT_E : ST_TUTOR_COUNT_FRLG;
    for (int s = species, guard = 0; s > 0 && s < 412 && guard < 4; s = ST_PRE_EVOLUTION[s], guard++) {
      for (int k = start[s]; k < start[s + 1]; k++)
        if ((lv[k] >> 9) <= level)
          count = add_move(out, count, cap, lv[k] & 0x1FF);
      for (int b = 0; b < 58; b++)
        if (tm[s] >> b & 1)
          count = add_move(out, count, cap, ST_TMHM_MOVES[b]);
      for (int b = 0; b < tuCount; b++)
        if (tu[s] >> b & 1)
          count = add_move(out, count, cap, tuMoves[b]);
      if (g->tables == ST_FRLG && (s == 3 || s == 6 || s == 9)) /* Cape Brink's tutor, for the three starters */
        count = add_move(out, count, cap, s == 3 ? 338 : s == 6 ? 307 : 308);
    }
    return count;
  }
  /* A hack: the ROM's own lists, the species' alone, as a guide (nothing is refused). */
  struct pd_level_move lm[128];
  struct pd_machine_move mm[128];
  int a = pd_dex_level_moves(&g->game, species, lm, 128), b = pd_machine_moves(&g->game, species, mm, 128);
  if (a <= 0 && b <= 0)
    return -1;
  for (int k = 0; k < a; k++)
    if (lm[k].level <= level)
      count = add_move(out, count, cap, lm[k].move);
  for (int k = 0; k < b; k++)
    count = add_move(out, count, cap, mm[k].move);
  return count;
}

static bool g3_set_mon(struct save_edit *e, int box, int slot, const struct se_mon *want, char *err, size_t n) {
  struct g3 *g = G(e);
  uint8_t *r = record(g, box, slot);
  struct se_mon now;
  if (!r || !g3_mon(e, box, slot, &now)) {
    say(err, n, "There's no Pokemon there to change");
    return false;
  }
  if (now.egg) {
    say(err, n, "Eggs aren't changed: hatch it first");
    return false;
  }
  if (!g3_species_name(e, want->species)) {
    say(err, n, "That isn't a Pokemon this game has");
    return false;
  }
  if (want->level < 1 || want->level > 100) {
    say(err, n, "Levels go from 1 to 100");
    return false;
  }
  if (want->nature < 0 || want->nature > 24) {
    say(err, n, "That isn't a nature");
    return false;
  }
  int evTotal = 0;
  for (int i = 0; i < 6; i++) {
    if (want->ivs[i] < 0 || want->ivs[i] > 31) {
      say(err, n, "IVs go from 0 to 31");
      return false;
    }
    if (want->evs[i] < 0 || want->evs[i] > 255) {
      say(err, n, "An EV goes from 0 to 255");
      return false;
    }
    evTotal += want->evs[i];
  }
  if (evTotal > 510) {
    say(err, n, "EVs add up to 510 at most");
    return false;
  }
  int moveCount = 0;
  for (int i = 0; i < 4; i++) {
    int m = want->moves[i];
    if (!m)
      continue;
    if (!g3_move_name(e, m) || m >= pd_move_pp_count) {
      say(err, n, "That isn't a move this game has");
      return false;
    }
    for (int k = 0; k < i; k++)
      if (want->moves[k] == m) {
        say(err, n, "A Pokemon can't know one move twice");
        return false;
      }
    moveCount++;
  }
  if (!moveCount) {
    say(err, n, "A Pokemon needs at least one move");
    return false;
  }
  if (g->tables) { /* a new move must be one it could learn; the ones it knows (egg moves too) stay */
    static uint16_t ok[400];
    bool complete;
    int count = g3_learnable(e, want->species, want->level, ok, 400, &complete);
    for (int i = 0; i < 4; i++) {
      int mv = want->moves[i];
      bool fine = !mv;
      for (int k = 0; k < 4 && !fine; k++)
        fine = now.moves[k] == mv;
      for (int k = 0; k < count && !fine; k++)
        fine = ok[k] == mv;
      if (!fine) {
        char who[32]; /* the names share one buffer */
        const char *s = g3_species_name(e, want->species);
        snprintf(who, sizeof who, "%s", s ? s : "This Pokemon");
        const char *what = g3_move_name(e, mv);
        snprintf(err, n, "%s can't learn %s by level %d in this game", who, what ? what : "that move", want->level);
        return false;
      }
    }
  }
  if (want->held) {
    int pocket = item_pocket(g, want->held);
    if (!g3_item_name(e, want->held) || pocket == 5 || (pocket == 3 && want->held >= FIRST_HM) || pocket < 0) {
      say(err, n, "That isn't an item a Pokemon can hold");
      return false;
    }
  }
  const char *a1 = NULL, *a2 = NULL;
  int ability = want->ability;
  if (ability && (!g3_abilities(e, want->species, &a1, &a2) || !a2)) {
    say(err, n, "This Pokemon has only one ability");
    return false;
  }
  int rate = growth_rate(g, want->species);
  if (rate < 0) {
    say(err, n, "This game's growth tables aren't known, so levels can't be set");
    return false;
  }
  /* Extras: friendship, PP Ups and the EXP within the level, as the game could have them. */
  long expLo = pd_exp_at(rate, want->level), expHi = want->level < 100 ? pd_exp_at(rate, want->level + 1) - 1 : expLo;
  long newExp = -1;
  if (want->extras) {
    if (want->friendship < 0 || want->friendship > 255) {
      say(err, n, "Friendship goes from 0 to 255");
      return false;
    }
    for (int i = 0; i < 4; i++)
      if (want->ppUps[i] < 0 || want->ppUps[i] > 3 || (want->ppUps[i] && !want->moves[i])) {
        say(err, n, "A move takes 0 to 3 PP Ups");
        return false;
      }
    if (want->exp >= expLo && want->exp <= expHi)
      newExp = want->exp;
    else if (want->level == now.level && want->species == now.species && want->exp != now.exp) {
      snprintf(err, n, "At level %d, EXP goes from %ld to %ld", want->level, expLo, expHi);
      return false;
    }
  }
  int stats[6] = {0};
  if (box == SE_PARTY && !party_stats(g, want->species, want->level, want->nature, want->ivs, want->evs, stats)) {
    say(err, n, "This game's base stats aren't known, so a party Pokemon can't be changed; try one in a box");
    return false;
  }
  uint8_t nick[10];
  bool rename = false;
  char oldSpecies[64] = "";
  const char *os = g3_species_name(e, now.species);
  if (os)
    snprintf(oldSpecies, sizeof oldSpecies, "%s", os);
  if (strcmp(want->nick, now.nick)) {
    if (g->game.language == 'J' || !encode_name(want->nick, nick, 10)) {
      say(err, n, "That name has letters the game can't show (10 at most)");
      return false;
    }
    rename = true;
  } else if (want->species != now.species && !strcmp(now.nick, oldSpecies)) {
    /* An unnamed Pokemon takes its new species' name, as an evolution renames it. */
    const char *ns = g3_species_name(e, want->species);
    rename = g->game.language != 'J' && ns && encode_name(ns, nick, 10);
  }

  struct box_mon m;
  decode_box(r, &m);
  uint32_t pid = rd32(m.head), ot = rd32(m.head + 4);
  if (want->nature != now.nature || want->shiny != now.shiny || want->species != now.species) {
    uint32_t p = pid;
    if (want->nature != (int)(pid % 25) || want->shiny != is_shiny(pid, ot) ||
        (want->species == SPECIES_UNOWN) != (now.species == SPECIES_UNOWN)) {
      if (!new_personality(g, want->species, pid, ot, want->nature, want->shiny, &p)) {
        say(err, n, "Couldn't find a personality with that nature");
        return false;
      }
    }
    pid = p;
  }
  wr32(m.head, pid);
  if (rename)
    memcpy(m.head + 8, nick, 10);
  m.head[0x13] |= 2; /* hasSpecies */
  wr16(m.data, (unsigned)want->species);
  wr16(m.data + 2, (unsigned)want->held);
  uint32_t exp = rd32(m.data + 4);
  if (newExp >= 0)
    wr32(m.data + 4, (uint32_t)newExp);
  else if (want->level != now.level || want->species != now.species || level_for(rate, exp) != want->level)
    wr32(m.data + 4, (uint32_t)pd_exp_at(rate, want->level));
  if (want->extras)
    m.data[9] = (uint8_t)want->friendship;
  uint8_t ppUps = m.data[8];
  for (int i = 0; i < 4; i++) {
    int mv = want->moves[i];
    if (mv != now.moves[i] || rd16(m.data + 12 + i * 2) != mv) {
      wr16(m.data + 12 + i * 2, (unsigned)mv);
      m.data[20 + i] = mv ? pd_move_pp[mv] : 0;
      ppUps = (uint8_t)(ppUps & ~(3 << (i * 2)));
    }
  }
  if (want->extras)
    for (int i = 0; i < 4; i++) {
      int mv = want->moves[i], was = (ppUps >> (i * 2)) & 3, ups = want->ppUps[i];
      if (!mv || ups == was)
        continue;
      /* The most PP grows (or shrinks) by a fifth of the base a PP Up, and what's left with it, as using one does. */
      int base = pd_move_pp[mv], fullWas = base + base * was / 5, full = base + base * ups / 5;
      int pp = m.data[20 + i] + full - fullWas;
      m.data[20 + i] = (uint8_t)(pp < 0 ? 0 : pp > full ? full : pp);
      ppUps = (uint8_t)((ppUps & ~(3 << (i * 2))) | ups << (i * 2));
    }
  m.data[8] = ppUps;
  for (int i = 0; i < 6; i++)
    m.data[24 + i] = (uint8_t)want->evs[i];
  uint32_t iv = rd32(m.data + 40) & (1u << 30);
  for (int i = 0; i < 6; i++)
    iv |= (uint32_t)want->ivs[i] << (i * 5);
  iv |= (uint32_t)(ability & 1) << 31;
  wr32(m.data + 40, iv);
  encode_box(&m, r);
  if (box == SE_PARTY) {
    int oldMax = rd16(r + 0x58), oldHp = rd16(r + 0x56);
    r[0x54] = (uint8_t)want->level;
    for (int s = 0; s < 6; s++)
      wr16(r + 0x58 + s * 2, (unsigned)stats[s]);
    int hp = oldMax ? (int)((long)oldHp * stats[0] / oldMax) : stats[0];
    if (oldHp && !hp)
      hp = 1;
    wr16(r + 0x56, (unsigned)(hp > stats[0] ? stats[0] : hp));
  }
  e->dirty = true;
  return true;
}

static int g3_heal(struct save_edit *e) {
  struct g3 *g = G(e);
  uint32_t count = rd32(g->sb1 + g->f->partyCountOff);
  int healed = 0;
  for (uint32_t i = 0; i < count && i < 6; i++) {
    uint8_t *r = g->sb1 + g->f->partyOff + i * PARTY_MON;
    struct box_mon m;
    decode_box(r, &m);
    if (!m.ok || !rd16(m.data) || ((rd32(m.data + 40) >> 30) & 1))
      continue;
    bool changed = rd32(r + 0x50) != 0 || rd16(r + 0x56) != rd16(r + 0x58);
    for (int k = 0; k < 4; k++) {
      int mv = rd16(m.data + 12 + k * 2);
      if (!mv || mv >= pd_move_pp_count)
        continue;
      int ups = (m.data[8] >> (k * 2)) & 3, base = pd_move_pp[mv];
      int full = base + base * ups / 5;
      if (m.data[20 + k] != full) {
        m.data[20 + k] = (uint8_t)full;
        changed = true;
      }
    }
    if (!changed)
      continue;
    encode_box(&m, r);
    wr32(r + 0x50, 0);
    wr16(r + 0x56, rd16(r + 0x58));
    healed++;
  }
  if (healed)
    e->dirty = true;
  return healed;
}

/* -- money, coins, bag -- */

static const char *g3_title(struct save_edit *e) { return G(e)->title; }
static long g3_money(struct save_edit *e) {
  struct g3 *g = G(e);
  return (long)(rd32(g->sb1 + g->f->moneyOff) ^ key32(g));
}
static long g3_money_max(struct save_edit *e) {
  uint32_t m = G(e)->game.cfg->maxMoney;
  return m ? (long)m : 999999;
}
static bool g3_set_money(struct save_edit *e, long v, char *err, size_t n) {
  struct g3 *g = G(e);
  if (v < 0 || v > g3_money_max(e)) {
    snprintf(err, n, "Money goes from 0 to %ld", g3_money_max(e));
    return false;
  }
  wr32(g->sb1 + g->f->moneyOff, (uint32_t)v ^ key32(g));
  e->dirty = true;
  return true;
}
static long g3_coins(struct save_edit *e) {
  struct g3 *g = G(e);
  return rd16(g->sb1 + g->f->coinsOff) ^ (key32(g) & 0xFFFF);
}
static bool g3_set_coins(struct save_edit *e, long v, char *err, size_t n) {
  struct g3 *g = G(e);
  if (v < 0 || v > 9999) {
    say(err, n, "Coins go from 0 to 9999");
    return false;
  }
  wr16(g->sb1 + g->f->coinsOff, ((unsigned)v ^ key32(g)) & 0xFFFF);
  e->dirty = true;
  return true;
}
static int g3_pockets(struct save_edit *e) {
  (void)e;
  return P_COUNT;
}
static const char *g3_pocket_name(struct save_edit *e, int p) {
  (void)e;
  return p >= 0 && p < P_COUNT ? POCKET_NAME[p] : "";
}
static int g3_pocket_slots(struct save_edit *e, int p) { return p >= 0 && p < P_COUNT ? G(e)->f->slots[p] : 0; }
static bool g3_pocket_item(struct save_edit *e, int p, int i, struct se_item *out) {
  struct g3 *g = G(e);
  if (p < 0 || p >= P_COUNT || i < 0 || i >= g->f->slots[p])
    return false;
  const uint8_t *s = g->sb1 + pocket_off(g, p) + i * 4;
  out->id = rd16(s);
  out->count = (rd16(s + 2) ^ (key32(g) & 0xFFFF)) & 0xFFFF;
  if (!out->id)
    out->count = 0;
  return true;
}
static const char *g3_pocket_locked(struct save_edit *e, int p) {
  (void)e;
  return p == P_KEY ? "Key items are the story's: the editor leaves them be" : NULL;
}
static int g3_item_max_count(struct save_edit *e, int p, int id) {
  if (p == P_TMHM && id >= FIRST_HM)
    return 1;
  return p == P_BERRIES ? 999 : G(e)->f->stackMax;
}
static bool g3_set_item(struct save_edit *e, int p, int i, int id, int count, char *err, size_t n) {
  struct g3 *g = G(e);
  if (p < 0 || p >= P_COUNT || i < 0 || i >= g->f->slots[p]) {
    say(err, n, "There's no such bag slot");
    return false;
  }
  if (g3_pocket_locked(e, p)) {
    say(err, n, g3_pocket_locked(e, p));
    return false;
  }
  struct se_item old;
  g3_pocket_item(e, p, i, &old);
  if (old.id && p == P_TMHM && old.id >= FIRST_HM) {
    say(err, n, "HMs are the story's: the editor leaves them be");
    return false;
  }
  uint8_t *base = g->sb1 + pocket_off(g, p);
  if (!count || !id) {
    /* Taken out: the slots after it move up, as RemoveBagItem's compaction leaves them. */
    if (!old.id)
      return true;
    memmove(base + i * 4, base + (i + 1) * 4, (size_t)(g->f->slots[p] - i - 1) * 4);
    wr16(base + (g->f->slots[p] - 1) * 4, 0);
    wr16(base + (g->f->slots[p] - 1) * 4 + 2, key32(g) & 0xFFFF);
    e->dirty = true;
    return true;
  }
  if (g3_item_pocket(e, id) != p) {
    snprintf(err, n, "That item doesn't go in %s", POCKET_NAME[p]);
    return false;
  }
  if (p == P_TMHM && id >= FIRST_HM) {
    say(err, n, "HMs are the story's: the editor leaves them be");
    return false;
  }
  if (count < 1 || count > g3_item_max_count(e, p, id)) {
    snprintf(err, n, "One stack holds 1 to %d", g3_item_max_count(e, p, id));
    return false;
  }
  for (int k = 0; k < g->f->slots[p]; k++) {
    struct se_item other;
    g3_pocket_item(e, p, k, &other);
    if (k != i && other.id == id) {
      say(err, n, "That item is in the bag already: change that stack");
      return false;
    }
  }
  if (!old.id) {
    /* A new item goes in the first empty slot, as AddBagItem puts it. */
    int k = 0;
    struct se_item s;
    while (k < g->f->slots[p] && g3_pocket_item(e, p, k, &s) && s.id)
      k++;
    if (k >= g->f->slots[p]) {
      say(err, n, "That pocket is full");
      return false;
    }
    i = k;
  }
  wr16(base + i * 4, (unsigned)id);
  wr16(base + i * 4 + 2, ((unsigned)count ^ key32(g)) & 0xFFFF);
  e->dirty = true;
  return true;
}

static int g3_boxes(struct save_edit *e) {
  (void)e;
  return BOXES;
}
static int g3_box_slots(struct save_edit *e) {
  (void)e;
  return BOX_SLOTS;
}

/* -- writing -- */

static bool g3_build(struct save_edit *e, uint8_t **outp, size_t *size, char *err, size_t n) {
  struct g3 *g = G(e);
  uint8_t *out = malloc(g->rawSize);
  if (!out) {
    say(err, n, "Out of memory");
    return false;
  }
  memcpy(out, g->raw, g->rawSize);
  int target = 1 - g->slot, rotation = (g->rotation + 1) % SECTORS;
  uint32_t counter = g->counter + 1;
  for (int id = 0; id < SECTORS; id++) {
    uint8_t *s = out + (target * SECTORS + (id + rotation) % SECTORS) * SECTOR;
    memset(s, 0, SECTOR);
    int size1 = chunk(g->f, id);
    const uint8_t *src = id == 0 ? g->sb2 : id <= 4 ? g->sb1 + (id - 1) * DATA : g->storage + (id - 5) * DATA;
    memcpy(s, src, (size_t)size1);
    wr16(s + ID_OFF, (unsigned)id);
    wr16(s + SUM_OFF, sector_sum(s, size1));
    wr32(s + SIG_OFF, SIGNATURE);
    wr32(s + COUNT_OFF, counter);
  }
  if (!e->ops->verify(e, out, g->rawSize, err, n)) {
    free(out);
    return false;
  }
  *outp = out;
  *size = g->rawSize;
  return true;
}

/* B is exactly the loaded save with the editor's blocks saved over it the game's way. */
static bool g3_verify(struct save_edit *e, const uint8_t *b, size_t size, char *err, size_t n) {
  struct g3 *g = G(e);
  if (size != g->rawSize) {
    say(err, n, "The save's size changed");
    return false;
  }
  struct g3 again = *g;
  uint8_t *sb2 = malloc((size_t)g->f->sb2Size), *sb1 = malloc((size_t)g->f->sb1Size), *st = malloc(STORAGE_SIZE);
  bool ok = false;
  if (sb2 && sb1 && st) {
    again.sb2 = sb2, again.sb1 = sb1, again.storage = st;
    int slot;
    uint32_t counter;
    enum slot_status other;
    const char *why = g3_parse(g->f, b, size, &again, &slot, &counter, &other);
    if (why)
      say(err, n, why);
    else if (slot == g->slot || counter != g->counter + 1 || other != SLOT_OK)
      say(err, n, "The game wouldn't load the new save over the old one");
    else if (memcmp(sb2, g->sb2, (size_t)g->f->sb2Size) || memcmp(sb1, g->sb1, (size_t)g->f->sb1Size) ||
             memcmp(st, g->storage, STORAGE_SIZE))
      say(err, n, "The save read back differently");
    else {
      size_t start = (size_t)(1 - g->slot) * SECTORS * SECTOR, end = start + SECTORS * SECTOR;
      ok = true;
      for (size_t i = 0; i < size && ok; i++)
        if ((i < start || i >= end) && b[i] != g->raw[i]) {
          ok = false;
          say(err, n, "A byte outside the new save slot changed");
        }
    }
  }
  free(sb2);
  free(sb1);
  free(st);
  return ok;
}

static void g3_free(struct save_edit *e) {
  struct g3 *g = G(e);
  free(g->raw);
  free(g->sb1);
  free(g->sb2);
  free(g->storage);
  free(g->rom);
  free(g);
  free(e);
}

static const struct se_ops G3_OPS = {
    g3_title,        g3_money,        g3_money_max,   g3_set_money,     g3_coins,     g3_set_coins,
    g3_pockets,      g3_pocket_name,  g3_pocket_slots, g3_pocket_item,  g3_pocket_locked, g3_set_item,
    g3_item_max_count, g3_boxes,      g3_box_slots,   g3_mon,           g3_set_mon,   g3_heal,
    g3_species_name, g3_species_max,  g3_move_name,   g3_move_max,      g3_item_name, g3_item_max,
    g3_abilities,    g3_build,        g3_verify,      g3_free,      g3_item_pocket, g3_learnable,
};

struct save_edit *se_open_gen3(const uint8_t *save, size_t saveSize, const uint8_t *rom, size_t romSize, char *err,
                               size_t errSize) {
  if (romSize < 0x200 || romSize > 64u * 1024 * 1024) {
    say(err, errSize, "This isn't a GBA game the editor knows");
    return NULL;
  }
  struct g3 *g = calloc(1, sizeof *g);
  struct save_edit *e = calloc(1, sizeof *e);
  if (!g || !e) {
    free(g);
    free(e);
    say(err, errSize, "Out of memory");
    return NULL;
  }
  e->impl = g;
  g->rom = (uint8_t *)rom; /* owned: se_open hands it over */
  g->romSize = romSize;
  pd_game_detect(&g->game, rom, romSize);
  g->game.rom = rom;
  g->game.romSize = romSize;
  pd_game_identify_quiet(&g->game);
  const char *code = g->game.code, *why = NULL;
  if (code[3] == 'J')
    why = "Japanese saves aren't covered yet";
  else if (!strncmp(code, "AXV", 3) || !strncmp(code, "AXP", 3))
    g->f = &G3_RS;
  else if (!strncmp(code, "BPE", 3))
    g->f = &G3_E;
  else if (!strncmp(code, "BPR", 3) || !strncmp(code, "BPG", 3))
    g->f = &G3_FRLG;
  else
    why = "The save editor covers the GBA Pokemon games (and HeartGold / SoulSilver) so far";
  const struct pd_config *c = g->game.cfg;
  if (!why && (!c || c->gb))
    why = g->game.unsupported ? g->game.unsupported : "This game isn't one the companion reads";
  if (!why && ((c->partyLayout && c->partyLayout != &pd_party_layout_vanilla) ||
               (c->pcStorage && c->pcStorage != &pd_pc_vanilla && c->pcStorage != &pd_pc_static) || c->expansion ||
               (c->moneyOff && (int)c->moneyOff != g->f->moneyOff) || c->encryptionKeyOff != g->f->keyOff))
    why = "This game keeps its Pokemon or bag its own way, so the editor won't change its save";
  if (!why) {
    g->raw = malloc(saveSize);
    g->sb2 = calloc(1, (size_t)g->f->sb2Size);
    g->sb1 = calloc(1, (size_t)g->f->sb1Size);
    g->storage = calloc(1, STORAGE_SIZE);
    if (!g->raw || !g->sb1 || !g->sb2 || !g->storage)
      why = "Out of memory";
  }
  if (!why) {
    memcpy(g->raw, save, saveSize);
    g->rawSize = saveSize;
    enum slot_status other;
    why = g3_parse(g->f, save, saveSize, g, &g->slot, &g->counter, &other);
  }
  if (!why && rd32(g->sb1 + g->f->partyCountOff) > 6)
    why = "The save's party doesn't read right, so the editor won't change it";
  if (why) {
    say(err, errSize, why);
    g->rom = NULL; /* the caller still owns it on failure */
    free(g->raw);
    free(g->sb1);
    free(g->sb2);
    free(g->storage);
    free(g);
    free(e);
    return NULL;
  }
  g->baseStats = find_base_stats(g);
  snprintf(g->title, sizeof g->title, "%s", g->game.title ? g->game.title : g->f->label);
  e->ops = &G3_OPS;
  g->tables = retail_tables(g);
  return e;
}

/* ---- TRADE SAVES (Gen 3): the Android app's Gen3Trade, with the games' own rules (pret's trade.c
 * CanTradeSelectedMon / GetUnionRoomTradeMessageId, link.c) and what a trade does afterwards
 * (UpdatePokedexForReceivedMon, trade evolutions, Ruby / Sapphire's National Dex). PC box to PC box. ---- */

#define SPECIES_MEW 151
#define SPECIES_DEOXYS 410
#define SPECIES_SPINDA 308
#define ITEM_EVERSTONE 195
#define ITEM_ENIGMA_BERRY 175
#define DEX_OFF 0x18 /* SaveBlock2.pokedex */

/* SaveBlock1's flags / vars, its two copies of the dex's seen bits, IsNationalPokedexEnabled's three parts and
 * FireRed / LeafGreen's FLAG_SYS_CAN_LINK_WITH_RS (the decomps' global.h and constants). */
struct g3_dex {
  int flagsOff, varsOff, seen1Off, seen2Off, magicOff, magic, natVar, natVarValue, natFlag, linkRs;
  bool kanto;
};
static const struct g3_dex DEX_RS = {0x1220, 0x1340, 0x938, 0x3A8C, 0x1A, 0xDA, 0x4046, 0x302, 0x836, -1, false};
static const struct g3_dex DEX_E = {0x1270, 0x139C, 0x988, 0x3B24, 0x1A, 0xDA, 0x4046, 0x302, 0x896, -1, false};
static const struct g3_dex DEX_FRLG = {0xEE0, 0x1000, 0x5F8, 0x3A18, 0x1B, 0xB9, 0x404E, 0x6258, 0x840, 0x844, true};
static const struct g3_dex *dex_of(struct g3 *g) { return g->f == &G3_RS ? &DEX_RS : g->f == &G3_E ? &DEX_E : &DEX_FRLG; }

/* gEvolutionTable's trade evolutions: species, the item it must hold (0 = none; used up), what it becomes. */
static const struct {
  int species, item, into;
} TRADE_EVOS[] = {{61, 187, 186}, {64, 0, 65},     {67, 0, 68},     {75, 0, 76},     {79, 187, 199},  {93, 0, 94},
                  {95, 199, 208}, {117, 201, 230}, {123, 199, 212}, {137, 218, 233}, {373, 192, 374}, {373, 193, 375}};

bool se_trade_ready(struct save_edit *e) { return e && e->ops == &G3_OPS; }

static bool flag_on(struct g3 *g, int id) { return id >= 0 && (g->sb1[dex_of(g)->flagsOff + id / 8] >> (id % 8) & 1); }
static int var_of(struct g3 *g, int id) { return rd16(g->sb1 + dex_of(g)->varsOff + (id - 0x4000) * 2); }
static bool national_dex(struct g3 *g) {
  const struct g3_dex *d = dex_of(g);
  return g->sb2[d->magicOff] == d->magic && var_of(g, d->natVar) == d->natVarValue && flag_on(g, d->natFlag);
}
static bool in_regional(int species, bool kanto) {
  if (kanto)
    return species >= 1 && species <= 151;
  return species > 0 && species < 412 && SPECIES_HOENN[species] >= 1 && SPECIES_HOENN[species] <= 202;
}
static bool dex_bit(const uint8_t *base, int off, int n) { return base[off + (n - 1) / 8] >> ((n - 1) % 8) & 1; }
static void set_dex_bit(uint8_t *base, int off, int n) { base[off + (n - 1) / 8] |= (uint8_t)(1 << ((n - 1) % 8)); }
/* UpdatePokedexForReceivedMon: seen in all three copies, then caught, with Unown's / Spinda's personality kept when it's
 * the first one caught. */
static void mark_caught(struct g3 *g, int species, uint32_t pid) {
  int n = species > 0 && species < 412 ? SPECIES_NATIONAL[species] : 0;
  if (n < 1 || n > 386)
    return;
  const struct g3_dex *d = dex_of(g);
  int owned = DEX_OFF + 0x10, seen = DEX_OFF + 0x44;
  bool first = !(dex_bit(g->sb2, owned, n) && dex_bit(g->sb2, seen, n) && dex_bit(g->sb1, d->seen1Off, n) &&
                 dex_bit(g->sb1, d->seen2Off, n));
  set_dex_bit(g->sb2, seen, n);
  set_dex_bit(g->sb1, d->seen1Off, n);
  set_dex_bit(g->sb1, d->seen2Off, n);
  if (first) {
    set_dex_bit(g->sb2, owned, n);
    if (species == SPECIES_UNOWN)
      wr32(g->sb2 + DEX_OFF + 0x04, pid);
    if (species == SPECIES_SPINDA)
      wr32(g->sb2 + DEX_OFF + 0x08, pid);
  }
}
/* Ruby / Sapphire's EnableNationalPokedex: what a trade with FireRed / LeafGreen / Emerald does for them. */
static void enable_national(struct g3 *g) {
  const struct g3_dex *d = dex_of(g);
  g->sb2[d->magicOff] = (uint8_t)d->magic;
  wr16(g->sb1 + d->varsOff + (d->natVar - 0x4000) * 2, (unsigned)d->natVarValue);
  g->sb1[d->flagsOff + d->natFlag / 8] |= (uint8_t)(1 << (d->natFlag % 8));
  g->sb2[DEX_OFF + 1] = 1; /* mode: national */
  g->sb2[DEX_OFF] = 0;     /* order: numerical */
}

int se_trade_slot(struct save_edit *e, int box, int slot) {
  if (!se_trade_ready(e) || box < 0)
    return -1;
  const uint8_t *r = record(G(e), box, slot);
  if (!r)
    return -1;
  struct box_mon m;
  decode_box(r, &m);
  if (!m.ok || (m.head[0x13] & 1))
    return -1;
  return rd16(m.data) ? 1 : 0;
}

const char *se_trade_link_blocker(struct save_edit *a, struct save_edit *b, char *buf, size_t n) {
  struct g3 *ga = G(a), *gb = G(b);
  if ((ga->f == &G3_FRLG) == (gb->f == &G3_FRLG))
    return NULL;
  struct save_edit *side = ga->f == &G3_FRLG ? a : b;
  if (flag_on(G(side), DEX_FRLG.linkRs))
    return NULL;
  snprintf(buf, n, "%s links with Ruby, Sapphire and Emerald only after the Sevii Islands", G(side)->title);
  return buf;
}

static const char *mon_blocker(struct box_mon *m, struct save_edit *from, struct save_edit *to, char *buf, size_t n) {
  int species = rd16(m->data);
  if (!species)
    return NULL;
  const char *link = se_trade_link_blocker(from, to, buf, n);
  if (link)
    return link;
  bool egg = (rd32(m->data + 40) >> 30) & 1;
  struct save_edit *sides[2] = {from, to};
  for (int i = 0; i < 2; i++) {
    struct g3 *g = G(sides[i]);
    if (g->f == &G3_RS || national_dex(g))
      continue;
    if (egg) {
      snprintf(buf, n, "%s has no National Dex yet, so no eggs", g->title);
      return buf;
    }
    if (!in_regional(species, dex_of(g)->kanto)) {
      const char *name = g3_species_name(from, species);
      snprintf(buf, n, "%s has no National Dex yet, so no %s", g->title, name ? name : "Pokemon from outside it");
      return buf;
    }
  }
  if ((species == SPECIES_MEW || species == SPECIES_DEOXYS) && !(m->data[47] & 0x80)) {
    const char *name = g3_species_name(from, species);
    snprintf(buf, n, "This %s isn't from an event, so the games won't trade it", name ? name : "Pokemon");
    return buf;
  }
  int held = rd16(m->data + 2);
  if ((held >= 121 && held <= 132) || held == ITEM_ENIGMA_BERRY) {
    snprintf(buf, n, "Mail and Enigma Berries stay with their save: take it off in the game first");
    return buf;
  }
  return NULL;
}

const char *se_trade_blocker(struct save_edit *from, int box, int slot, struct save_edit *to, char *buf, size_t n) {
  if (!se_trade_ready(from) || !se_trade_ready(to))
    return "Trades are between GBA Pokemon saves";
  int kind = se_trade_slot(from, box, slot);
  if (kind < 0) {
    snprintf(buf, n, "A Bad Egg can't be traded");
    return buf;
  }
  if (!kind)
    return NULL;
  struct box_mon m;
  decode_box(record(G(from), box, slot), &m);
  return mon_blocker(&m, from, to, buf, n);
}

static int evolution_of(struct box_mon *m, struct save_edit *to) {
  int species = rd16(m->data), held = rd16(m->data + 2);
  if (!species || ((rd32(m->data + 40) >> 30) & 1) || held == ITEM_EVERSTONE)
    return -1;
  /* English text: a trade evolution renames by the English species name, so only English games evolve on arrival. */
  if (G(to)->game.code[3] != 'E')
    return -1;
  for (size_t i = 0; i < sizeof TRADE_EVOS / sizeof TRADE_EVOS[0]; i++)
    if (TRADE_EVOS[i].species == species && (!TRADE_EVOS[i].item || TRADE_EVOS[i].item == held))
      return (int)i;
  return -1;
}

int se_trade_evolution(struct save_edit *from, int box, int slot, struct save_edit *to) {
  if (se_trade_slot(from, box, slot) != 1 || !se_trade_ready(to))
    return 0;
  struct box_mon m;
  decode_box(record(G(from), box, slot), &m);
  int i = evolution_of(&m, to);
  return i >= 0 ? TRADE_EVOS[i].into : 0;
}

static bool same_name(const char *a, const char *b) {
  while (*a && *b && toupper((unsigned char)*a) == toupper((unsigned char)*b))
    a++, b++;
  return !*a && !*b;
}

/* M's record as it lands in TO: evolved when it trade-evolves and EVOLVE (the item it needed used up, renamed when its
 * name was the species' own), and TO's Pokedex told. */
static void arrive(struct box_mon *m, struct save_edit *from, struct save_edit *to, bool evolve, uint8_t out[BOX_MON]) {
  struct g3 *g = G(to);
  uint32_t pid = rd32(m->head);
  bool egg = (rd32(m->data + 40) >> 30) & 1;
  int species = rd16(m->data);
  if (!egg)
    mark_caught(g, species, pid);
  int i = evolve ? evolution_of(m, to) : -1;
  if (i >= 0) {
    char nick[32], oldName[64];
    pd_gen3_text(m->head + 8, 10, nick, sizeof nick);
    const char *old = g3_species_name(from, species);
    snprintf(oldName, sizeof oldName, "%s", old ? old : "");
    int into = TRADE_EVOS[i].into;
    wr16(m->data, (unsigned)into);
    if (TRADE_EVOS[i].item)
      wr16(m->data + 2, 0);
    const char *newName = g3_species_name(to, into);
    if (*oldName && same_name(nick, oldName) && newName) {
      char upper[64];
      size_t k = 0;
      for (; newName[k] && k + 1 < sizeof upper; k++)
        upper[k] = (char)toupper((unsigned char)newName[k]);
      upper[k] = 0;
      uint8_t name[10];
      if (encode_name(upper, name, 10)) {
        /* As gSpeciesNames keeps it: the name, its end, then zeros. */
        bool ended = false;
        for (int c = 0; c < 10; c++) {
          if (ended)
            name[c] = 0;
          else if (name[c] == 0xFF)
            ended = true;
        }
        memcpy(m->head + 8, name, 10);
      }
    }
    mark_caught(g, into, pid);
  }
  encode_box(m, out);
}

bool se_trade(struct save_edit *a, int boxA, int slotA, struct save_edit *b, int boxB, int slotB, bool evolve,
              char *err, size_t n) {
  if (!se_trade_ready(a) || !se_trade_ready(b) || a == b) {
    say(err, n, "Trades are between two GBA Pokemon saves");
    return false;
  }
  int ka = se_trade_slot(a, boxA, slotA), kb = se_trade_slot(b, boxB, slotB);
  if (ka < 0 || kb < 0) {
    say(err, n, "A Bad Egg can't be traded");
    return false;
  }
  if (!ka && !kb) {
    say(err, n, "There's no Pokemon there");
    return false;
  }
  uint8_t *ra = record(G(a), boxA, slotA), *rb = record(G(b), boxB, slotB);
  struct box_mon ma, mb;
  decode_box(ra, &ma);
  decode_box(rb, &mb);
  const char *why = ka ? mon_blocker(&ma, a, b, err, n) : NULL;
  if (!why && kb)
    why = mon_blocker(&mb, b, a, err, n);
  if (why) {
    if (why != err)
      say(err, n, why);
    return false;
  }
  /* Every check passed: now both sides change. */
  uint8_t toB[BOX_MON], toA[BOX_MON];
  memset(toB, 0, sizeof toB);
  memset(toA, 0, sizeof toA);
  if (ka)
    arrive(&ma, a, b, evolve, toB);
  if (kb)
    arrive(&mb, b, a, evolve, toA);
  memcpy(rb, toB, BOX_MON);
  memcpy(ra, toA, BOX_MON);
  /* Ruby / Sapphire get their National Dex from any trade with FireRed / LeafGreen / Emerald (pokeruby trade.c). */
  struct save_edit *sides[2] = {a, b};
  for (int i = 0; i < 2; i++) {
    struct g3 *g = G(sides[i]), *other = G(sides[1 - i]);
    if (g->f == &G3_RS && other->f != &G3_RS && !national_dex(g))
      enable_national(g);
  }
  a->dirty = b->dirty = true;
  return true;
}

/* The file is still the one the editor read (nothing saved it since). */
static bool unchanged(struct save_edit *e) {
  size_t size = 0;
  uint8_t *now = read_file(e->path, &size, 2 * 1024 * 1024);
  bool same = now && size == G(e)->rawSize && !memcmp(now, G(e)->raw, size);
  free(now);
  return same;
}

bool se_save_both(struct save_edit *a, struct save_edit *b, char *err, size_t n) {
  if (!se_trade_ready(a) || !se_trade_ready(b)) {
    say(err, n, "Trades are between two GBA Pokemon saves");
    return false;
  }
  if (!unchanged(a) || !unchanged(b)) {
    say(err, n, "A save changed since it was read (the game saved?); nothing written. Open the trade again.");
    return false;
  }
  /* Both built and checked before either is written. */
  uint8_t *oa = NULL, *ob = NULL;
  size_t sa = 0, sb = 0;
  bool built = a->ops->build(a, &oa, &sa, err, n) && b->ops->build(b, &ob, &sb, err, n);
  free(oa);
  free(ob);
  if (!built)
    return false;
  char backA[1100];
  if (!se_save(a, backA, sizeof backA, err, n))
    return false;
  char backB[1100];
  if (se_save(b, backB, sizeof backB, err, n))
    return true;
  /* The second couldn't be written: the first goes back, so no Pokemon is lost or copied. */
  size_t bs = 0;
  uint8_t *back = read_file(backA, &bs, 2 * 1024 * 1024);
  char tmp[1100];
  snprintf(tmp, sizeof tmp, "%s.tmp", a->path);
  bool restored = back && write_file(tmp, back, bs) && replace_file(tmp, a->path);
  free(back);
  char why[200];
  snprintf(why, sizeof why, "%s", err ? err : "");
  if (err && n)
    snprintf(err, n, restored ? "%s. The first save was put back, so nothing changed."
                              : "%s. The first save was written but couldn't be put back: its backup is beside it.",
             why);
  return false;
}

struct save_edit *(*se_gen4_opener)(const uint8_t *, size_t, const uint8_t *, size_t, char *, size_t);
