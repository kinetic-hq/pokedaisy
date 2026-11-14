/* EDIT SAVE's STATS: a Pokemon's stats worked out the way Gen 3 and later do it, the natures' +/-, the quick EV roles
 * (a spread and a nature each, the best fit picked from the base stats) and the natures worth suggesting. Stats go in
 * se_mon's order: HP, Attack, Defense, Speed, Sp. Atk, Sp. Def. */
#ifndef POCKET_STAT_TOOLS_H
#define POCKET_STAT_TOOLS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The stat a nature raises and the one it lowers (1 to 5), both 0 for a neutral one. */
int st_nature_up(int nature);
int st_nature_down(int nature);
/* "+SPE -SP. ATK", or "NEUTRAL". */
const char *st_nature_effect(int nature, char *buf, int n);
/* The nature raising UP and lowering DOWN (1 to 5, different), or -1. */
int st_nature_of(int up, int down);
/* Short stat names: HP, ATK, DEF, SPE, SP. ATK, SP. DEF. */
const char *st_stat_short(int i);
/* The six stats at LEVEL (a base HP of 1 keeps 1 HP, as the game does). */
void st_stats(const int base[6], const int ivs[6], const int evs[6], int level, int nature, int out[6]);
/* One stat's least (0 IV, 0 EV) and most (31 IV, 252 EV), with NATURE. */
int st_stat_min(const int base[6], int stat, int level, int nature);
int st_stat_max(const int base[6], int stat, int level, int nature);

enum {
  ROLE_PHYS_SWEEPER,
  ROLE_SPEC_SWEEPER,
  ROLE_MIXED,
  ROLE_PHYS_TANK,
  ROLE_SPEC_TANK,
  ROLE_BULKY_PHYS,
  ROLE_BULKY_SPEC,
  ROLE_TRICK_ROOM,
  ROLE_BALANCED,
  ROLE_COUNT
};
const char *st_role_name(int role);
/* ROLE's spread for a Pokemon with BASE (TRICK ROOM and BALANCED lean on its better attacking stat) and the nature
 * it suggests. */
void st_role_spread(int role, const int base[6], int evs[6], int *nature);
/* "252 ATK / 252 SPE / 4 HP". */
const char *st_spread_text(const int evs[6], char *buf, int n);
/* The role that fits BASE best. */
int st_best_role(const int base[6]);
/* Up to 3 natures to suggest into OUT, the most used first: SETS (25 counts, a nature's uses in this Pokemon's sets;
 * NULL or all 0 = none) then BASE's best roles. FROM_SETS[i] says which came from the sets. How many. */
int st_recommend(const int base[6], const int *sets, int out[3], bool fromSets[3]);

#ifdef __cplusplus
}
#endif

#endif
