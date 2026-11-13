/* REPEL, PC anywhere and BIKE from the QUICK MENU: the app's FieldItems.kt (useFieldItem), Pc.kt (openPc) and
 * KeyItems.kt (the SELECT register) in C, over the config's generated field-item fields and the game's own
 * functions (pk_call). GPL-3.0, see ../NOTICE. */
#ifndef POCKET_FIELD_ITEMS_H
#define POCKET_FIELD_ITEMS_H
#include "pd_game.h"
#include "pd_snapshot.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mCore;

enum item_result {
  ITEM_USED,
  ITEM_STILL_ACTIVE, // a Repel is still working
  ITEM_NOT_NOW,      // the field's controls held, a menu, not on the field
  ITEM_IN_BATTLE,
  ITEM_NONE_LEFT,
  ITEM_FAILED,       // the game's code isn't where the config says, or a call didn't return
  ITEM_NO_EVOLUTION, // TRADE EVOLUTIONS: it doesn't evolve by trading now
  ITEM_MOVED,        // ... the party changed under the summary
};

/* REPEL's kinds, weakest first (QuickMenuSettings.kt's REPEL_KINDS): item id, steps. */
#define REPEL_KINDS 3
extern const int repel_kinds[REPEL_KINDS][2];

/* Whether this ROM ([rom]: its bytes from 0x08000000) has the config's field functions (VarGet ... the field lock). */
bool field_items_supported(const struct pd_config *cfg, const uint8_t *rom, size_t len);
/* ... and the PC's: ScriptContext_SetupScript, gSpecials' entry. */
bool pc_supported(const struct pd_config *cfg, const uint8_t *rom, size_t len);
/* BIKE: a layout the SELECT register is known in (SaveBlock1.registeredItem = money + 6: FireRed's / Emerald's). */
bool bike_supported(const struct pd_config *cfg);

/* The steps left on the Repel working now (0 = none): VAR_REPEL_STEP_COUNT, read from SaveBlock1's vars. */
int repel_steps(struct mCore *core, const struct pd_config *cfg);
/* How many of [item] the bag holds (the snapshot's). */
int bag_count(const struct pd_snapshot *snap, int item);
/* The first bike the bag holds (FireRed's BICYCLE, Emerald's MACH / ACRO), or 0. */
int bag_bike(const struct pd_snapshot *snap);

/* Uses the Repel [item] the game's way: VarGet (none still working), CheckBagHasItem, VarSet(steps = its gItems
 * holdEffectParam), RemoveBagItem. [steps] gets its steps. Between frames, when pk_call_safe says so. */
enum item_result repel_use(struct mCore *core, const struct pd_config *cfg, const uint8_t *rom, size_t len,
                           int item, bool busy, bool battle, int *steps);
/* The game's own PC, as a PC's script opens it: lockall; special ShowPokemonStorageSystemPC; waitstate;
 * releaseall; end - written to the config's script address and run by ScriptContext_SetupScript. */
enum item_result pc_open(struct mCore *core, const struct pd_config *cfg, const uint8_t *rom, size_t len, bool busy,
                         bool battle);
/* BIKE's first step: [item] into the SELECT register; [old] gets the player's own registered item (put back with
 * bike_restore once the game took SELECT). The host then presses SELECT for the game. */
enum item_result bike_register(struct mCore *core, const struct pd_config *cfg, const uint8_t *rom, size_t len,
                               int item, bool busy, bool battle, uint16_t *old);
void bike_restore(struct mCore *core, const struct pd_config *cfg, int item, uint16_t old);
/* Whether the field's controls are held now (ArePlayerFieldControlsLocked; true if it can't be asked). */
bool field_locked(struct mCore *core, const struct pd_config *cfg);
/* TRADE EVOLUTIONS (TradeEvolution.kt): this ROM has the config's evolution functions where they're expected. */
bool trade_evo_supported(const struct pd_config *cfg, const uint8_t *rom, size_t len);
/* What each party slot would become by trade right now (0 = nothing), asked of the game on each slot with its bytes
 * put back after, so a held item isn't spent by asking; [personality] gets each slot's. Between frames only. */
bool trade_evo_targets(struct mCore *core, const struct pd_config *cfg, const uint8_t *rom, size_t len,
                       int target[6], uint32_t personality[6]);
/* Evolves party slot [slot] as a trade would: the game's checks again, then its own evolution scene, which hands the
 * field back when it ends. */
enum item_result trade_evolve(struct mCore *core, const struct pd_config *cfg, const uint8_t *rom, size_t len,
                              int slot, uint32_t personality, bool busy, bool battle);
/* What happened, in the app's words (ItemsScreen.kt's itemUseMessage). */
const char *item_reason(enum item_result r);

#endif
