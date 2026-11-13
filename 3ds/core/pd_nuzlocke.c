// NUZLOCKE's tracker: see pd_nuzlocke.h. NuzlockeTracker.feed, rule for rule. GPL-3.0, see ../../NOTICE.
#include "pd_nuzlocke.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pd_dex.h"
#include "pd_game.h"
#include "pd_snapshot.h"
#include "pd_tables.h"

// Fainting needs FAINT_SAMPLES samples in a row at 0 HP (one torn read mustn't end a run), an encounter settles
// SETTLE_SAMPLES samples after its battle (the catch reaches the party or the dex during it, but a sample can land in
// between), and a battle's foe is trusted after FOE_SAMPLES samples with no battle menu read.
#define FAINT_SAMPLES 2
#define SETTLE_SAMPLES 4
#define FOE_SAMPLES 4

static void copy_name(char* dst, const char* src) {
    size_t i = 0;
    for (; src && src[i] && i < PD_NUZ_NAME - 1; i++)
        dst[i] = src[i] == '\t' || src[i] == '\n' || src[i] == '\r' ? ' ' : src[i];
    dst[i] = 0;
}

void pd_nuz_init(struct pd_nuzlocke* n) {
    memset(n, 0, sizeof(*n));
    n->run.warnings = true;
    n->pendingArea = -1;
}

void pd_nuz_reset(struct pd_nuzlocke* n) {
    n->wasInBattle = n->battleJudged = false;
    n->battleSamples = 0;
    n->pendingArea = -1;
    memset(n->zeroHpWho, 0, sizeof(n->zeroHpWho));
    memset(n->zeroHpCount, 0, sizeof(n->zeroHpCount));
    memset(n->lastHpWho, 0, sizeof(n->lastHpWho));
    memset(n->lastHp, 0, sizeof(n->lastHp));
    n->eventCount = 0;
}

bool pd_nuz_supports(const struct pd_game* g) {
    return g && g->cfg && g->kind != PD_GAME_GB && g->kind != PD_GAME_NONE && g->cfg->partyLayout != &pd_party_layout_rogue;
}

static bool dex_bit(const uint8_t* bits, int national) {
    return bits && national > 0 && national <= PD_DEX_BYTES * 8 && (bits[(national - 1) >> 3] >> ((national - 1) & 7) & 1);
}

void pd_nuz_sample_of(struct pd_nuz_sample* out, const struct pd_game* g, const struct pd_snapshot* s) {
    memset(out, 0, sizeof(*out));
    out->inBattle = s->inBattle;
    out->trainerKnown = s->valid;
    out->trainer = s->isTrainer;
    out->area = s->mapsec;
    out->areaName = pd_mapsec_name(g, s->mapsec);
    if (!out->areaName) out->areaName = "";
    out->menuUp = s->battleState == PD_BATTLE_ACTION || s->battleState == PD_BATTLE_MOVE ||
                  s->battleState == PD_BATTLE_BAG_OPEN || s->battleState == PD_BATTLE_PARTY_OPEN;
    // A foe the game's tables can't name, or at no real level, is a battle read wrong: no foe, so no encounter.
    const struct pd_battle_mon* foe = &s->battlers[PD_POS_OPPONENT_LEFT];
    char buf[PD_NUZ_NAME];
    const char* name = foe->species ? pd_species_name(g, foe->species, buf, sizeof(buf)) : NULL;
    if (s->inBattle && foe->species && foe->level >= 1 && foe->level <= 100 && name && *name && *name != '#') {
        out->foeSpecies = foe->species;
        copy_name(out->foeName, name);
        int national = pd_dex_national(g, foe->species);
        out->foeDex = national > 0 ? national : foe->species;
    }
    out->caughtKnown = s->dexOk;
    out->caught = s->dexOk ? s->dexCaught : NULL;
    for (int i = 0; i < s->partyCount && i < 6; i++) {
        const struct pd_mon* m = &s->party[i];
        struct pd_nuz_party_mon* p = &out->party[out->partyCount++];
        p->personality = m->personality;
        p->species = m->species;
        p->level = m->level;
        p->hp = m->hp;
        p->maxHp = m->maxHp;
        p->egg = m->egg || m->species == PD_SPECIES_EGG;
        copy_name(p->name, *m->nickname ? m->nickname : pd_species_name(g, m->species, buf, sizeof(buf)));
    }
}

const struct pd_nuz_encounter* pd_nuz_encounter_at(const struct pd_nuz_run* r, int area) {
    for (int i = 0; i < r->encounterCount; i++)
        if (r->encounters[i].area == area) return &r->encounters[i];
    return NULL;
}

static struct pd_nuz_mon* mon_of(struct pd_nuz_run* r, uint32_t personality) {
    if (!personality) return NULL;
    for (int i = 0; i < r->monCount; i++)
        if (r->mons[i].personality == personality) return &r->mons[i];
    return NULL;
}
const struct pd_nuz_mon* pd_nuz_mon_of(const struct pd_nuz_run* r, uint32_t personality) {
    return mon_of((struct pd_nuz_run*)r, personality);
}

bool pd_nuz_is_dead(const struct pd_nuzlocke* n, uint32_t personality) {
    if (!n || !n->on || !n->supported) return false;
    const struct pd_nuz_mon* m = pd_nuz_mon_of(&n->run, personality);
    return m && m->dead;
}

int pd_nuz_alive(const struct pd_nuz_run* r) {
    int k = 0;
    for (int i = 0; i < r->monCount; i++) k += !r->mons[i].dead;
    return k;
}
int pd_nuz_dead(const struct pd_nuz_run* r) { return r->monCount - pd_nuz_alive(r); }

static void event(struct pd_nuzlocke* n, const char* fmt, const char* a, const char* b) {
    if (n->eventCount >= PD_NUZ_EVENTS) return;
    char* e = n->events[n->eventCount++];
    if (b) snprintf(e, PD_NUZ_EVENT_LEN, fmt, a, b);
    else snprintf(e, PD_NUZ_EVENT_LEN, fmt, a);
}

static void settle(struct pd_nuzlocke* n, const struct pd_nuz_sample* s) {
    const struct pd_nuz_party_mon* joined = NULL;
    for (int i = 0; i < s->partyCount && !joined; i++) {
        const struct pd_nuz_party_mon* p = &s->party[i];
        bool before = false;
        for (int k = 0; k < n->partyBeforeCount; k++) before |= n->partyBefore[k] == p->personality;
        if (p->personality && !before && p->species == n->pendingSpecies) joined = p;
    }
    bool toPc = n->caughtBeforeKnown && !n->caughtBefore && s->caughtKnown && dex_bit(s->caught, n->pendingDex);
    enum pd_nuz_outcome outcome;
    if (joined || toPc) outcome = PD_NUZ_CAUGHT;
    else if (s->inBattle) return;
    else if (--n->settleLeft > 0) return;
    else outcome = PD_NUZ_MISSED;
    int area = n->pendingArea;
    n->pendingArea = -1;
    // Changed on the tab meanwhile (cleared or set by hand): the player's word stands.
    struct pd_nuz_encounter* e = (struct pd_nuz_encounter*)pd_nuz_encounter_at(&n->run, area);
    if (!e || e->outcome != PD_NUZ_NOW) return;
    e->outcome = outcome;
    n->dirty = true;
    if (outcome == PD_NUZ_CAUGHT) {
        event(n, "%s joins the run", joined ? joined->name : n->pendingFoe, NULL);
        // The catch joined where it was met (the roster saw it first in this sample).
        struct pd_nuz_mon* m = joined ? mon_of(&n->run, joined->personality) : NULL;
        if (m) copy_name(m->areaName, e->areaName);
    }
}

static int slot_of(uint32_t* who, uint32_t personality) {
    for (int i = 0; i < 6; i++)
        if (who[i] == personality) return i;
    for (int i = 0; i < 6; i++)
        if (!who[i]) {
            who[i] = personality;
            return i;
        }
    return -1;
}

void pd_nuz_feed(struct pd_nuzlocke* n, const struct pd_nuz_sample* s) {
    n->eventCount = 0;
    // The title screen, a save not loaded yet: nothing to follow.
    if (s->partyCount <= 0) return;
    struct pd_nuz_run* r = &n->run;
    const struct pd_nuz_party_mon* party[6];
    int pc = 0;
    for (int i = 0; i < s->partyCount; i++)
        if (s->party[i].personality && s->party[i].species && !s->party[i].egg) party[pc++] = &s->party[i];

    // New faces join the run where they're first seen; the living keep their name and level up to date. The party the
    // run starts with has been along from the start (no area).
    bool first = r->monCount == 0;
    for (int i = 0; i < pc; i++) {
        const struct pd_nuz_party_mon* p = party[i];
        struct pd_nuz_mon* m = mon_of(r, p->personality);
        if (!m && r->monCount < PD_NUZ_MAX_MONS) {
            m = &r->mons[r->monCount++];
            memset(m, 0, sizeof(*m));
            m->personality = p->personality;
            m->species = p->species;
            m->level = p->level;
            copy_name(m->name, p->name);
            copy_name(m->areaName, first ? "" : s->areaName);
            n->dirty = true;
        } else if (m && !m->dead && (strcmp(m->name, p->name) || m->level != p->level || m->species != p->species)) {
            copy_name(m->name, p->name);
            m->level = p->level;
            m->species = p->species;
            n->dirty = true;
        }
    }

    if (s->inBattle && !n->wasInBattle) {
        n->battleJudged = false;
        n->battleSamples = 0;
    }
    if (s->inBattle) n->battleSamples++;
    // A wild battle's first foe: the area's encounter, if it hasn't had one. Not on the battle's first samples (it opens
    // with the last battle's foe still in memory): at its menu, or after a few samples without one.
    bool foeIsThisBattles = s->menuUp || n->battleSamples > FOE_SAMPLES;
    if (s->inBattle && !n->battleJudged && foeIsThisBattles && s->foeSpecies && s->trainerKnown) {
        n->battleJudged = true;
        for (int i = 0; i < pc; i++) {
            const struct pd_nuz_mon* m = mon_of(r, party[i]->personality);
            if (m && m->dead && party[i]->hp > 0) event(n, "%s is out of the run: leave it in the PC", party[i]->name, NULL);
        }
        if (!s->trainer) {
            if (pd_nuz_encounter_at(r, s->area)) {
                event(n, "%s is used up: no catching here", s->areaName, NULL);
            } else if (r->dupes && s->caughtKnown && dex_bit(s->caught, s->foeDex)) {
                event(n, "%s is caught already: this one doesn't count", s->foeName, NULL);
            } else if (r->encounterCount < PD_NUZ_MAX_ENCOUNTERS) {
                struct pd_nuz_encounter* e = &r->encounters[r->encounterCount++];
                e->area = s->area;
                e->species = s->foeSpecies;
                e->outcome = PD_NUZ_NOW;
                copy_name(e->areaName, s->areaName);
                n->pendingArea = s->area;
                n->pendingSpecies = s->foeSpecies;
                n->pendingDex = s->foeDex;
                copy_name(n->pendingFoe, s->foeName);
                n->partyBeforeCount = 0;
                for (int i = 0; i < s->partyCount; i++) n->partyBefore[n->partyBeforeCount++] = s->party[i].personality;
                n->caughtBeforeKnown = s->caughtKnown;
                n->caughtBefore = dex_bit(s->caught, s->foeDex);
                n->settleLeft = SETTLE_SAMPLES;
                n->dirty = true;
                event(n, "First encounter in %s: %s", s->areaName, s->foeName);
            }
        }
    }

    if (n->pendingArea >= 0) settle(n, s);

    // Fainting: out of the run for good. A dead Pokemon healed again gets a reminder, once each time.
    for (int i = 0; i < pc; i++) {
        const struct pd_nuz_party_mon* p = party[i];
        struct pd_nuz_mon* m = mon_of(r, p->personality);
        if (!m) continue;
        int z = slot_of(n->zeroHpWho, p->personality), l = slot_of(n->lastHpWho, p->personality);
        if (p->hp == 0 && p->maxHp > 0) {
            if (z >= 0) n->zeroHpCount[z]++;
            if (!m->dead && z >= 0 && n->zeroHpCount[z] >= FAINT_SAMPLES) {
                m->dead = true;
                copy_name(m->diedIn, s->areaName);
                m->diedLevel = p->level;
                n->dirty = true;
                event(n, "%s fainted: it's out of the run", m->name, NULL);
            }
        } else {
            if (z >= 0) n->zeroHpCount[z] = 0;
            if (m->dead && p->hp > 0 && l >= 0 && n->lastHp[l] == 0 && !s->inBattle)
                event(n, "%s is out of the run: leave it in the PC", m->name, NULL);
        }
        if (l >= 0) n->lastHp[l] = p->hp;
    }
    // The slots of Pokemon no longer in the party are let go.
    for (int k = 0; k < 6; k++) {
        bool here = false, hereL = false;
        for (int i = 0; i < pc; i++) {
            here |= n->zeroHpWho[k] == party[i]->personality;
            hereL |= n->lastHpWho[k] == party[i]->personality;
        }
        if (!here) n->zeroHpWho[k] = 0, n->zeroHpCount[k] = 0;
        if (!hereL) n->lastHpWho[k] = 0;
    }
    n->wasInBattle = s->inBattle;
    if (!r->warnings) n->eventCount = 0;
}

void pd_nuz_set_outcome(struct pd_nuzlocke* n, int area, int outcome) {
    struct pd_nuz_run* r = &n->run;
    for (int i = 0; i < r->encounterCount; i++) {
        if (r->encounters[i].area != area) continue;
        if (outcome < 0) {
            memmove(&r->encounters[i], &r->encounters[i + 1], (size_t)(r->encounterCount - i - 1) * sizeof(r->encounters[0]));
            r->encounterCount--;
        } else {
            r->encounters[i].outcome = (enum pd_nuz_outcome)outcome;
        }
        n->dirty = true;
        return;
    }
}

void pd_nuz_set_dead(struct pd_nuzlocke* n, uint32_t personality, bool dead, const char* where) {
    struct pd_nuz_mon* m = mon_of(&n->run, personality);
    if (!m) return;
    m->dead = dead;
    copy_name(m->diedIn, dead ? where : "");
    m->diedLevel = dead ? m->level : 0;
    n->dirty = true;
}

void pd_nuz_new_run(struct pd_nuzlocke* n) {
    bool dupes = n->run.dupes, warnings = n->run.warnings;
    memset(&n->run, 0, sizeof(n->run));
    n->run.dupes = dupes;
    n->run.warnings = warnings;
    n->pendingArea = -1;
    n->dirty = true;
}

static const char* const OUTCOMES[] = {"NOW", "CAUGHT", "MISSED"};

size_t pd_nuz_encode(const struct pd_nuz_run* r, char* out, size_t len) {
    size_t at = 0;
#define PUT(...)                                                                                                       \
    do {                                                                                                               \
        int w = snprintf(out + at, at < len ? len - at : 0, __VA_ARGS__);                                              \
        if (w > 0) at += (size_t)w;                                                                                    \
    } while (0)
    PUT("v1\n");
    PUT("rules\t%d\t%d\n", r->dupes ? 1 : 0, r->warnings ? 1 : 0);
    for (int i = 0; i < r->encounterCount; i++) {
        const struct pd_nuz_encounter* e = &r->encounters[i];
        PUT("enc\t%d\t%d\t%s\t%s\n", e->area, e->species, OUTCOMES[e->outcome], e->areaName);
    }
    for (int i = 0; i < r->monCount; i++) {
        const struct pd_nuz_mon* m = &r->mons[i];
        PUT("mon\t%lu\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n", (unsigned long)m->personality, m->species, m->level, m->dead ? 1 : 0,
            m->diedLevel, m->name, m->areaName, m->diedIn);
    }
#undef PUT
    return at < len ? at : len ? len - 1 : 0;
}

// The fields of one line, split on tabs in place.
static int fields(char* line, char** f, int max) {
    int n = 0;
    f[n++] = line;
    for (char* p = line; *p && n < max; p++)
        if (*p == '\t') {
            *p = 0;
            f[n++] = p + 1;
        }
    return n;
}

static bool to_int(const char* s, long long* v) {
    char* end;
    *v = strtoll(s, &end, 10);
    return *s && !*end;
}

void pd_nuz_decode(struct pd_nuz_run* r, const char* text) {
    memset(r, 0, sizeof(*r));
    r->warnings = true;
    if (!text) return;
    const char* p = text;
    char line[512];
    while (*p) {
        const char* eol = strchr(p, '\n');
        size_t n = eol ? (size_t)(eol - p) : strlen(p);
        if (n >= sizeof(line)) n = sizeof(line) - 1;
        memcpy(line, p, n);
        line[n] = 0;
        if (n && line[n - 1] == '\r') line[n - 1] = 0;
        p = eol ? eol + 1 : p + strlen(p);
        char* f[10];
        int k = fields(line, f, 10);
        long long a, b, c, d, e;
        if (!strcmp(f[0], "rules") && k >= 3) {
            r->dupes = !strcmp(f[1], "1");
            r->warnings = !strcmp(f[2], "1");
        } else if (!strcmp(f[0], "enc") && k >= 4 && to_int(f[1], &a) && to_int(f[2], &b) && r->encounterCount < PD_NUZ_MAX_ENCOUNTERS) {
            int o = -1;
            for (int i = 0; i < 3; i++)
                if (!strcmp(f[3], OUTCOMES[i])) o = i;
            if (o < 0) continue;
            struct pd_nuz_encounter* en = &r->encounters[r->encounterCount++];
            en->area = (int)a;
            en->species = (int)b;
            // A battle cut short by closing the game: missed, as it would have been.
            en->outcome = o == PD_NUZ_NOW ? PD_NUZ_MISSED : (enum pd_nuz_outcome)o;
            copy_name(en->areaName, k > 4 ? f[4] : "");
        } else if (!strcmp(f[0], "mon") && k >= 7 && to_int(f[1], &a) && to_int(f[2], &b) && to_int(f[3], &c) &&
                   to_int(f[4], &d) && to_int(f[5], &e) && r->monCount < PD_NUZ_MAX_MONS) {
            struct pd_nuz_mon* m = &r->mons[r->monCount++];
            memset(m, 0, sizeof(*m));
            m->personality = (uint32_t)a;
            m->species = (int)b;
            m->level = (int)c;
            m->dead = d == 1;
            m->diedLevel = (int)e;
            copy_name(m->name, f[6]);
            copy_name(m->areaName, k > 7 ? f[7] : "");
            copy_name(m->diedIn, k > 8 ? f[8] : "");
        }
    }
}
