#include "pd_tables.h"
#include "pd_randomized.h"

#include "pd_game.h"

#include <stdio.h>
#include <string.h>

// Upper-cases ASCII letters, leaving é (UTF-8 C3 A9) and the rest alone.
static const char* game_case(const char* src, char* buf, size_t len) {
    size_t i = 0;
    for (; src[i] && i + 1 < len; i++) {
        char c = src[i];
        buf[i] = (c >= 'a' && c <= 'z') ? (char) (c - 'a' + 'A') : c;
    }
    buf[i] = 0;
    return buf;
}

// Emerald Legacy numbers DEOXYS's other forms 412-414 (its egg 415): named and typed as DEOXYS (the game's own
// names, EmeraldTextLegacyGen.kt). No retail game has a species there (an egg is PD_SPECIES_EGG by its flag).
static int form_base(int species) {
    return species >= 412 && species <= 414 ? 410 : species;
}

static const struct pd_text* hack_text(const struct pd_game* g) {
    return g && g->cfg ? g->cfg->text : NULL;
}

const char* pd_species_name(const struct pd_game* g, int species, char* buf, size_t len) {
    if (species == 0) return "-";
    if (species == PD_SPECIES_EGG) return "EGG";
    const struct pd_text* t = hack_text(g);
    if (t && t->species) {
        if (species > 0 && species < t->speciesCount && t->species[species]) {
            snprintf(buf, len, "%s", t->species[species]);
            return buf;
        }
    } else {
        species = form_base(species);
        if (species > 0 && species < pd_species_names_count && pd_species_names[species]) {
            return game_case(pd_species_names[species], buf, len);
        }
    }
    snprintf(buf, len, "#%d", species);
    return buf;
}

// The move a hack's table (when it has one) or retail's names; NULL = none.
static const struct pd_move_info* move_info(const struct pd_game* g, int move) {
    const struct pd_text* t = hack_text(g);
    int count = t && t->moves ? t->movesCount : pd_moves_count;
    const struct pd_move_info* table = t && t->moves ? t->moves : pd_moves;
    return move > 0 && move < count && table[move].name ? &table[move] : NULL;
}

const char* pd_move_name(const struct pd_game* g, int move, char* buf, size_t len) {
    if (move <= 0) return "-";
    const struct pd_move_info* m = move_info(g, move);
    if (m) {
        if (hack_text(g) && hack_text(g)->moves) {
            snprintf(buf, len, "%s", m->name);
            return buf;
        }
        return game_case(m->name, buf, len);
    }
    snprintf(buf, len, "MOVE#%d", move);
    return buf;
}

const struct pd_ds_types* pd_ds_types = NULL;

const char* pd_type_name(const struct pd_game* g, int type) {
    const struct pd_text* t = hack_text(g);
    if (t && t->typeNames) {
        return type >= 0 && type < t->typeNamesCount && t->typeNames[type] ? t->typeNames[type] : "";
    }
    if (!g && pd_ds_types && pd_ds_types->name9 && type == 9) return pd_ds_types->name9;
    if (type >= 0 && type < pd_type_names_count && pd_type_names[type]) return pd_type_names[type];
    return "";
}

int pd_type_count(const struct pd_game* g) {
    const struct pd_text* t = hack_text(g);
    return t && t->typeNames ? t->typeNamesCount : pd_type_names_count;
}

int pd_gender_ratio(const struct pd_game* g, int species) {
    const struct pd_text* t = hack_text(g);
    if (t && t->genderRatios) return species >= 0 && species < t->genderRatiosCount ? t->genderRatios[species] : -1;
    return species >= 0 && species < pd_gender_ratios_count ? pd_gender_ratios[species] : -1;
}

int pd_move_type(const struct pd_game* g, int move) {
    int type, power;
    if (pd_randomized_move(g, move, &type, &power)) return type;
    const struct pd_move_info* m = move_info(g, move);
    return m ? m->type : PD_TYPE_NONE;
}

int pd_move_power(const struct pd_game* g, int move) {
    int type, power;
    if (pd_randomized_move(g, move, &type, &power)) return power;
    const struct pd_move_info* m = move_info(g, move);
    return m ? m->power : 0;
}

int pd_move_low_pp(const struct pd_game* g, int move, int pp) {
    if (move <= 0) return 0;
    if (pp <= 0) return 2;
    int type, power, base = 0;
    const struct pd_text* t = hack_text(g);
    if (g && g->kind != PD_GAME_GB && !(t && t->moves) && !pd_randomized_move(g, move, &type, &power) &&
        move < pd_move_pp_count)
        base = pd_move_pp[move];
    int low = base > 0 ? (base / 4 > 1 ? base / 4 : 1) : 2;
    return pp <= low ? 1 : 0;
}

// The ROM's gBattleMoves (12 bytes a move: effect, power, type, accuracy, pp, ...), found once a ROM by its first
// three moves, which every Gen 3 game and hack keeps; byte 10 is the split in CFRU's (padding in vanilla's).
#define MOVE_STRIDE 12
static const uint8_t* movesTable;
static size_t movesCount;
static bool movesSplit;
static unsigned movesLoad;
static const struct pd_game* movesGame;
static void find_moves(const struct pd_game* g) {
    if (movesGame == g && movesLoad == g->loadId) return;
    movesGame = g;
    movesLoad = g->loadId;
    movesTable = NULL;
    movesCount = 0;
    movesSplit = false;
    if (!g->rom || (g->kind != PD_GAME_FIRERED && g->kind != PD_GAME_EMERALD) || g->romSize < 64) return;
    static const uint8_t first[3][4] = { { 40, 0, 100, 35 }, { 50, 1, 100, 25 }, { 15, 0, 85, 10 } };
    const uint8_t* r = g->rom;
    for (size_t off = 0; off + 4 * MOVE_STRIDE <= g->romSize; off += 4) {
        bool ok = true;
        for (int i = 0; i < 3 && ok; i++) ok = !memcmp(r + off + (size_t) (i + 1) * MOVE_STRIDE + 1, first[i], 4);
        if (!ok) continue;
        movesTable = r + off;
        movesCount = (g->romSize - off) / MOVE_STRIDE;
        if (movesCount > 2048) movesCount = 2048;
        // The split: POUND physical, EMBER (52) special, GROWL (45) and SWORDS DANCE (14) status.
        movesSplit = movesCount > 52 && movesTable[1 * MOVE_STRIDE + 10] == 0 && movesTable[52 * MOVE_STRIDE + 10] == 1 &&
                     movesTable[45 * MOVE_STRIDE + 10] == 2 && movesTable[14 * MOVE_STRIDE + 10] == 2;
        return;
    }
}

int pd_move_accuracy(const struct pd_game* g, int move) {
    if (move <= 0) return -1;
    int type, power;
    if (g) {
        find_moves(g);
        if (movesTable && (size_t) move < movesCount) return movesTable[(size_t) move * MOVE_STRIDE + 3];
        if (g->kind == PD_GAME_GB || pd_randomized_move(g, move, &type, &power)) return -1;
    }
    const struct pd_text* t = hack_text(g);
    if (t && t->moves) return -1;
    return move < pd_move_acc_count ? pd_move_acc[move] : -1;
}

int pd_move_kind(const struct pd_game* g, int move) {
    if (move <= 0) return -1;
    if (g) {
        find_moves(g);
        if (movesTable && movesSplit && (size_t) move < movesCount) {
            int k = movesTable[(size_t) move * MOVE_STRIDE + 10];
            return k <= 2 ? k : -1;
        }
    }
    if (pd_move_power(g, move) <= 0) return PD_MOVE_STATUS;
    const struct pd_text* t = hack_text(g);
    if ((g && g->kind == PD_GAME_GB) || (t && t->typeNames)) return -1;
    int type = pd_move_type(g, move);
    // Gen 3: NORMAL to STEEL (0-8) hit physically, FIRE to DARK (10-17) specially.
    return type >= 0 && type <= 8 ? PD_MOVE_PHYSICAL : type >= 10 && type <= 17 ? PD_MOVE_SPECIAL : -1;
}

static int single_pct(const struct pd_game* g, int atk, int def) {
    int pct;
    if (pd_randomized_pct(g, atk, def, &pct)) return pct;
    if (!g && pd_ds_types && pd_ds_types->chart && atk >= 0 && atk < 18 && def >= 0 && def < 18)
        return pd_ds_types->chart[atk * 18 + def] * 25;
    const struct pd_text* t = hack_text(g);
    const struct pd_type_pair* chart = t && t->typeChart ? t->typeChart : pd_type_chart;
    int n = t && t->typeChart ? t->typeChartCount : pd_type_chart_count;
    for (int i = 0; i < n; i++) {
        if (chart[i].atk == atk && chart[i].def == def) return chart[i].pct;
    }
    return 100;
}

int pd_type_multiplier_pct(const struct pd_game* g, int atk, int def1, int def2) {
    int pct = single_pct(g, atk, def1);
    if (def2 != PD_TYPE_NONE && def2 != def1) pct = pct * single_pct(g, atk, def2) / 100;
    return pct;
}

void pd_species_type_pair(const struct pd_game* g, int species, int* t1, int* t2) {
    *t1 = *t2 = PD_TYPE_NONE;
    if (pd_randomized_types(g, species, t1, t2)) return;
    const struct pd_text* t = hack_text(g);
    if (t && t->speciesTypes) {
        if (species > 0 && species < t->speciesTypesCount) {
            *t1 = t->speciesTypes[species][0];
            *t2 = t->speciesTypes[species][1];
        }
        return;
    }
    species = form_base(species);
    if (species <= 0 || species >= pd_species_type_table_count) return;
    *t1 = pd_species_type_table[species][0];
    *t2 = pd_species_type_table[species][1];
}
