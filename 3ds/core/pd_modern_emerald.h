// Pokémon Modern Emerald v3.6: its types follow the save's own settings (the app's ModernEmerald.kt). A new game picks
// MODERN TYPES, FAIRY TYPE and a TYPE CHART, and its randomizer can shuffle species' types and the chart; the snapshot
// reads those bits each sample and the type lookups (pd_tables.c) follow them.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pd_snapshot.h"

struct pd_game;
struct pd_config;

// FAIRY, Modern Emerald's type 18 (retail's names stop at 17).
#define PD_MODERN_FAIRY 18

// The running save's settings (the snapshot keeps them; the defaults, the Classic preset, until a save is read).
void pd_modern_emerald_read(const struct pd_config* cfg, pd_read_fn read, void* ctx, uint32_t sb1, uint32_t sb2);
bool pd_modern_emerald(const struct pd_game* g);
// GetTypeBySpecies under the settings: false when G isn't Modern Emerald.
bool pd_modern_emerald_types(const struct pd_game* g, int species, int* t1, int* t2);
// Its chart under the settings (the "Gen VI+" or the "Modern" one, random matchups passed through): false when G isn't it.
bool pd_modern_emerald_pct(const struct pd_game* g, int atk, int def, int* pct);
