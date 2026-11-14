/* EDIT SAVE's PRESETS: battle sets the player imports (Showdown text, CSV or JSON, or a .zip / .rar of them) into
 * <data>/presets, listed by generation and species, and filled into a Pokemon as far as the game allows. */
#ifndef POCKET_PRESETS_H
#define POCKET_PRESETS_H

#include <stdbool.h>
#include <stddef.h>

#include "save_edit.h"

#ifdef __cplusplus
extern "C" {
#endif

struct preset {
  int gen; /* 0 = not said (a pasted set) */
  char format[32], species[32], name[48], item[32], ability[32], nature[16], tera[16];
  int evs[6], ivs[6]; /* se_mon's order: HP, Attack, Defense, Speed, Sp. Atk, Sp. Def */
  bool ivSet[6];      /* IVs the set names (the rest are 31) */
  char moves[4][32];
};

struct preset_list {
  struct preset *v;
  int count, cap;
};

/* Sets in TEXT (NAME's extension picks the reader: .csv, .json, else Showdown text); GEN for sets that don't say.
 * Returns how many were added. */
int presets_parse(const char *name, const char *text, size_t len, int gen, struct preset_list *out);
/* Every set in DIR's .txt / .csv / .json files (and its folders'), each set once. */
int presets_load_dir(const char *dir, struct preset_list *out);
void presets_free(struct preset_list *list);
/* FILE copied into DIR, or an archive (.zip, .rar) unpacked into DIR/<its name>. MSG says what happened. */
bool presets_import(const char *file, const char *dir, char *msg, size_t n);

/* GET SETS: Smogon's sets for generations 1 to PRESETS_GENS, downloaded into DIR/smogon (PROGRESS called before each
 * generation). Returns how many sets came, -1 when none did; MSG says what happened. */
#define PRESETS_GENS 9
int presets_fetch(const char *dir, void (*progress)(int gen, void *ctx), void *ctx, char *msg, size_t n);

/* P is a set for the species named SPECIES (a form's set counts for its base species). */
bool preset_for(const struct preset *p, const char *species);
/* The species P names in E's game (forms fall back to the base species), or -1. */
int preset_species(struct save_edit *e, const struct preset *p);

#define PRESET_NOTES 10
struct preset_notes {
  int count;
  char line[PRESET_NOTES][96];
};
/* P filled into M as far as E's game allows: species (when it differs), moves it can learn, nature, EVs, IVs (Hidden
 * Power's type through them), held item and ability. NOTES gets a line for each thing left out. Returns how many of
 * those were filled. */
int preset_apply(struct save_edit *e, const struct preset *p, struct se_mon *m, struct preset_notes *notes);
/* What of a filled-in set to keep (EDIT SAVE's COPY WHAT?): its parts, ALL being the whole of it (its species too). */
enum { PART_NATURE = 1, PART_IVS = 2, PART_EVS = 4, PART_MOVES = 8, PART_ITEM = 16, PART_ABILITY = 32,
       PART_ALL = 63, PART_STATS = PART_NATURE | PART_IVS | PART_EVS };
/* The parts of NEXT (M with a set filled in by preset_apply) that PARTS names, onto M; the rest of M stays. */
void preset_take(struct se_mon *m, const struct se_mon *next, int parts);

#ifdef __cplusplus
}
#endif

#endif
