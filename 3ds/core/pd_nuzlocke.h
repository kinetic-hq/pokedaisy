// NUZLOCKE: the app's Nuzlocke.kt (data/ and companion/), a tracker only that never touches the game or the save. The
// first wild Pokemon met in each map section is that area's one encounter (caught, or missed for good), and a Pokemon
// that faints is out of the run, even revived. The run's text is the app's (NuzlockeRun.encode), so a run reads the
// same on either. GPL-3.0, see ../../NOTICE.
#ifndef PD_NUZLOCKE_H
#define PD_NUZLOCKE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pd_game;
struct pd_snapshot;

#define PD_NUZ_MAX_ENCOUNTERS 256
#define PD_NUZ_MAX_MONS 256
#define PD_NUZ_NAME 32
#define PD_NUZ_EVENTS 8
#define PD_NUZ_EVENT_LEN 96

enum pd_nuz_outcome { PD_NUZ_NOW, PD_NUZ_CAUGHT, PD_NUZ_MISSED };

struct pd_nuz_encounter {
    int area, species;
    enum pd_nuz_outcome outcome;
    char areaName[PD_NUZ_NAME];
};

// A Pokemon of the run, by its personality: where it joined (empty: there when the run started), and once it fainted,
// where and at which level.
struct pd_nuz_mon {
    uint32_t personality;
    int species, level, diedLevel;
    bool dead;
    char name[PD_NUZ_NAME], areaName[PD_NUZ_NAME], diedIn[PD_NUZ_NAME];
};

struct pd_nuz_run {
    int encounterCount, monCount;
    struct pd_nuz_encounter encounters[PD_NUZ_MAX_ENCOUNTERS];
    struct pd_nuz_mon mons[PD_NUZ_MAX_MONS];
    bool dupes, warnings; // DUPES CLAUSE (off to start), WARNINGS (on)
};

// The tracker: the run, whether it's on for this game, and the battle being followed.
struct pd_nuzlocke {
    bool on, supported;
    bool dirty; // changed since the host last stored it
    struct pd_nuz_run run;
    // The battle being followed (NuzlockeTracker's privates).
    bool wasInBattle, battleJudged;
    int battleSamples;
    int pendingArea; // -1 = none
    int pendingSpecies, pendingDex, settleLeft;
    char pendingFoe[PD_NUZ_NAME];
    uint32_t partyBefore[6];
    int partyBeforeCount;
    bool caughtBeforeKnown;
    bool caughtBefore; // the pending foe's dex number was caught before the battle
    uint32_t zeroHpWho[6], lastHpWho[6];
    int zeroHpCount[6], lastHp[6];
    // The notices this sample made (WARNINGS on), for the game screen.
    int eventCount;
    char events[PD_NUZ_EVENTS][PD_NUZ_EVENT_LEN];
};

// A sample cut down to what the tracker reads (NuzlockeSample).
struct pd_nuz_party_mon {
    uint32_t personality;
    int species, level, hp, maxHp;
    bool egg;
    char name[PD_NUZ_NAME];
};
struct pd_nuz_sample {
    bool inBattle, trainerKnown, trainer, menuUp, caughtKnown;
    int area;
    const char* areaName;
    int foeSpecies, foeDex;
    char foeName[PD_NUZ_NAME];
    int partyCount;
    struct pd_nuz_party_mon party[6];
    const uint8_t* caught; // national - 1, a bit each; NULL when caughtKnown is false
};

void pd_nuz_init(struct pd_nuzlocke* n);
// Another ROM's run (or the first): nothing of the last battle carries over.
void pd_nuz_reset(struct pd_nuzlocke* n);
// The app's supports(): GBA games, not Emerald Rogue (a roguelike walks the same areas every run).
bool pd_nuz_supports(const struct pd_game* g);
// A sample of the running game, from its snapshot.
void pd_nuz_sample_of(struct pd_nuz_sample* out, const struct pd_game* g, const struct pd_snapshot* s);
// One sample in: the run follows it (dirty when it changed), and events[] holds its notices (none with WARNINGS off).
void pd_nuz_feed(struct pd_nuzlocke* n, const struct pd_nuz_sample* s);

const struct pd_nuz_encounter* pd_nuz_encounter_at(const struct pd_nuz_run* r, int area);
const struct pd_nuz_mon* pd_nuz_mon_of(const struct pd_nuz_run* r, uint32_t personality);
bool pd_nuz_is_dead(const struct pd_nuzlocke* n, uint32_t personality); // on, and out of the run
int pd_nuz_alive(const struct pd_nuz_run* r);
int pd_nuz_dead(const struct pd_nuz_run* r);

// The tab's changes: an encounter set by hand (outcome < 0 clears it), a Pokemon marked dead or alive, NEW RUN.
void pd_nuz_set_outcome(struct pd_nuzlocke* n, int area, int outcome);
void pd_nuz_set_dead(struct pd_nuzlocke* n, uint32_t personality, bool dead, const char* where);
void pd_nuz_new_run(struct pd_nuzlocke* n);

// NuzlockeRun.encode / decode: "v1", "rules", "enc" and "mon" lines, tab-separated. Encode returns the length written
// (len is enough at 128 bytes a row); decode skips a line that doesn't parse.
size_t pd_nuz_encode(const struct pd_nuz_run* r, char* out, size_t len);
void pd_nuz_decode(struct pd_nuz_run* r, const char* text);

#endif
