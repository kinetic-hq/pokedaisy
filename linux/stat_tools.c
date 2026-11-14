/* EDIT SAVE's STATS (stat_tools.h). */
#include "stat_tools.h"

#include <stdio.h>
#include <string.h>

enum {
  HARDY, LONELY, BRAVE, ADAMANT, NAUGHTY, BOLD, DOCILE, RELAXED, IMPISH, LAX, TIMID, HASTY, SERIOUS, JOLLY, NAIVE,
  MODEST, MILD, QUIET, BASHFUL, RASH, CALM, GENTLE, SASSY, CAREFUL, QUIRKY
};
enum { HP, ATK, DEF, SPE, SPA, SPD };

/* A nature's raised stat is the fifth it falls in, its lowered one the place in that fifth, in the order Attack,
 * Defense, Speed, Sp. Atk, Sp. Def: se_mon's order from 1 on. */
int st_nature_up(int n) { return n >= 0 && n < 25 && n / 5 != n % 5 ? 1 + n / 5 : 0; }
int st_nature_down(int n) { return n >= 0 && n < 25 && n / 5 != n % 5 ? 1 + n % 5 : 0; }
int st_nature_of(int up, int down) {
  return up >= 1 && up <= 5 && down >= 1 && down <= 5 && up != down ? (up - 1) * 5 + (down - 1) : -1;
}

const char *st_stat_short(int i) {
  static const char *const s[6] = {"HP", "ATK", "DEF", "SPE", "SP. ATK", "SP. DEF"};
  return i >= 0 && i < 6 ? s[i] : "?";
}

const char *st_nature_effect(int nature, char *buf, int n) {
  int up = st_nature_up(nature), down = st_nature_down(nature);
  if (!up)
    snprintf(buf, (size_t)n, "NEUTRAL");
  else
    snprintf(buf, (size_t)n, "+%s -%s", st_stat_short(up), st_stat_short(down));
  return buf;
}

static int one(const int base[6], int i, int iv, int ev, int level, int nature) {
  int core = (2 * base[i] + iv + ev / 4) * level / 100;
  if (i == HP)
    return base[HP] == 1 ? 1 : core + level + 10;
  int v = core + 5;
  if (st_nature_up(nature) == i)
    v = v * 110 / 100;
  else if (st_nature_down(nature) == i)
    v = v * 90 / 100;
  return v;
}

void st_stats(const int base[6], const int ivs[6], const int evs[6], int level, int nature, int out[6]) {
  for (int i = 0; i < 6; i++)
    out[i] = one(base, i, ivs[i], evs[i], level, nature);
}
int st_stat_min(const int base[6], int stat, int level, int nature) { return one(base, stat, 0, 0, level, nature); }
int st_stat_max(const int base[6], int stat, int level, int nature) { return one(base, stat, 31, 252, level, nature); }

const char *st_role_name(int role) {
  static const char *const n[ROLE_COUNT] = {"PHYSICAL SWEEPER", "SPECIAL SWEEPER", "MIXED ATTACKER",
                                            "PHYSICAL TANK",    "SPECIAL TANK",    "BULKY ATTACKER",
                                            "BULKY SPECIAL ATTACKER", "TRICK ROOM", "BALANCED"};
  return role >= 0 && role < ROLE_COUNT ? n[role] : "?";
}

static void set(int evs[6], int hp, int atk, int def, int spe, int spa, int spd) {
  evs[HP] = hp, evs[ATK] = atk, evs[DEF] = def, evs[SPE] = spe, evs[SPA] = spa, evs[SPD] = spd;
}

void st_role_spread(int role, const int base[6], int evs[6], int *nature) {
  bool physical = base[ATK] >= base[SPA];
  switch (role) {
  case ROLE_PHYS_SWEEPER:
    set(evs, 4, 252, 0, 252, 0, 0), *nature = JOLLY;
    break;
  case ROLE_SPEC_SWEEPER:
    set(evs, 4, 0, 0, 252, 252, 0), *nature = TIMID;
    break;
  case ROLE_MIXED:
    set(evs, 0, 128, 0, 252, 128, 0), *nature = base[DEF] < base[SPD] ? HASTY : NAIVE;
    break;
  case ROLE_PHYS_TANK:
    set(evs, 252, 0, 252, 0, 0, 4), *nature = physical ? IMPISH : BOLD;
    break;
  case ROLE_SPEC_TANK:
    set(evs, 252, 0, 4, 0, 0, 252), *nature = physical ? CAREFUL : CALM;
    break;
  case ROLE_BULKY_PHYS:
    set(evs, 252, 252, 0, 0, 0, 4), *nature = ADAMANT;
    break;
  case ROLE_BULKY_SPEC:
    set(evs, 252, 0, 0, 0, 252, 4), *nature = MODEST;
    break;
  case ROLE_TRICK_ROOM:
    if (physical)
      set(evs, 252, 252, 4, 0, 0, 0), *nature = BRAVE;
    else
      set(evs, 252, 0, 4, 0, 252, 0), *nature = QUIET;
    break;
  default: { // BALANCED: an even spread, the nature raising its best stat and lowering the attack it doesn't use
    set(evs, 88, 84, 84, 84, 84, 84);
    int up = ATK;
    for (int i = ATK; i <= SPD; i++)
      if (base[i] > base[up])
        up = i;
    int down = physical ? SPA : ATK;
    if (down == up)
      down = up == ATK ? SPA : ATK;
    *nature = st_nature_of(up, down);
    break;
  }
  }
}

const char *st_spread_text(const int evs[6], char *buf, int n) {
  // The biggest first, as sets are written.
  static const int order[6] = {ATK, SPA, SPE, HP, DEF, SPD};
  int used = 0;
  if (evs[HP] > 84 && evs[ATK] == 84 && evs[DEF] == 84 && evs[SPE] == 84 && evs[SPA] == 84 && evs[SPD] == 84) {
    snprintf(buf, (size_t)n, "%d HP, 84 EACH", evs[HP]);
    return buf;
  }
  buf[0] = 0;
  for (int pass = 0; pass < 2; pass++)
    for (int k = 0; k < 6; k++) {
      int i = order[k];
      if (!evs[i] || (pass == 0) != (evs[i] >= 128))
        continue;
      used += snprintf(buf + used, (size_t)(n - used > 0 ? n - used : 0), "%s%d %s", used ? " / " : "", evs[i],
                       st_stat_short(i));
      if (used >= n)
        return buf;
    }
  if (!used)
    snprintf(buf, (size_t)n, "NO EVS");
  return buf;
}

int st_best_role(const int base[6]) {
  int atk = base[ATK], spa = base[SPA], spe = base[SPE], attack = atk > spa ? atk : spa;
  bool physical = atk >= spa;
  if (spe <= 45 && attack >= 90)
    return ROLE_TRICK_ROOM;
  if (spe >= 80 && atk >= 80 && spa >= 80 && (atk > spa ? atk - spa : spa - atk) <= 15)
    return ROLE_MIXED;
  if (spe >= 80 && attack >= 80)
    return physical ? ROLE_PHYS_SWEEPER : ROLE_SPEC_SWEEPER;
  if (attack >= 90)
    return physical ? ROLE_BULKY_PHYS : ROLE_BULKY_SPEC;
  int lo = base[0], hi = base[0];
  for (int i = 1; i < 6; i++)
    lo = base[i] < lo ? base[i] : lo, hi = base[i] > hi ? base[i] : hi;
  if (hi - lo <= 25)
    return ROLE_BALANCED;
  return base[DEF] >= base[SPD] ? ROLE_PHYS_TANK : ROLE_SPEC_TANK;
}

static int push(int out[3], bool from[3], int n, int nature, bool sets) {
  if (n >= 3 || nature < 0)
    return n;
  for (int i = 0; i < n; i++)
    if (out[i] == nature)
      return n;
  out[n] = nature, from[n] = sets;
  return n + 1;
}

int st_recommend(const int base[6], const int *sets, int out[3], bool fromSets[3]) {
  int n = 0;
  if (sets) {
    bool taken[25] = {false};
    for (int round = 0; round < 3; round++) {
      int best = -1;
      for (int k = 0; k < 25; k++)
        if (!taken[k] && sets[k] > 0 && (best < 0 || sets[k] > sets[best]))
          best = k;
      if (best < 0)
        break;
      taken[best] = true;
      n = push(out, fromSets, n, best, true);
    }
  }
  // Then the base stats: the best role's nature, and the other side of the same plan.
  int role = st_best_role(base), evs[6], nature;
  st_role_spread(role, base, evs, &nature);
  n = push(out, fromSets, n, nature, false);
  static const int partner[ROLE_COUNT] = {ADAMANT, MODEST, LONELY, BOLD, CALM, IMPISH, CALM, RELAXED, -1};
  int other = partner[role];
  if (role == ROLE_TRICK_ROOM && base[ATK] < base[SPA])
    other = SASSY;
  n = push(out, fromSets, n, other, false);
  bool physical = base[ATK] >= base[SPA];
  n = push(out, fromSets, n, physical ? JOLLY : TIMID, false);
  n = push(out, fromSets, n, physical ? ADAMANT : MODEST, false);
  return n;
}
