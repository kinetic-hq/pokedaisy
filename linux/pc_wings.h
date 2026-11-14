/* GPL-3.0, see ../NOTICE. PC FAN GAMES' wings (pc_game.h): the companion's JSON drawn with the wings' own panels.
 * Left: the party (pd_ui_party_panel_icons, as the DS games draw theirs). Right: HERE (the place and its wild
 * Pokemon), QUICK INFO (badges, money, play time) and PARTY WEAK TO; in a battle, the foe's card with its types and
 * what hits it hard. The icons are the game's own pictures, read from its folder. */
#ifndef POCKET_PC_WINGS_H
#define POCKET_PC_WINGS_H
#include <stdbool.h>

#include "pc_game.h"
#include "pd_canvas.h"

/* The modern type chart (Fairy as type 9) as pd_ds_types: set while a PC game plays, NULL after. */
void pc_wings_types(bool on);
/* GAMEDIR: where the icons' paths start. */
void pc_wing_left(struct pd_canvas *c, const struct pc_state *s, const char *gameDir);
void pc_wing_right(struct pd_canvas *c, const struct pc_state *s, const char *gameDir);
/* The pictures read so far, let go (a game closed). */
void pc_wings_free(void);
/* Play time as the wings show it: "12:05" (hours:minutes). */
void pc_play_time(long seconds, char *out, int size);
/* The sides of a PLAY ONLY game (no game data to show): WHICH 0, the clock and the battery (BATTERY < 0: none to
 * show); 1, the play time PokeDaisy has counted for the game and the zoom. Centred in the canvas. */
void pc_side_panel(struct pd_canvas *c, int which, int hour, int minute, int battery, bool charging, long playSeconds,
                   int zoomPct);
#endif
