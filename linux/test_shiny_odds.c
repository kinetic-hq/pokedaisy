// SPDX-License-Identifier: GPL-3.0-or-later
// SHINY ODDS (shiny_odds.c): the new personality, and a made-up foe in made-up RAM turned shiny with all else kept.
#include "shiny_odds.h"

#include <assert.h>
#include <mgba/core/core.h>
#include <stdio.h>
#include <string.h>

static uint8_t ewram[0x40000];
static uint32_t rd8(struct mCore *c, uint32_t a, int seg) {
  (void)c, (void)seg;
  return a >= 0x02000000u && a < 0x02040000u ? ewram[a - 0x02000000u] : 0;
}
static uint32_t rd32(struct mCore *c, uint32_t a, int seg) {
  return rd8(c, a, seg) | rd8(c, a + 1, seg) << 8 | rd8(c, a + 2, seg) << 16 | rd8(c, a + 3, seg) << 24;
}
static void wr8(struct mCore *c, uint32_t a, int seg, uint8_t v) {
  (void)c, (void)seg;
  if (a >= 0x02000000u && a < 0x02040000u)
    ewram[a - 0x02000000u] = v;
}

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> (8 * i));
}
static const char ORDER[24][5] = {"GAEM", "GAME", "GEAM", "GEMA", "GMAE", "GMEA", "AGEM", "AGME", "AEGM", "AEMG", "AMGE", "AMEG",
                                  "EGAM", "EGMA", "EAGM", "EAMG", "EMGA", "EMAG", "MGAE", "MGEA", "MAGE", "MAEG", "MEGA", "MEAG"};
static int logical(char c) { return c == 'G' ? 0 : c == 'A' ? 1 : c == 'E' ? 2 : 3; }
// The box's 48 bytes in logical order, through its personality and OT ID (decrypt == encrypt).
static void box_crypt(const uint8_t *box, const uint8_t *in, uint8_t *out, bool toBox) {
  uint32_t p = le32(box), key = p ^ le32(box + 4);
  const char *order = ORDER[p % 24];
  for (int slot = 0; slot < 4; slot++)
    for (int w = 0; w < 3; w++) {
      int boxAt = slot * 12 + w * 4, dataAt = logical(order[slot]) * 12 + w * 4;
      put32(out + (toBox ? boxAt : dataAt), le32(in + (toBox ? dataAt : boxAt)) ^ key);
    }
}
static bool shiny(uint32_t p, uint32_t ot) { return ((ot >> 16) ^ (ot & 0xFFFF) ^ (p >> 16) ^ (p & 0xFFFF)) < 8; }

int main(void) {
  const uint32_t ot = 0x1234ABCD;
  uint64_t rng = 42;
  // Any species: nature (personality % 25), gender and ability (the low byte) kept, and shiny.
  for (uint32_t old = 1; old < 2000000000u; old += 123456789u) {
    uint32_t p;
    assert(shiny_personality(old, ot, 25, false, &rng, &p));
    assert(p % 25 == old % 25 && (p & 0xFF) == (old & 0xFF) && shiny(p, ot));
  }
  assert(shiny_reroll_chance(8192, 8) == 0 && shiny_reroll_chance(1, 8) == 1);
  assert(shiny_reroll_chance(128, 8) > 0.007 && shiny_reroll_chance(128, 8) < 0.008);

  // A foe at 0x02024744 (Emerald's gEnemyParty), the player's ID at 0x02000000 + 0x0A, gMain's battle flag clear.
  struct shiny_target t = {.enemy = 0x02024744, .save2 = 0x02000000, .save2Static = true, .inBattle = 0x02030000,
                           .trainerIdOff = 0x0A, .shinyBelow = 8, .stride = 100, .plainSpecies = 0x20, .flags = 0x13,
                           .eggMask = 0x04, .badEggMask = 0x01, .eggIvWord = -1, .shinyModifier = 0, .shinyBits = -1,
                           .speciesMask = 0x7FF};
  put32(ewram + 0x0A, ot);
  uint8_t data[48] = {0}, *box = ewram + 0x24744;
  data[0] = 25;                    // growth: species
  data[4] = 0x34, data[5] = 0x12;  // growth: experience
  data[12] = 33;                   // attacks: a move
  data[36 + 4] = 0x1F;             // misc: IVs
  uint32_t old = 0x5EED0042;
  put32(box, old);
  put32(box + 4, ot);
  box[0x13] = 0x02;  // has species
  unsigned sum = 0;
  for (int i = 0; i < 24; i++)
    sum += data[i * 2] | data[i * 2 + 1] << 8;
  box[0x1C] = (uint8_t)sum, box[0x1D] = (uint8_t)(sum >> 8);
  box_crypt(box, data, box + 0x20, true);
  assert(!shiny(old, ot));

  struct mCore core;
  memset(&core, 0, sizeof core);
  core.rawRead8 = rd8, core.rawRead32 = rd32, core.rawWrite8 = wr8;
  struct shiny_roller r;
  shiny_roller_init(&r, &t, 7);
  // First frame: not decided on yet (the same bytes must be there twice). Second: made shiny at 1 in 1.
  assert(shiny_tick(&r, 1, &core) == 0);
  assert(shiny_tick(&r, 1, &core) == 1);
  uint32_t now = le32(box);
  assert(now != old && shiny(now, ot) && now % 25 == old % 25 && (now & 0xFF) == (old & 0xFF));
  uint8_t back[48];
  box_crypt(box, box + 0x20, back, false);
  assert(!memcmp(back, data, sizeof data));
  // Once is enough: the same foe isn't touched again.
  assert(shiny_tick(&r, 1, &core) == 0 && shiny_tick(&r, 1, &core) == 0 && le32(box) == now);

  // A foe met with a battle under way already (a trainer's next one) is left alone.
  put32(box, old);
  box_crypt(box, data, box + 0x20, true);
  ewram[0x30000] = 2;
  shiny_roller_init(&r, &t, 7);
  shiny_tick(&r, 1, &core);
  assert(shiny_tick(&r, 1, &core) == 0 && le32(box) == old);
  puts("shiny odds ok");
  return 0;
}
