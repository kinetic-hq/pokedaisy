// The low PP warning (pd_move_low_pp): a quarter of the base PP on vanilla moves, 2 where the base isn't known.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pd_game.h"
#include "pd_tables.h"

int main(void) {
    struct pd_game g;
    memset(&g, 0, sizeof g);
    g.kind = PD_GAME_FIRERED;
    // Move 1 has 35 base PP: 8 is a quarter (rounded down), 9 isn't.
    assert(pd_move_pp[1] == 35);
    assert(pd_move_low_pp(&g, 1, 35) == 0);
    assert(pd_move_low_pp(&g, 1, 9) == 0);
    assert(pd_move_low_pp(&g, 1, 8) == 1);
    assert(pd_move_low_pp(&g, 1, 0) == 2);
    // A 5 PP move warns at its last one.
    assert(pd_move_pp[12] == 5);
    assert(pd_move_low_pp(&g, 12, 2) == 0);
    assert(pd_move_low_pp(&g, 12, 1) == 1);
    // No move, no warning; unknown bases (no game, a Game Boy game, a move past the table) warn at 2.
    assert(pd_move_low_pp(&g, 0, 0) == 0);
    assert(pd_move_low_pp(NULL, 1, 3) == 0);
    assert(pd_move_low_pp(NULL, 1, 2) == 1);
    assert(pd_move_low_pp(&g, 600, 2) == 1);
    g.kind = PD_GAME_GB;
    assert(pd_move_low_pp(&g, 1, 8) == 0);
    puts("moves ok");
    return 0;
}
