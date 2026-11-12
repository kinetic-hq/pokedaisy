/* The wings' toasts: a short line over a wing when something happens in the game (a level up, a new area, the
 * game saved, the spray wore off), worked out from one frame's facts to the next. Pure: test_bezel.c checks it. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WING_TOAST_MS 2600u   // how long a toast stays up
#define WING_TOAST_IN_MS 160u  // its fade in
#define WING_TOAST_OUT_MS 420u // its fade out
#define WING_TOAST_PARTY 6

struct wing_toast_mon {
  uint32_t key; // personality ^ species: the same Pokemon whatever its place
  int level;
  char name[24];
};
struct wing_toast_in {
  bool valid;        // a snapshot with a party read
  bool battle;       // no new area in a battle
  bool saved;        // the game (or a state) saved this frame
  int mapsec;
  const char *place; // the map section's name, NULL when unknown
  int repel;         // steps left on the spray, 0 = none
  int partyCount;
  struct wing_toast_mon party[WING_TOAST_PARTY];
};
struct wing_toasts {
  bool primed; // a first frame seen: nothing before it counts as a change
  int mapsec, repel, partyCount;
  struct wing_toast_mon party[WING_TOAST_PARTY];
  char head[16]; // what kind of news: LEVEL UP, NEW AREA, SAVED, REPEL
  char text[48]; // the news itself
  uint32_t at;   // when it went up, 0 = none
};

static inline void wing_toast_say(struct wing_toasts *t, const char *head, const char *text, uint32_t now) {
  snprintf(t->head, sizeof t->head, "%s", head);
  snprintf(t->text, sizeof t->text, "%s", text);
  t->at = now ? now : 1;
}

/* What changed since the last frame: the newest event wins (a level up over a new area over a save). */
static inline void wing_toast_watch(struct wing_toasts *t, const struct wing_toast_in *in, uint32_t now) {
  if (in->saved)
    wing_toast_say(t, "SAVED", "YOUR GAME", now);
  if (!in->valid)
    return;
  int count = in->partyCount < 0 ? 0 : in->partyCount > WING_TOAST_PARTY ? WING_TOAST_PARTY : in->partyCount;
  if (t->primed) {
    if (in->repel <= 0 && t->repel > 0)
      wing_toast_say(t, "REPEL", "WORE OFF", now);
    if (in->mapsec != t->mapsec && !in->battle && in->place && *in->place)
      wing_toast_say(t, "NEW AREA", in->place, now);
    for (int i = 0; i < count; i++)
      for (int j = 0; j < t->partyCount; j++)
        if (in->party[i].key == t->party[j].key && in->party[i].level > t->party[j].level &&
            t->party[j].level > 0) {
          char line[48];
          snprintf(line, sizeof line, "%.24s Lv%d", in->party[i].name, in->party[i].level);
          wing_toast_say(t, "LEVEL UP", line, now);
        }
  }
  t->primed = true;
  t->mapsec = in->mapsec;
  t->repel = in->repel;
  t->partyCount = count;
  memcpy(t->party, in->party, sizeof t->party);
}

/* The toast's strength now, 0 (gone) to 255: in quickly, held, then faded out. */
static inline int wing_toast_alpha(const struct wing_toasts *t, uint32_t now) {
  if (!t->at || !t->text[0])
    return 0;
  uint32_t age = now - t->at;
  if (age >= WING_TOAST_MS)
    return 0;
  if (age < WING_TOAST_IN_MS)
    return (int)(255u * age / WING_TOAST_IN_MS);
  if (age > WING_TOAST_MS - WING_TOAST_OUT_MS)
    return (int)(255u * (WING_TOAST_MS - age) / WING_TOAST_OUT_MS);
  return 255;
}
