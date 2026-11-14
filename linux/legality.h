/* EDIT SAVE's LEGALITY: what about a Pokemon the game itself couldn't have made, checked against the game's own tables
 * (save_edit.h's species, items, abilities, learnable moves), and the fix for each where one is safe. */
#ifndef POCKET_LEGALITY_H
#define POCKET_LEGALITY_H

#include "save_edit.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  LG_SPECIES,  /* not a species this game has */
  LG_LEVEL,    /* outside 1 to 100 */
  LG_IV,       /* an IV over 31 (INDEX the stat) */
  LG_EV,       /* an EV over 255 (INDEX the stat) */
  LG_EV_TOTAL, /* EVs over 510 in all */
  LG_MOVE,     /* a move this game doesn't have (INDEX the slot) */
  LG_UNLEARNT, /* a move outside its level, TM / HM, tutor and egg moves (INDEX the slot): an event's may be fine */
  LG_ABILITY,  /* not checked (a real catch can carry the second ability's bit); kept for lg_fix */
  LG_ITEM,     /* a held item this game doesn't have */
  LG_MET,      /* met at a level over its own */
  LG_PP_UPS,   /* PP boosts over 3 (INDEX the slot) */
  LG_FRIEND,   /* friendship over 255 */
};

struct lg_issue {
  int kind, index;
  bool hard;     /* the game couldn't have it (false: worth a look, it may be fine) */
  bool fixable;  /* lg_fix can put it right */
  char text[96]; /* what's wrong, plainly */
};

#define LG_MAX 24

/* M's issues into OUT (up to CAP): how many. 0 = LEGAL. */
int lg_check(struct save_edit *e, const struct se_mon *m, struct lg_issue *out, int cap);
/* True when every issue is only worth a look (the badge reads CHECK rather than a hard problem). */
bool lg_soft_only(const struct lg_issue *v, int n);
/* Puts ISSUE right on M where that's safe: true when it changed M. */
bool lg_fix(struct save_edit *e, struct se_mon *m, const struct lg_issue *issue);
/* Every fixable issue put right: how many. */
int lg_fix_all(struct save_edit *e, struct se_mon *m);

#ifdef __cplusplus
}
#endif

#endif
