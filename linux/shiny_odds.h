/* SHINY ODDS on the desktop, ported from the Android app's ShinyOdds.kt (its notes say why each step is so): a wild
 * Pokemon turns up shiny at the odds the player picked. Each new foe is changed in gEnemyParty between the frame the
 * game makes it and the battle drawing it, its personality swapped for a shiny one that keeps its nature, gender,
 * ability, Unown letter and Wurmple branch. Retail and the hacks the app checked, by hash (pd_shiny_gen.c).
 * GPL-3.0, see ../NOTICE. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

struct mCore;
struct pd_game;

/* SHINY ODDS' choices: the game's own odds (NORMAL), then better ones. */
#define SHINY_CHOICES 5
extern const int SHINY_ODDS[SHINY_CHOICES];

struct shiny_target {
  uint32_t enemy, save2, inBattle;
  bool save2Static, plain, national;
  int trainerIdOff, shinyBelow, stride, plainSpecies, flags, eggMask, badEggMask, eggIvWord, shinyModifier, shinyBits;
  unsigned speciesMask, shinyBitsMask, shinyBitSet;
};

struct shiny_roller {
  struct shiny_target t;
  uint64_t seen[2];
  uint8_t settling[2][128];
  bool settled[2];
  uint64_t rng;
};

/* The game's foe party and player ID, or false where SHINY ODDS doesn't work (a ROM not checked, another layout). */
bool shiny_target_for(const struct pd_game *g, struct shiny_target *out);
void shiny_roller_init(struct shiny_roller *r, const struct shiny_target *t, uint64_t seed);
/* One frame: a new wild foe, not shiny and not in a battle yet, made shiny at 1 in [odds]. Returns the slots changed. */
int shiny_tick(struct shiny_roller *r, int odds, struct mCore *c);

/* The chance a foe that isn't shiny by itself is made shiny (0 when the game's own odds are as good). */
double shiny_reroll_chance(int odds, int shinyBelow);
/* A personality shiny for [otId] that decides everything [old] did for [species]; false if there's none. */
bool shiny_personality(uint32_t old, uint32_t otId, int species, bool national, uint64_t *rng, uint32_t *out);
