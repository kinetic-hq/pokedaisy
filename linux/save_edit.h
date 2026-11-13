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
  /* Read and changed where EXTRAS is set (Gen 3): friendship (0 to 255), each move's PP Ups (0 to 3) and the EXP
   * within the level (a level change starts it at the level's least). */
  bool extras;
  int friendship;
  int ppUps[4];
  long exp;
  /* Where it came from (Gen 3, ORIGIN set): its original trainer's name and ID, the game it was caught or hatched in
   * (se_game_name) and its level then (0 = hatched from an egg). */
  bool origin;
  char ot[16];
  long tid;
  int metGame, metLevel;
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
  /* The pocket (0 .. pockets - 1) the item goes in, from the game's own item table; -1 when it isn't known or never
   * goes in the bag. NULL: the game can't tell (the pocket's ADD AN ITEM then searches every item). */
  int (*item_pocket)(struct save_edit *, int id);
  /* The moves SPECIES may know at LEVEL (by level, TM / HM, the move tutors, and what it evolved from): how many, or -1
   * when the game's lists aren't known. COMPLETE: the game is retail, so set_mon refuses a new move outside them (egg
   * moves aren't listed: a Pokemon keeps the ones it knows). NULL = not offered. */
  int (*learnable)(struct save_edit *, int species, int level, uint16_t *out, int cap, bool *complete);
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
/* The game's generation: 3 for the GBA games, 4 for the DS ones (PRESETS lists that generation's sets). */
int se_generation(struct save_edit *e);

/* TRADE SAVES (Gen 3 saves only), PC box to PC box with the games' own rules; nothing is written until se_save_both.
 * se_trade_ready: a save the trade works with. se_trade_slot: 1 a Pokemon, 0 empty, -1 unreadable (a Bad Egg). */
bool se_trade_ready(struct save_edit *e);
int se_trade_slot(struct save_edit *e, int box, int slot);
/* Why FireRed / LeafGreen and Ruby / Sapphire / Emerald can't link yet, or NULL (BUF holds the text). */
const char *se_trade_link_blocker(struct save_edit *a, struct save_edit *b, char *buf, size_t n);
/* Why the Pokemon at FROM's BOX / SLOT can't go to TO, or NULL. */
const char *se_trade_blocker(struct save_edit *from, int box, int slot, struct save_edit *to, char *buf, size_t n);
/* The species it becomes on arriving in TO (a trade evolution), or 0. */
int se_trade_evolution(struct save_edit *from, int box, int slot, struct save_edit *to);
/* The trade: A's slot and B's swap (one may be empty: a Pokemon sent over), each evolving on arrival when EVOLVE.
 * Refused, with nothing changed, when the games wouldn't. */
bool se_trade(struct save_edit *a, int boxA, int slotA, struct save_edit *b, int boxB, int slotB, bool evolve,
              char *err, size_t n);
/* MOVE (Gen 3 saves, as se_trade_ready): what's in a slot, the party's too (SE_PARTY): 1 a Pokemon or an egg, 0 empty,
 * -1 a Bad Egg or unreadable. */
int se_slot(struct save_edit *e, int box, int slot);
int se_party_count(struct save_edit *e);
/* Moves the Pokemon at FROM to TO the way the game's PC does: two Pokemon swap; one into the party goes last, with its
 * stats worked out and full HP; one out of the party closes the gap. Refused, with nothing changed, when the party
 * would have no Pokemon left that can battle, the party is full, it holds mail or a slot holds a Bad Egg. */
bool se_move(struct save_edit *e, int fromBox, int fromSlot, int toBox, int toSlot, char *err, size_t n);
/* The game a Pokemon came from (se_mon.metGame): "EMERALD" and so on, NULL when unknown. */
const char *se_game_name(int game);
/* Writes both (se_save each, after checking neither file changed since it was read); if the second can't be
 * written, the first is put back from its backup. */
bool se_save_both(struct save_edit *a, struct save_edit *b, char *err, size_t n);

#ifdef __cplusplus
}
#endif

#endif
