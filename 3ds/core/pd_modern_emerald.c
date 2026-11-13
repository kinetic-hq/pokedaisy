#include "pd_modern_emerald.h"

#include <string.h>

#include "pd_game.h"
#include "pd_tables.h"

#include "pd_modern_emerald_gen.h"

// SaveBlock1 + these (offsetof against the tag's headers): tx_Random_Chaos bit 0, tx_Random_Type bit 5,
// tx_Random_TypeEffectiveness bit 6; tx_Mode_Modern_Types bit 5, tx_Mode_Fairy_Types bit 6; tx_Mode_TypeEffectiveness bit 4.
#define RANDOM_OFF 0x3D9D
#define TYPES_OFF 0x3DA5
#define CHART_OFF 0x3DA7
#define TRAINER_ID_OFF 0x0A        // SaveBlock2's playerTrainerId (little-endian u32)
#define TYPE_PERMUTATION 0x0201E0F4 // sTypeEffectivenessList: each type's stand-in with random matchups on
#define TYPE_COUNT 19

extern const struct pd_text pd_text_modern_emerald;

static struct {
    bool modernTypes, fairyTypes, modernChart, randomTypes, randomChart, chaos;
    uint32_t trainerId;
    uint8_t perm[TYPE_COUNT];
    bool permOk;
} s;

static bool ram(uint32_t a) {
    return a >= 0x02000000 && a < 0x02040000;
}

bool pd_modern_emerald(const struct pd_game* g) {
    return g && g->cfg && g->cfg->text == &pd_text_modern_emerald;
}

void pd_modern_emerald_read(const struct pd_config* cfg, pd_read_fn read, void* ctx, uint32_t sb1, uint32_t sb2) {
    if (!cfg || cfg->text != &pd_text_modern_emerald) return;
    memset(&s, 0, sizeof s);
    uint8_t random = 0, types = 0, chart = 0, id[4] = { 0 };
    if (!ram(sb1) || !ram(sb2) || !read(ctx, sb1 + RANDOM_OFF, &random, 1) || !read(ctx, sb1 + TYPES_OFF, &types, 1) ||
        !read(ctx, sb1 + CHART_OFF, &chart, 1) || !read(ctx, sb2 + TRAINER_ID_OFF, id, 4))
        return;
    s.modernTypes = types & 0x20;
    s.fairyTypes = types & 0x40;
    s.modernChart = chart & 0x10;
    s.randomTypes = random & 0x20;
    s.randomChart = random & 0x40;
    s.chaos = random & 0x01;
    s.trainerId = (uint32_t) id[0] | (uint32_t) id[1] << 8 | (uint32_t) id[2] << 16 | (uint32_t) id[3] << 24;
    if (s.randomChart) s.permOk = read(ctx, TYPE_PERMUTATION, s.perm, TYPE_COUNT);
}

static const struct pd_modern_types* find(const struct pd_modern_types* t, int n, int species) {
    for (int i = 0; i < n; i++)
        if (t[i].species == species) return &t[i];
    return NULL;
}

// random.c's RandomSeededModulo without CHAOS: ISO_RANDOMIZE1 over the trainer ID, retried while past RAND_MAX.
static int seeded_modulo(uint32_t value, uint32_t modulo, uint32_t trainerId) {
    uint32_t randMax = 0xFFFFFFFFu - (0xFFFFFFFFu % modulo), result = 0;
    int i = 0;
    do {
        result = 1103515245u * (trainerId + value + result) + 24691u;
    } while (result >= randMax && ++i != 5);
    return (int) (result % modulo);
}

// sOneTypeChallengeValidTypes: every type but "???", in the game's order.
static const uint8_t RANDOM_TYPES[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18 };
#define N(a) ((int) (sizeof(a) / sizeof((a)[0])))

bool pd_modern_emerald_types(const struct pd_game* g, int species, int* t1, int* t2) {
    if (!pd_modern_emerald(g)) return false;
    const struct pd_text* t = &pd_text_modern_emerald;
    const struct pd_modern_types* o = NULL;
    if (!s.modernTypes) o = find(PD_MODERN_CLASSIC, N(PD_MODERN_CLASSIC), species);
    if (!o && !s.fairyTypes) o = find(PD_MODERN_NO_FAIRY, N(PD_MODERN_NO_FAIRY), species);
    if (!o && s.modernTypes) o = find(PD_MODERN_NEW, N(PD_MODERN_NEW), species);
    if (o) {
        *t1 = o->t1;
        *t2 = o->t2;
    } else if (species > 0 && species < t->speciesTypesCount) {
        *t1 = t->speciesTypes[species][0];
        *t2 = t->speciesTypes[species][1];
    } else {
        return true; // PD_TYPE_NONE, as set by the caller
    }
    if (s.randomTypes && !s.chaos) {
        *t1 = RANDOM_TYPES[seeded_modulo((uint32_t) (*t1 + 1 + species), N(RANDOM_TYPES), s.trainerId)];
        *t2 = RANDOM_TYPES[seeded_modulo((uint32_t) (*t2 + 2 + species), N(RANDOM_TYPES), s.trainerId)];
    }
    return true;
}

bool pd_modern_emerald_pct(const struct pd_game* g, int atk, int def, int* pct) {
    if (!pd_modern_emerald(g)) return false;
    const struct pd_type_pair* chart = s.modernChart ? PD_MODERN_CHART : pd_text_modern_emerald.typeChart;
    int n = s.modernChart ? N(PD_MODERN_CHART) : pd_text_modern_emerald.typeChartCount;
    bool shuffled = s.randomChart && !s.chaos && s.permOk;
    *pct = 100;
    for (int i = 0; i < n; i++) {
        int a = chart[i].atk, d = chart[i].def;
        if (shuffled) {
            a = a < TYPE_COUNT ? s.perm[a] : a;
            d = d < TYPE_COUNT ? s.perm[d] : d;
        }
        if (a == atk && d == def) {
            *pct = chart[i].pct;
            if (!shuffled) break; // Kotlin's associate keeps the last of any collision
        }
    }
    return true;
}
