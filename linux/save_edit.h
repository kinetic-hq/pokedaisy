/* SAVE EDITOR: a game's battery save read and changed with the game closed (the library's game options), the way the
 * Android app's PC BOXES / SAVE EDITOR does it (companion/data/save/). Gen 3 saves (Ruby, Sapphire, Emerald, FireRed,
 * LeafGreen and the hacks that keep their save layout) here, HeartGold / SoulSilver in dsperate/save_edit_ds.cpp.
 *
 * Every change is checked against the game's own tables (species, moves, items, pockets) and refused otherwise; a
 * write always backs the file up beside it first, writes the way the game saves (checksums, the other Gen 3 slot) and
 * reads the result back before it's kept. Nothing is written until se_save. */
#ifndef POCKET_SAVE_EDIT_H
#define POCKET_SAVE_EDIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SE_PARTY (-1)    /* se_mon's box for the party */
#define SE_POCKETS_MAX 8

struct se_item {
  int id, count;
};

struct se_mon {
  int species, level, nature, held;
  int ability; /* 0 = the species' first ability, 1 = its second */
  int moves[4];
  int ivs[6], evs[6]; /* HP, Attack, Defense, Speed, Sp. Atk, Sp. Def */
  bool shiny, egg;
  char nick[32];      /* UTF-8, as the game prints it */
  int hp, maxHp;      /* the party's (0 in a box) */
};

struct save_edit;

/* One game's way of reading and changing its save (save_edit.c's Gen 3, save_edit_ds.cpp's Gen 4). */
struct se_ops {
  const char *(*title)(struct save_edit *);
  long (*money)(struct save_edit *);
  long (*money_max)(struct save_edit *);
  bool (*set_money)(struct save_edit *, long, char *, size_t);
  long (*coins)(struct save_edit *); /* -1 = the game keeps none */
  bool (*set_coins)(struct save_edit *, long, char *, size_t);
  int (*pockets)(struct save_edit *);
  const char *(*pocket_name)(struct save_edit *, int);
  int (*pocket_slots)(struct save_edit *, int);
  bool (*pocket_item)(struct save_edit *, int, int, struct se_item *);
  /* Why the pocket's items can't be changed, NULL when they can. */
  const char *(*pocket_locked)(struct save_edit *, int);
  bool (*set_item)(struct save_edit *, int pocket, int slot, int id, int count, char *, size_t);
  int (*item_max_count)(struct save_edit *, int pocket, int id);
  int (*boxes)(struct save_edit *);
  int (*box_slots)(struct save_edit *);
  bool (*mon)(struct save_edit *, int box, int slot, struct se_mon *);
  bool (*set_mon)(struct save_edit *, int box, int slot, const struct se_mon *, char *, size_t);
  int (*heal)(struct save_edit *);
  const char *(*species_name)(struct save_edit *, int); /* NULL = not a species this game has */
  int (*species_max)(struct save_edit *);
  const char *(*move_name)(struct save_edit *, int);
  int (*move_max)(struct save_edit *);
  const char *(*item_name)(struct save_edit *, int);
  int (*item_max)(struct save_edit *);
  /* The species' two abilities' names (NULL second = it has one). */
  bool (*abilities)(struct save_edit *, int species, const char **first, const char **second);
  /* The changed save's bytes (whole file), or false with why. */
  bool (*build)(struct save_edit *, uint8_t **out, size_t *size, char *, size_t);
  /* The file's bytes read back: the same save as built? */
  bool (*verify)(struct save_edit *, const uint8_t *bytes, size_t size, char *, size_t);
  void (*free)(struct save_edit *);
};

struct save_edit {
  const struct se_ops *ops;
  void *impl;
  char path[1024];
  bool dirty;
};

/* Opens SAVE for the game in ROM (its header and tables decide the layout): NULL with why in ERR when the editor
 * can't change it safely. */
struct save_edit *se_open(const char *save, const char *rom, char *err, size_t errSize);
void se_close(struct save_edit *e);
/* Backs SAVE up beside it (<save>.before-edit-<date>.sav), writes the changes through a temporary file and a rename,
 * reads them back. The backup's path in BACKUP. */
bool se_save(struct save_edit *e, char *backup, size_t backupSize, char *err, size_t errSize);
/* After an edit: a resume state left from before it (statesDir/<romFile>*.resume.ss, GBA's and DS's alike) would put
 * the old save back with CONTINUE, so each is renamed to <state>.before-edit-<stamp> (kept, never deleted) and the
 * next start boots from the edited save. Returns how many were set aside, -1 if one couldn't be. */
int se_set_resume_aside(const char *statesDir, const char *romFile, time_t now);

/* Gen 3 from bytes (tests, and se_open's GBA path). ROM is kept by the editor (not copied). */
struct save_edit *se_open_gen3(const uint8_t *save, size_t saveSize, const uint8_t *rom, size_t romSize, char *err,
                               size_t errSize);
/* HeartGold / SoulSilver (dsperate/save_edit_ds.cpp's se_open_gen4): the DS build sets it at start, NULL without one. */
extern struct save_edit *(*se_gen4_opener)(const uint8_t *save, size_t saveSize, const uint8_t *rom, size_t romSize,
                                           char *err, size_t errSize);
struct save_edit *se_open_gen4(const uint8_t *save, size_t saveSize, const uint8_t *rom, size_t romSize, char *err,
                               size_t errSize);

const char *se_nature_name(int nature);

#ifdef __cplusplus
}
#endif

#endif
