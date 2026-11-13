/* SHINY ODDS on the desktop: see shiny_odds.h. GPL-3.0, see ../NOTICE. */
#include "shiny_odds.h"

#include "pd_game.h"

#include <mgba/core/core.h>
#include <string.h>

#define NORMAL 8192
const int SHINY_ODDS[SHINY_CHOICES] = {NORMAL, 4096, 1024, 512, 128};

#define MON 100
#define BOX 80
#define UNOWN 201
#define WURMPLE 290
#define WURMPLE_NATIONAL 265
#define WURMPLE_FEMALE_BELOW 127

static uint64_t next(uint64_t *s) {
  *s ^= *s << 13;
  *s ^= *s >> 7;
  *s ^= *s << 17;
  return *s;
}
static double unit(uint64_t *s) { return (double)(next(s) >> 11) / 9007199254740992.0; }

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static unsigned le16(const uint8_t *p) { return p[0] | p[1] << 8; }
static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> (8 * i));
}

bool shiny_target_for(const struct pd_game *g, struct shiny_target *out) {
  memset(out, 0, sizeof *out);
  const struct pd_config *cfg = g->cfg;
  if (!cfg || g->shinyBelow <= 0 || g->kind == PD_GAME_GB)
    return false;
  const struct pd_party_layout *l = cfg->partyLayout ? cfg->partyLayout : &pd_party_layout_vanilla;
  int stride = cfg->monStride ? cfg->monStride : MON;
  if (l->size != stride)
    return false;
  bool fireRed = !strncmp(g->code, "BPR", 3) || !strncmp(g->code, "BPG", 3);
  uint32_t enemy = cfg->enemyParty ? cfg->enemyParty : fireRed ? cfg->playerParty - 6 * MON : 0;
  if (!enemy || !cfg->gMain || !cfg->saveBlock2Ptr)
    return false;
  bool plain = (g->shinyFlags & PD_SHINY_PLAIN) || l->size != MON || l->rowe || l->eliteRedux;
  out->enemy = enemy;
  out->save2 = cfg->saveBlock2Ptr;
  out->save2Static = cfg->staticSaveBlocks;
  out->inBattle = cfg->gMain + cfg->inBattleOff;
  out->trainerIdOff = (int)cfg->playerGenderOff + 2;
  out->plain = plain;
  out->national = (g->shinyFlags & PD_SHINY_NATIONAL) != 0;
  out->shinyBelow = g->shinyBelow < 8 ? 8 : g->shinyBelow;
  out->stride = stride;
  out->plainSpecies = l->rowe ? 0x1C : l->eliteRedux ? 0x10 : l->plainBox;
  out->speciesMask = l->rowe ? 0xFFF : plain ? l->plainSpeciesMask : (unsigned)cfg->speciesMask;
  if (!out->speciesMask)
    out->speciesMask = 0xFFFF;
  out->flags = l->flags;
  out->eggMask = l->eggInIvWord ? 0 : l->eggMask;
  out->badEggMask = l->eggMask == 0x04 && !l->eggInIvWord ? 0x01 : 0;
  out->eggIvWord = l->eggInIvWord ? l->plainBox + 0x28 : -1;
  out->shinyModifier = l->shinyModifier;
  out->shinyBits = l->shinyBitsMask ? l->shinyBits : -1;
  out->shinyBitsMask = l->shinyBitsMask;
  out->shinyBitSet = l->shinyBitsMask & (0u - l->shinyBitsMask);
  return out->plainSpecies + 2 <= stride && out->flags < stride;
}

void shiny_roller_init(struct shiny_roller *r, const struct shiny_target *t, uint64_t seed) {
  memset(r, 0, sizeof *r);
  r->t = *t;
  r->seen[0] = r->seen[1] = UINT64_MAX;
  r->rng = seed ? seed : 0x9E3779B97F4A7C15ull;
}

double shiny_reroll_chance(int odds, int shinyBelow) {
  if (odds >= NORMAL || odds <= 0)
    return 0;
  double base = shinyBelow / 65536.0, c = (1.0 / odds - base) / (1.0 - base);
  return c < 0 ? 0 : c > 1 ? 1 : c;
}

static uint32_t unown_letter(uint32_t p) {
  uint32_t bits = ((p >> 24) & 3) << 6 | ((p >> 16) & 3) << 4 | ((p >> 8) & 3) << 2 | (p & 3);
  return bits % 28;
}
static bool wurmple_silcoon(uint32_t p) { return (p >> 16) % 10 <= 4; }

/* Each candidate in turn: [pick] < 0 counts them, else returns the pick-th. */
static long candidates(uint32_t old, uint32_t otId, int species, bool national, long pick, uint32_t *out) {
  int wurmple = national ? WURMPLE_NATIONAL : WURMPLE;
  uint32_t tsv = (otId >> 16) ^ (otId & 0xFFFF);
  int oldLow = (int)(old & 0xFF);
  long n = 0;
  for (int low = 0; low < 256; low++) {
    if (species == UNOWN ? (low & 3) != (oldLow & 3)
        : species == wurmple ? (low & 1) != (oldLow & 1) || (low < WURMPLE_FEMALE_BELOW) != (oldLow < WURMPLE_FEMALE_BELOW)
                             : low != oldLow)
      continue;
    for (uint32_t mid = 0; mid < 256; mid++) {
      uint32_t lo = mid << 8 | (uint32_t)low;
      for (uint32_t x = 0; x < 8; x++) {
        uint32_t p = ((tsv ^ lo ^ x) << 16) | lo;
        if (p % 25 != old % 25)
          continue;
        if (species == UNOWN && unown_letter(p) != unown_letter(old))
          continue;
        if (species == wurmple && wurmple_silcoon(p) != wurmple_silcoon(old))
          continue;
        if (n++ == pick) {
          *out = p;
          return n;
        }
      }
    }
  }
  return n;
}

bool shiny_personality(uint32_t old, uint32_t otId, int species, bool national, uint64_t *rng, uint32_t *out) {
  long n = candidates(old, otId, species, national, -1, out);
  if (!n)
    return false;
  candidates(old, otId, species, national, (long)(next(rng) % (uint64_t)n), out);
  return true;
}

/* Gen 3's substruct orders: for each personality % 24, the substruct (growth, attacks, EVs, misc) in each slot. */
static const char ORDER[24][5] = {"GAEM", "GAME", "GEAM", "GEMA", "GMAE", "GMEA", "AGEM", "AGME", "AEGM", "AEMG", "AMGE", "AMEG",
                                  "EGAM", "EGMA", "EAGM", "EAMG", "EMGA", "EMAG", "MGAE", "MGEA", "MAGE", "MAEG", "MEGA", "MEAG"};
static int logical(char c) { return c == 'G' ? 0 : c == 'A' ? 1 : c == 'E' ? 2 : 3; }

/* An encrypted box's 48 bytes in logical order; false if its checksum doesn't hold. */
static bool decrypt(const uint8_t *box, uint8_t data[48]) {
  uint32_t p = le32(box), key = p ^ le32(box + 4);
  const char *order = ORDER[p % 24];
  for (int slot = 0; slot < 4; slot++)
    for (int w = 0; w < 3; w++)
      put32(data + logical(order[slot]) * 12 + w * 4, le32(box + 0x20 + slot * 12 + w * 4) ^ key);
  unsigned sum = 0;
  for (int i = 0; i < 24; i++)
    sum = (sum + le16(data + i * 2)) & 0xFFFF;
  return sum == le16(box + 0x1C);
}

static void encrypt(uint8_t *box, const uint8_t data[48], uint32_t personality) {
  put32(box, personality);
  uint32_t key = personality ^ le32(box + 4);
  const char *order = ORDER[personality % 24];
  for (int slot = 0; slot < 4; slot++)
    for (int w = 0; w < 3; w++)
      put32(box + 0x20 + slot * 12 + w * 4, le32(data + logical(order[slot]) * 12 + w * 4) ^ key);
}

static bool in_ram(uint32_t a) { return (a >= 0x02000000u && a < 0x02040000u) || (a >= 0x03000000u && a < 0x03008000u); }

static void read_bytes(struct mCore *c, uint32_t a, uint8_t *out, int n) {
  for (int i = 0; i < n; i++)
    out[i] = (uint8_t)c->rawRead8(c, a + i, -1);
}
static void write_bytes(struct mCore *c, uint32_t a, const uint8_t *in, int n) {
  for (int i = 0; i < n; i++)
    c->rawWrite8(c, a + i, -1, in[i]);
}

int shiny_tick(struct shiny_roller *r, int odds, struct mCore *c) {
  const struct shiny_target *t = &r->t;
  int changed = 0;
  for (int slot = 0; slot < 2; slot++) {
    uint32_t at = t->enemy + (uint32_t)(slot * t->stride);
    uint8_t rec[128];
    int size = t->plain ? (t->stride < 128 ? t->stride : 128) : BOX;
    read_bytes(c, at, rec, 8);
    uint64_t key = le32(rec) | (uint64_t)le32(rec + 4) << 32;
    if (key == r->seen[slot]) {
      r->settled[slot] = false;
      continue;
    }
    read_bytes(c, at, rec, size);
    // Decided on only once the same bytes are there two frames running (a frame can end mid-CreateMon).
    if (!r->settled[slot] || memcmp(rec, r->settling[slot], (size_t)size)) {
      memcpy(r->settling[slot], rec, (size_t)size);
      r->settled[slot] = true;
      continue;
    }
    r->settled[slot] = false;
    r->seen[slot] = key;
    double chance = shiny_reroll_chance(odds, t->shinyBelow);
    if (chance <= 0 || (c->rawRead8(c, t->inBattle, -1) & 2))
      continue;
    uint32_t save2 = t->save2Static ? t->save2 : c->rawRead32(c, t->save2, -1);
    if (!in_ram(save2))
      continue;
    uint32_t playerOt = c->rawRead32(c, save2 + (uint32_t)t->trainerIdOff, -1);
    uint32_t personality = le32(rec), otId = le32(rec + 4);
    if (otId != playerOt || !personality)
      continue;
    if (rec[t->flags] & (t->eggMask | t->badEggMask))
      continue;
    int species;
    uint8_t data[48];
    if (t->plain) {
      species = (int)(le16(rec + t->plainSpecies) & t->speciesMask);
      if (!species)
        continue;
      if (t->eggIvWord >= 0 && t->eggIvWord + 4 <= size && ((le32(rec + t->eggIvWord) >> 30) & 1))
        continue;
    } else {
      if (!decrypt(rec, data))
        continue;
      species = (int)(le16(data) & t->speciesMask);
      if (!species || ((le32(data + 36 + 4) >> 30) & 1)) // misc's IV word: bit 30 the egg flag
        continue;
    }
    if (t->shinyBits >= 0) {
      // A shiny bit of its own: set it, the personality left alone.
      uint8_t bits = rec[t->shinyBits];
      if ((bits & t->shinyBitsMask) || unit(&r->rng) >= chance)
        continue;
      bits = (uint8_t)(bits | t->shinyBitSet);
      write_bytes(c, at + (uint32_t)t->shinyBits, &bits, 1);
      changed++;
      continue;
    }
    if (t->shinyModifier > 0 && (rec[t->shinyModifier] & 0x40))
      continue;
    uint32_t sv = (otId >> 16) ^ (otId & 0xFFFF) ^ (personality >> 16) ^ (personality & 0xFFFF);
    if ((int)sv < t->shinyBelow || unit(&r->rng) >= chance)
      continue;
    uint32_t shiny;
    if (!shiny_personality(personality, otId, species, t->national, &r->rng, &shiny))
      continue;
    if (t->plain) {
      uint8_t p[4];
      put32(p, shiny);
      write_bytes(c, at, p, 4);
    } else {
      encrypt(rec, data, shiny);
      write_bytes(c, at, rec, BOX);
    }
    r->seen[slot] = shiny | (uint64_t)otId << 32;
    changed++;
  }
  return changed;
}
