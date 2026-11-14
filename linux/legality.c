/* EDIT SAVE's LEGALITY (legality.h). */
#include "legality.h"

#include <stdio.h>
#include <string.h>

static const char *const STAT[6] = {"HP", "ATTACK", "DEFENSE", "SPEED", "SP. ATK", "SP. DEF"};

static void add(struct lg_issue *out, int cap, int *n, int kind, int index, bool hard, bool fixable, const char *text) {
  if (*n >= cap)
    return;
  struct lg_issue *is = &out[(*n)++];
  is->kind = kind;
  is->index = index;
  is->hard = hard;
  is->fixable = fixable;
  snprintf(is->text, sizeof is->text, "%s", text);
}

int lg_check(struct save_edit *e, const struct se_mon *m, struct lg_issue *out, int cap) {
  int n = 0;
  char t[96];
  if (m->egg)
    return 0; // an egg isn't changed by the editor
  const struct se_ops *o = e->ops;
  bool species = o->species_name(e, m->species) != NULL;
  if (!species)
    add(out, cap, &n, LG_SPECIES, 0, true, false, "This game has no such Pokemon.");
  if (m->level < 1 || m->level > 100) {
    snprintf(t, sizeof t, "Level %d: the game's levels go from 1 to 100.", m->level);
    add(out, cap, &n, LG_LEVEL, 0, true, true, t);
  }
  int total = 0;
  for (int i = 0; i < 6; i++) {
    if (m->ivs[i] > 31 || m->ivs[i] < 0) {
      snprintf(t, sizeof t, "%s IV is %d: an IV goes up to 31.", STAT[i], m->ivs[i]);
      add(out, cap, &n, LG_IV, i, true, true, t);
    }
    if (m->evs[i] > 255 || m->evs[i] < 0) {
      snprintf(t, sizeof t, "%s EVs are %d: a stat takes up to 255.", STAT[i], m->evs[i]);
      add(out, cap, &n, LG_EV, i, true, true, t);
    }
    total += m->evs[i] > 0 ? m->evs[i] : 0;
  }
  if (total > 510) {
    snprintf(t, sizeof t, "EVs come to %d: a Pokemon takes up to 510 in all.", total);
    add(out, cap, &n, LG_EV_TOTAL, 0, true, true, t);
  }
  // Moves: one the game doesn't have, then one this Pokemon can't learn (the game's own lists, retail only).
  uint16_t learn[1024];
  bool complete = false;
  int count = species && o->learnable ? o->learnable(e, m->species, m->level, learn, 1024, &complete) : -1;
  for (int s = 0; s < 4; s++) {
    int mv = m->moves[s];
    if (!mv)
      continue;
    const char *name = o->move_name(e, mv);
    if (!name || mv > o->move_max(e)) {
      snprintf(t, sizeof t, "Move %d isn't a move this game has.", s + 1);
      add(out, cap, &n, LG_MOVE, s, true, true, t);
      continue;
    }
    if (count >= 0 && complete) {
      bool known = false;
      for (int i = 0; i < count && !known; i++)
        known = learn[i] == mv;
      if (!known) {
        snprintf(t, sizeof t, "%s: not by level, TM, tutor or egg here (an event can teach it).", name);
        add(out, cap, &n, LG_UNLEARNT, s, false, false, t);
      }
    }
    if (m->extras && (m->ppUps[s] < 0 || m->ppUps[s] > 3)) {
      snprintf(t, sizeof t, "Move %d has %d PP boosts: it takes up to 3.", s + 1, m->ppUps[s]);
      add(out, cap, &n, LG_PP_UPS, s, true, true, t);
    }
  }
  // The ability isn't checked: a kind with one ability can carry the second's bit in a real catch (the game reads
  // the first then), so it's never a sign of an edit.
  if (m->held && (m->held > o->item_max(e) || !o->item_name(e, m->held)))
    add(out, cap, &n, LG_ITEM, 0, true, true, "It holds an item this game doesn't have.");
  if (m->origin && m->metLevel > m->level && m->level >= 1) {
    snprintf(t, sizeof t, "Met at level %d, over its own level %d.", m->metLevel, m->level);
    add(out, cap, &n, LG_MET, 0, true, true, t);
  }
  if (m->extras && (m->friendship < 0 || m->friendship > 255))
    add(out, cap, &n, LG_FRIEND, 0, true, true, "Friendship goes from 0 to 255.");
  return n;
}

bool lg_soft_only(const struct lg_issue *v, int n) {
  for (int i = 0; i < n; i++)
    if (v[i].hard)
      return false;
  return true;
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

bool lg_fix(struct save_edit *e, struct se_mon *m, const struct lg_issue *is) {
  struct se_mon was = *m;
  switch (is->kind) {
  case LG_LEVEL:
    m->level = clamp(m->level, 1, 100);
    break;
  case LG_IV:
    m->ivs[is->index] = clamp(m->ivs[is->index], 0, 31);
    break;
  case LG_EV:
    m->evs[is->index] = clamp(m->evs[is->index], 0, 255);
    break;
  case LG_EV_TOTAL: {
    // Trimmed from the last stat back (Sp. Def first), so the stats a set builds on keep theirs.
    int total = 0;
    for (int i = 0; i < 6; i++)
      total += m->evs[i] = clamp(m->evs[i], 0, 255);
    for (int i = 5; i >= 0 && total > 510; i--) {
      int cut = total - 510 < m->evs[i] ? total - 510 : m->evs[i];
      m->evs[i] -= cut;
      total -= cut;
    }
    break;
  }
  case LG_MOVE:
    // The slot empties; the moves after it close up, as the game keeps them.
    for (int s = is->index; s < 3; s++) {
      m->moves[s] = m->moves[s + 1];
      m->ppUps[s] = m->ppUps[s + 1];
    }
    m->moves[3] = m->ppUps[3] = 0;
    if (!m->moves[0]) {
      *m = was; // a Pokemon keeps at least one move
      return false;
    }
    break;
  case LG_PP_UPS:
    m->ppUps[is->index] = clamp(m->ppUps[is->index], 0, 3);
    break;
  case LG_ABILITY:
    m->ability = 0;
    break;
  case LG_ITEM:
    m->held = 0;
    break;
  case LG_MET:
    m->metLevel = m->level;
    break;
  case LG_FRIEND:
    m->friendship = clamp(m->friendship, 0, 255);
    break;
  default:
    return false;
  }
  (void)e;
  return memcmp(&was, m, sizeof was) != 0;
}

int lg_fix_all(struct save_edit *e, struct se_mon *m) {
  int fixed = 0;
  for (int round = 0; round < 64; round++) {
    struct lg_issue v[LG_MAX];
    int n = lg_check(e, m, v, LG_MAX), done = 0;
    for (int i = 0; i < n && !done; i++) // one at a time: a fix can move the slots the next one names
      if (v[i].fixable && lg_fix(e, m, &v[i]))
        done = 1;
    if (!done)
      break;
    fixed++;
  }
  return fixed;
}
