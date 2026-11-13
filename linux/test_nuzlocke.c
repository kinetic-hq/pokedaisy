// SPDX-License-Identifier: GPL-3.0-or-later
// NUZLOCKE's tracker (pd_nuzlocke.c) on made-up samples: the app's NuzlockeTrackerTest cases.
#include "pd_nuzlocke.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct pd_nuz_sample sample(bool inBattle, bool trainer, int area, int foe, int partyCount) {
    struct pd_nuz_sample s;
    memset(&s, 0, sizeof s);
    s.inBattle = inBattle;
    s.trainerKnown = true;
    s.trainer = trainer;
    s.menuUp = inBattle;
    s.area = area;
    s.areaName = area == 1 ? "ROUTE A" : "ROUTE B";
    s.foeSpecies = foe;
    s.foeDex = foe;
    snprintf(s.foeName, sizeof s.foeName, "FOE%d", foe);
    for (int i = 0; i < partyCount; i++) {
        s.party[i].personality = 0x1000u + (uint32_t)i;
        s.party[i].species = i == 1 ? 77 : 10;
        s.party[i].level = 5;
        s.party[i].hp = s.party[i].maxHp = 20;
        snprintf(s.party[i].name, sizeof s.party[i].name, "MON%d", i);
    }
    s.partyCount = partyCount;
    return s;
}

static bool said(const struct pd_nuzlocke* n, const char* text) {
    for (int i = 0; i < n->eventCount; i++)
        if (strstr(n->events[i], text)) return true;
    return false;
}

int main(void) {
    static struct pd_nuzlocke n;
    pd_nuz_init(&n);
    n.on = n.supported = true;

    // The party the run starts with, from the start.
    struct pd_nuz_sample s = sample(false, false, 1, 0, 1);
    pd_nuz_feed(&n, &s);
    assert(n.run.monCount == 1 && !*n.run.mons[0].areaName);

    // A wild battle in ROUTE A: its encounter, NOW; caught when a new one of that kind joins the party.
    s = sample(true, false, 1, 77, 1);
    pd_nuz_feed(&n, &s);
    assert(n.run.encounterCount == 1 && n.run.encounters[0].outcome == PD_NUZ_NOW);
    assert(said(&n, "First encounter in ROUTE A: FOE77"));
    s = sample(true, false, 1, 77, 2);
    pd_nuz_feed(&n, &s);
    assert(n.run.encounters[0].outcome == PD_NUZ_CAUGHT && said(&n, "MON1 joins the run"));
    assert(n.run.monCount == 2 && !strcmp(n.run.mons[1].areaName, "ROUTE A"));
    s = sample(false, false, 1, 0, 2);
    pd_nuz_feed(&n, &s);

    // A second wild battle there: used up, no new encounter. A trainer battle elsewhere: none either.
    s = sample(true, false, 1, 50, 2);
    pd_nuz_feed(&n, &s);
    assert(n.run.encounterCount == 1 && said(&n, "ROUTE A is used up"));
    s = sample(false, false, 2, 0, 2);
    pd_nuz_feed(&n, &s);
    s = sample(true, true, 2, 60, 2);
    pd_nuz_feed(&n, &s);
    assert(n.run.encounterCount == 1);
    s = sample(false, false, 2, 0, 2);
    pd_nuz_feed(&n, &s);

    // A wild one in ROUTE B that gets away: MISSED once the battle is a few samples gone.
    s = sample(true, false, 2, 61, 2);
    pd_nuz_feed(&n, &s);
    assert(n.run.encounterCount == 2);
    for (int i = 0; i < 4; i++) {
        s = sample(false, false, 2, 0, 2);
        pd_nuz_feed(&n, &s);
    }
    assert(n.run.encounters[1].outcome == PD_NUZ_MISSED);

    // Fainting: two samples at 0 HP, then out of the run for good, healed or not.
    s = sample(false, false, 2, 0, 2);
    s.party[0].hp = 0;
    pd_nuz_feed(&n, &s);
    assert(!n.run.mons[0].dead);
    pd_nuz_feed(&n, &s);
    assert(n.run.mons[0].dead && !strcmp(n.run.mons[0].diedIn, "ROUTE B") && said(&n, "MON0 fainted"));
    s = sample(false, false, 2, 0, 2);
    pd_nuz_feed(&n, &s);
    assert(n.run.mons[0].dead && said(&n, "MON0 is out of the run: leave it in the PC"));
    assert(pd_nuz_alive(&n.run) == 1 && pd_nuz_dead(&n.run) == 1);

    // The run's text round trip, the app's format.
    static char text[65536];
    size_t len = pd_nuz_encode(&n.run, text, sizeof text);
    assert(len > 0 && !strncmp(text, "v1\nrules\t0\t1\nenc\t1\t77\tCAUGHT\tROUTE A\n", 36));
    static struct pd_nuz_run back;
    pd_nuz_decode(&back, text);
    assert(back.encounterCount == 2 && back.monCount == 2 && back.mons[0].dead && back.mons[1].personality == 0x1001);
    assert(!strcmp(back.mons[1].areaName, "ROUTE A") && back.encounters[1].outcome == PD_NUZ_MISSED);

    // By hand: an area given back, a Pokemon brought back, NEW RUN keeping the rules.
    pd_nuz_set_outcome(&n, 2, -1);
    assert(n.run.encounterCount == 1);
    pd_nuz_set_dead(&n, 0x1000, false, "");
    assert(pd_nuz_dead(&n.run) == 0);
    n.run.dupes = true;
    pd_nuz_new_run(&n);
    assert(!n.run.encounterCount && !n.run.monCount && n.run.dupes && n.run.warnings);
    puts("nuzlocke ok");
    return 0;
}
