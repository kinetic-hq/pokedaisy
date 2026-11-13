// Name / move / type tables, generated from the app's Kotlin tables by
// 3ds/tools/gen_tables.py (pd_tables_gen.c), plus small lookups over them.
#ifndef PD_TABLES_H
#define PD_TABLES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pd_move_info {
    const char* name;
    int type;
    int power;
};

struct pd_type_pair {
    int atk, def, pct;
};

// A hack's own names and moves (pd_text_gen.c, from the app's Kotlin tables), dense by id; a NULL table (or an id
// past its count, or a hole) = the hack has none there. pd_config.text NULL = retail's tables.
struct pd_text {
    const char* const* species;
    int speciesCount;
    const char* const* items;
    int itemsCount;
    const struct pd_move_info* moves;
    int movesCount;
    // Its own type numbering (expansion's Normal = 1): names by id, the chart's non-100 pairs, each species' two types.
    const char* const* typeNames;
    int typeNamesCount;
    const struct pd_type_pair* typeChart;
    int typeChartCount;
    const uint8_t (*speciesTypes)[2];
    int speciesTypesCount;
    // GetMonGender's ratio byte by species id (Gen 3 games); NULL = retail's pd_gender_ratios.
    const uint8_t* genderRatios;
    int genderRatiosCount;
    // Its map sections' names (the app's activeMapSecData for the game), by id; NULL = retail's names for its engine.
    const char* const* mapsecs;
    int mapsecsCount;
    // A FireRed 1.0 hack whose region_map.c reads its own sMapNames (the app's RomRegionMap): those names first.
    bool mapsecsRom;
    // A hack drawn on Emerald's own map with its own section numbering (Petalburg Dynasty): each section's
    // {page, x, y, w, h} on it, mapsecsCount of them (page 255 = not on the map); NULL = Emerald's.
    const uint8_t (*mapsecRects)[5];
};

struct pd_game;

extern const char* const pd_species_names[];
extern const int pd_species_names_count;
extern const struct pd_move_info pd_moves[];
extern const int pd_moves_count;
extern const uint8_t pd_move_pp[];   // vanilla gBattleMoves' base PP by move id
extern const int pd_move_pp_count;
extern const uint8_t pd_move_acc[];  // ... and accuracy (0 = never misses)
extern const int pd_move_acc_count;
extern const char* const pd_type_names[];
extern const int pd_type_names_count;
extern const struct pd_type_pair pd_type_chart[];
extern const int pd_type_chart_count;
extern const char* const pd_mapsec_firered[];
extern const int pd_mapsec_firered_count;
extern const char* const pd_mapsec_emerald[];
extern const int pd_mapsec_emerald_count;
extern const char* const pd_item_names_firered[];
extern const int pd_item_names_firered_count;
extern const char* const pd_item_names_emerald[];
extern const int pd_item_names_emerald_count;
// gBaseStats[species].genderRatio (FireRed's numbering, which Emerald shares).
extern const unsigned char pd_gender_ratios[];
extern const int pd_gender_ratios_count;
extern const unsigned char pd_species_type_table[][2];
extern const int pd_species_type_table_count;
// Generated from MonStats.kt, not a second maintained list.
extern const char* const pd_nature_names[];
extern const int pd_nature_names_count;
extern const unsigned char pd_hidden_power_types[16];

#define PD_TYPE_NONE 255

// A DS host's own types where g is NULL (retail's numbering): Origin HeartGold has Fairy as type 9, where the Gen 4
// carts keep "???", and its own 18 x 18 chart (attacker by defender, in quarters: 4 = 1x). NULL = the retail ones.
struct pd_ds_types {
    const char* name9;
    const unsigned char* chart;
};
extern const struct pd_ds_types* pd_ds_types;
// An egg, whatever its game numbers it (retail 412, Emerald Legacy 415: pd_config.eggSpecies): past every game's
// species, so a hack's species 412 (Legacy's Deoxys forms) isn't one.
#define PD_SPECIES_EGG 0x7FFF

// How many types g's chart has names for (g NULL = retail's).
int pd_type_count(const struct pd_game* g);
// A species' gender ratio in g (254 all female ...), -1 where it isn't known.
int pd_gender_ratio(const struct pd_game* g, int species);

// Names in the game's own casing: FireRed / Emerald print species and moves
// in capitals with a small é (the app's gameCase()); a hack's own tables already carry theirs. Writes into buf.
// g = the running game (NULL = retail's tables).
const char* pd_species_name(const struct pd_game* g, int species, char* buf, size_t len);
const char* pd_move_name(const struct pd_game* g, int move, char* buf, size_t len);
// g NULL (or a game on retail's numbering) = retail's names; Hidden Power's type is always retail-numbered.
const char* pd_type_name(const struct pd_game* g, int type);
int pd_move_type(const struct pd_game* g, int move);
int pd_move_power(const struct pd_game* g, int move);
// The low PP warning: 2 when a move has no PP left, 1 when it is down to a quarter of its base PP (where the base is
// known: Gen 3 games on vanilla moves) or to 2 (elsewhere), else 0. PP Ups only raise the top, so it never warns early.
int pd_move_low_pp(const struct pd_game* g, int move, int pp);
// A move's accuracy in percent, 0 when it never misses, -1 when unknown: the ROM's own move table where it is found
// (a hack's changed moves too), else vanilla's.
int pd_move_accuracy(const struct pd_game* g, int move);
// A move's kind: the game's own split where its move table has one (CFRU hacks), else Gen 3's rule by the type
// (vanilla type numbering only); a move with no power is STATUS. -1 when unknown.
enum { PD_MOVE_PHYSICAL, PD_MOVE_SPECIAL, PD_MOVE_STATUS };
int pd_move_kind(const struct pd_game* g, int move);

// One attacking type against a (possibly dual-typed) defender, in percent
// (100 = 1x) - the app's typeMultiplierPct().
int pd_type_multiplier_pct(const struct pd_game* g, int atk, int def1, int def2);
// A species' types (the app's speciesTypeData; the same type twice for one),
// PD_TYPE_NONE both when unknown.
void pd_species_type_pair(const struct pd_game* g, int species, int* t1, int* t2);

#endif
