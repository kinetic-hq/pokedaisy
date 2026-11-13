#pragma once
/* The bezel bars: the strips above and below the game (where the screen has room for them, or COMPANION VIEW gives
 * the game strips), a few facts each in three places - left, centre, right - that the player picks in SETTINGS >
 * DISPLAY OPTIONS > BEZEL BARS (each one or NONE). What the save doesn't give (a hack's badge flags, the play time
 * outside the card's games) leaves its place empty, never a made-up value.
 *  - BEZEL STYLE: BLACK (near-black, off-white text: the default with BACKGROUND BLACK / OLED), GLOW (the game's
 *    own picture blurred, darkened and faint behind the text) or THEME (the theme's title window, as before).
 *  - In a battle the bottom bar is the battle's: the foe and its level, how the moves out do against it, and for a
 *    wild one how a ball would do. */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum { BEZEL_STYLE_BLACK, BEZEL_STYLE_GLOW, BEZEL_STYLE_THEME, BEZEL_STYLES };

enum {
  BEZEL_NONE,
  BEZEL_LOCATION,
  BEZEL_CLOCK_BATTERY, // the clock, the handheld's battery and a charging mark
  BEZEL_CLOCK,
  BEZEL_BATTERY,
  BEZEL_BADGES_TIME, // badges and the play time
  BEZEL_BADGES,
  BEZEL_PLAY_TIME,
  BEZEL_HINTS,       // which button does what; fades after BEZEL_HINT_MS without input
  BEZEL_CHIPS,       // FF, the zoom (as a share of the game's picture), SAVED after a save
  BEZEL_REPEL_MONEY, // the repel's steps while one works, else the money
  BEZEL_MONEY,
  BEZEL_NEXT_BOSS,   // the next unbeaten boss and the level cap (its team's top level)
  BEZEL_CAUGHT_HERE, // how many of this area's wild kinds are caught
  BEZEL_LEAD,        // the lead POKéMON: its HP, and the EXP to its next level where the game's curve is known
  BEZEL_LAST_SAVED,  // how long since the game was last saved
  // LAYOUT's own (the values are stored: append only)
  BEZEL_REPEL,       // the repel's steps, only while one works
  BEZEL_CAUGHT_SEEN, // the POKEDEX's caught and seen
  BEZEL_DEATHS,      // NUZLOCKE's dead, while a run is on
  BEZEL_AREA,        // NUZLOCKE: this area's encounter used or free; off: this area's caught kinds
  BEZEL_ZOOM,        // the zoom, for a few seconds after it changes
  BEZEL_SPEED,       // the game's speed, only when it isn't 1x
  BEZEL_BOSS,        // the next unbeaten boss
  BEZEL_CAP,         // the level cap
  BEZEL_FOE,         // a battle's foe and its level
  BEZEL_MATCHUP,     // what the moves out do to it at best
  BEZEL_CATCH,       // a wild one: how likely a ball is to work
  BEZEL_ITEMS
};
enum { BEZEL_TOP_LEFT, BEZEL_TOP_CENTRE, BEZEL_TOP_RIGHT, BEZEL_BOTTOM_LEFT, BEZEL_BOTTOM_CENTRE, BEZEL_BOTTOM_RIGHT,
       BEZEL_SLOTS };

#define BEZEL_HINT_MS 5000u
#define BEZEL_SAVED_MS 3000u

static const char *const BEZEL_ITEM_NAMES[BEZEL_ITEMS] = {
    "NONE", "LOCATION", "CLOCK AND BATTERY", "CLOCK", "BATTERY", "BADGES AND PLAY TIME", "BADGES", "PLAY TIME",
    "BUTTON HINTS", "STATUS CHIPS", "REPEL OR MONEY", "MONEY", "NEXT BOSS AND CAP", "CAUGHT HERE", "LEAD POKEMON",
    "LAST SAVED", "REPEL STEPS", "CAUGHT AND SEEN", "NUZLOCKE DEATHS", "AREA ENCOUNTER", "ZOOM", "GAME SPEED",
    "NEXT LEADER", "LEVEL CAP", "FOE", "BEST MATCHUP", "CATCH CHANCE"};
/* LAYOUT's bars: each an ordered list, as many of its items as fit across it shown (one with nothing to say gives its
 * room to the rest). When they don't all fit, the lowest BEZEL_KEEP goes first (later ones before earlier on a tie),
 * the place shrinking (cut with "...") before anything goes. */
#define BEZEL_LIST_MAX 32
static const unsigned char BEZEL_KEEP[BEZEL_ITEMS] = {0, 8, 5, 6, 4, 3, 5, 1, 0, 2, 2, 2, 9, 3, 3, 1,
                                                       4, 5, 6, 6, 3, 4, 9, 9, 9, 8, 7};
static const int BEZEL_DEFAULT_TOP[] = {BEZEL_LOCATION, BEZEL_CLOCK,  BEZEL_BATTERY, BEZEL_BADGES,
                                        BEZEL_PLAY_TIME, BEZEL_MONEY, BEZEL_BOSS,    BEZEL_CAP};
static const int BEZEL_DEFAULT_BOTTOM[] = {BEZEL_CAUGHT_SEEN, BEZEL_ZOOM, BEZEL_SPEED, BEZEL_REPEL, BEZEL_AREA};
/* In a battle: the top bar is the walking one's list until you give it its own; the bottom one is the battle's. */
static const int BEZEL_DEFAULT_BATTLE_BOTTOM[] = {BEZEL_FOE, BEZEL_MATCHUP, BEZEL_CATCH};
#define BEZEL_ZOOM_MS 3000u // ZOOM's showing after the zoom changes
static const char *const BEZEL_SLOT_NAMES[BEZEL_SLOTS] = {"TOP LEFT", "TOP CENTRE", "TOP RIGHT",
                                                          "BOTTOM LEFT", "BOTTOM CENTRE", "BOTTOM RIGHT"};
static const char *const BEZEL_STYLE_NAMES[BEZEL_STYLES] = {"BLACK", "GLOW", "THEME"};
/* The top bar says where you are, the time and what's next (NEXT BOSS AND CAP); the bottom one your progress: the
 * badges, the play time and the money. */
static const int BEZEL_DEFAULT_SLOTS[BEZEL_SLOTS] = {BEZEL_LOCATION, BEZEL_CLOCK_BATTERY, BEZEL_NEXT_BOSS,
                                                     BEZEL_BADGES,   BEZEL_PLAY_TIME,     BEZEL_REPEL_MONEY};
/* What fills a place whose own item has nothing to say (unknown, or BUTTON HINTS faded), in this order, each at most
 * once a bar and never what the bar already shows: a bar is never half empty while the save has something useful. */
static const int BEZEL_FILLERS[] = {BEZEL_HINTS,     BEZEL_NEXT_BOSS, BEZEL_CAUGHT_HERE, BEZEL_LEAD,
                                    BEZEL_LAST_SAVED, BEZEL_CHIPS,     BEZEL_REPEL_MONEY, BEZEL_BADGES_TIME};
#define BEZEL_SAVE_LATE_MIN 30 // LAST SAVED turns to a warning after this many minutes

/* BEZEL STYLE in force: the setting, or by BACKGROUND (-1: BLACK on a black background, else THEME). */
static inline int bezel_style_resolve(int setting, bool blackBackground) {
  if (setting >= 0 && setting < BEZEL_STYLES)
    return setting;
  return blackBackground ? BEZEL_STYLE_BLACK : BEZEL_STYLE_THEME;
}

/* What the host knows this frame; -1 (or NULL) = unknown. */
struct bezel_facts {
  const char *place;
  int hour, minute;    // the real clock
  int battery;         // percent
  bool charging;
  int badges;          // of 8
  int playHours, playMinutes;
  bool hints;          // input lately (BEZEL_HINT_MS)
  bool companion;      // a companion to open (X)
  bool quickMenu;      // a quick menu (Y)
  int ff;              // fast-forward's speed, 0 = off
  int zoomPct;         // EXPAND's zoom as a share of the closest (100%), 0 = at the closest
  bool saved;          // a save in the last BEZEL_SAVED_MS
  long money;
  int repel;           // steps left, 0 = none working
  const char *boss;    // the next unbeaten boss, NULL = none known
  int cap;             // its team's top level, 0 = unknown
  int caughtHere, kindsHere; // kindsHere 0 = no wild list for this area
  const char *lead;    // the lead's name, NULL = no party
  int leadHp, leadMaxHp;
  long leadToNext;     // EXP to its next level, -1 = unknown (0 at the top level)
  int savedMinutes;    // since the game was last saved this run, -1 = not yet
  int dexCaught, dexSeen; // the POKEDEX's, -1 = unknown
  int nuzDeaths;       // NUZLOCKE's dead, -1 = no run on
  int nuzArea;         // this area's encounter: 0 free, 1 used, -1 = no run on (AREA then says CAUGHT HERE's)
  bool zoomRecent;     // the zoom changed in the last BEZEL_ZOOM_MS
  int pacePct;         // the game's speed off fast-forward, in percent (100 = 1x, 0 = unknown)
  const char *foe;     // a battle's: "FOE name Lv", the best matchup, a wild one's catch hint (NULL = none)
  const char *matchup;
  const char *catchHint;
};

static inline void bezel_item_text(int item, const struct bezel_facts *f, char *out, size_t n) {
  out[0] = 0;
  char a[32] = "", b[32] = "";
  switch (item) {
  case BEZEL_LOCATION:
    if (f->place && *f->place)
      snprintf(out, n, "%s", f->place);
    break;
  case BEZEL_CLOCK_BATTERY:
  case BEZEL_CLOCK:
  case BEZEL_BATTERY:
    if (item != BEZEL_BATTERY && f->hour >= 0)
      snprintf(a, sizeof a, "%02d:%02d", f->hour, f->minute);
    if (item != BEZEL_CLOCK && f->battery >= 0)
      snprintf(b, sizeof b, "%s%d%%", f->charging ? "+" : "", f->battery);
    snprintf(out, n, "%s%s%s", a, *a && *b ? "  " : "", b);
    break;
  case BEZEL_BADGES_TIME:
  case BEZEL_BADGES:
  case BEZEL_PLAY_TIME:
    if (item != BEZEL_PLAY_TIME && f->badges >= 0)
      snprintf(a, sizeof a, "BADGES %d/8", f->badges);
    if (item != BEZEL_BADGES && f->playHours >= 0 && (f->playHours || f->playMinutes > 0))
      snprintf(b, sizeof b, "%d:%02d", f->playHours, f->playMinutes);
    snprintf(out, n, "%s%s%s", a, *a && *b ? "  " : "", b);
    break;
  case BEZEL_HINTS:
    if (f->hints)
      snprintf(out, n, "%s%s%s  R3 VIEW", f->quickMenu ? "Y QUICK MENU" : "",
               f->quickMenu && f->companion ? "  " : "", f->companion ? "X COMPANION" : "");
    break;
  case BEZEL_CHIPS: {
    size_t at = 0;
    if (f->ff > 0)
      at += (size_t)snprintf(out + at, n - at, "FF x%d", f->ff);
    if (f->zoomPct > 0 && at < n)
      at += (size_t)snprintf(out + at, n - at, "%sZOOM %d%%", at ? "  " : "", f->zoomPct);
    if (f->saved && at < n)
      snprintf(out + at, n - at, "%sSAVED", at ? "  " : "");
    break;
  }
  case BEZEL_REPEL_MONEY:
    if (f->repel > 0) {
      snprintf(out, n, "REPEL %d", f->repel);
      break;
    }
    /* fall through */
  case BEZEL_MONEY:
    if (f->money >= 0)
      snprintf(out, n, "$%ld", f->money);
    break;
  case BEZEL_NEXT_BOSS:
    if (f->boss && *f->boss && f->cap > 0)
      snprintf(out, n, "NEXT %.36s  CAP Lv%d", f->boss, f->cap % 1000);
    break;
  case BEZEL_CAUGHT_HERE:
    if (f->kindsHere > 0)
      snprintf(out, n, "CAUGHT HERE %d/%d", f->caughtHere, f->kindsHere);
    break;
  case BEZEL_LEAD:
    if (f->lead && *f->lead && f->leadMaxHp > 0) {
      snprintf(a, sizeof a, "%.12s %d/%d", f->lead, f->leadHp, f->leadMaxHp);
      if (f->leadToNext > 0)
        snprintf(b, sizeof b, "%ld EXP TO GO", f->leadToNext);
      snprintf(out, n, "%s%s%s", a, *b ? "  " : "", b);
    }
    break;
  case BEZEL_REPEL:
    if (f->repel > 0)
      snprintf(out, n, "REPEL %d", f->repel);
    break;
  case BEZEL_CAUGHT_SEEN:
    if (f->dexSeen > 0)
      snprintf(out, n, "CAUGHT %d  SEEN %d", f->dexCaught, f->dexSeen);
    break;
  case BEZEL_DEATHS:
    if (f->nuzDeaths >= 0)
      snprintf(out, n, "DEATHS %d", f->nuzDeaths);
    break;
  case BEZEL_AREA:
    if (f->nuzArea >= 0)
      snprintf(out, n, "AREA %s", f->nuzArea ? "USED" : "FREE");
    else if (f->kindsHere > 0)
      snprintf(out, n, "HERE %d/%d", f->caughtHere, f->kindsHere);
    break;
  case BEZEL_ZOOM:
    if (f->zoomRecent && f->zoomPct > 0)
      snprintf(out, n, "ZOOM %d%%", f->zoomPct);
    break;
  case BEZEL_SPEED:
    if (f->ff > 0)
      snprintf(out, n, "FF x%d", f->ff);
    else if (f->pacePct > 0 && f->pacePct != 100)
    {
      int p = f->pacePct;
      if (p % 10)
        snprintf(out, n, "SPEED x%d.%02d", p / 100, p % 100);
      else if (p % 100)
        snprintf(out, n, "SPEED x%d.%d", p / 100, p % 100 / 10);
      else
        snprintf(out, n, "SPEED x%d", p / 100);
    }
    break;
  case BEZEL_BOSS:
    if (f->boss && *f->boss)
      snprintf(out, n, "NEXT %.40s", f->boss);
    break;
  case BEZEL_CAP:
    if (f->cap > 0)
      snprintf(out, n, "CAP Lv%d", f->cap);
    break;
  case BEZEL_FOE:
    if (f->foe && *f->foe)
      snprintf(out, n, "%s", f->foe);
    break;
  case BEZEL_MATCHUP:
    if (f->matchup && *f->matchup)
      snprintf(out, n, "%s", f->matchup);
    break;
  case BEZEL_CATCH:
    if (f->catchHint && *f->catchHint)
      snprintf(out, n, "%s", f->catchHint);
    break;
  case BEZEL_LAST_SAVED:
    if (f->savedMinutes >= BEZEL_SAVE_LATE_MIN)
      snprintf(out, n, "NOT SAVED FOR %d MIN", f->savedMinutes);
    else if (f->savedMinutes >= 1)
      snprintf(out, n, "SAVED %d MIN AGO", f->savedMinutes);
    else if (f->savedMinutes == 0)
      snprintf(out, n, "SAVED JUST NOW");
    break;
  default:
    break;
  }
}

/* A bar's three places: each its own item's text, and an empty one given the first filler with something to say that
 * neither this bar nor the other one (other: its three items, or NULL; taken: what it drew, [BEZEL_ITEMS] or NULL)
 * shows yet. The battle's bar is its own. */
static inline void bezel_bar_text_taken(const int items[3], const int *other, const bool *taken,
                                        const struct bezel_facts *f, char out[3][64],
                                        bool *shown /* [BEZEL_ITEMS], what was drawn; may be NULL */) {
  bool used[BEZEL_ITEMS] = {false};
  for (int i = 0; taken && i < BEZEL_ITEMS; i++)
    used[i] = taken[i];
  for (int i = 0; other && i < 3; i++)
    if (other[i] > BEZEL_NONE && other[i] < BEZEL_ITEMS)
      used[other[i]] = true;
  for (int i = 0; i < 3; i++) {
    int item = items[i] >= 0 && items[i] < BEZEL_ITEMS ? items[i] : BEZEL_NONE;
    bezel_item_text(item, f, out[i], 64);
    used[item] = true;
    if (shown && out[i][0])
      shown[item] = true;
  }
  for (int i = 0; i < 3; i++) {
    if (out[i][0] || items[i] == BEZEL_NONE)
      continue;
    for (size_t k = 0; k < sizeof BEZEL_FILLERS / sizeof BEZEL_FILLERS[0]; k++) {
      int item = BEZEL_FILLERS[k];
      if (used[item])
        continue;
      bezel_item_text(item, f, out[i], 64);
      if (out[i][0]) {
        used[item] = true;
        if (shown)
          shown[item] = true;
        break;
      }
    }
  }
}

static inline void bezel_bar_text(const int items[3], const int *other, const struct bezel_facts *f, char out[3][64],
                                  bool *shown) {
  bezel_bar_text_taken(items, other, NULL, f, out, shown);
}
/* Both bars off the six places: the top first, then the bottom, which never says what the top drew (a filler there
 * included). TOP_SHOWN / BOTTOM_SHOWN ([BEZEL_ITEMS]) may be NULL. */
static inline void bezel_bars(const int slots[BEZEL_SLOTS], const struct bezel_facts *f, char top[3][64],
                              char bottom[3][64], bool *topShown, bool *bottomShown) {
  bool drawn[BEZEL_ITEMS] = {false};
  bezel_bar_text_taken(&slots[BEZEL_TOP_LEFT], &slots[BEZEL_BOTTOM_LEFT], NULL, f, top, drawn);
  if (topShown)
    memcpy(topShown, drawn, sizeof drawn);
  // A place of the bottom's own whose item the top already drew says nothing: its filler steps in.
  int items[3];
  for (int i = 0; i < 3; i++) {
    int item = slots[BEZEL_BOTTOM_LEFT + i];
    items[i] = item > BEZEL_NONE && item < BEZEL_ITEMS && drawn[item] ? BEZEL_ITEMS /* fillable */ : item;
  }
  char out[3][64];
  bool taken[BEZEL_ITEMS];
  memcpy(taken, drawn, sizeof drawn);
  bezel_bar_text_taken(items, &slots[BEZEL_TOP_LEFT], taken, f, out, bottomShown);
  memcpy(bottom, out, sizeof out);
}
/* LAYOUT's bar from its list: each item's text in order (OUT[i] for the i-th that has something to say; ITEM[i] the
 * item), skipping what TAKEN (the other bar's, [BEZEL_ITEMS], may be NULL) already says. Returns how many. */
static inline int bezel_list_texts(const int *list, int count, const bool *taken, const struct bezel_facts *f,
                                   char out[][64], int *item, int max) {
  int n = 0;
  bool used[BEZEL_ITEMS] = {false};
  for (int i = 0; i < count && n < max; i++) {
    int it = list[i];
    if (it <= BEZEL_NONE || it >= BEZEL_ITEMS || used[it] || (taken && taken[it]))
      continue;
    used[it] = true;
    bezel_item_text(it, f, out[n], 64);
    if (out[n][0])
      item[n++] = it;
  }
  return n;
}
/* Which of N texts (widths W, the place's index SHRINK or -1, its least width SHRINK_MIN) fit across WIDTH GAP apart:
 * KEEP[i] false for the dropped ones, the lowest BEZEL_KEEP first (later first on a tie). Returns the place's width
 * (its whole width when it needn't shrink). */
static inline int bezel_fit(const int *w, const int *item, int n, int shrink, int shrinkMin, int width, int gap,
                            bool *keep) {
  int placeW = shrink >= 0 ? w[shrink] : 0;
  for (int i = 0; i < n; i++)
    keep[i] = true;
  for (;;) {
    int total = 0, shown = 0;
    for (int i = 0; i < n; i++)
      if (keep[i]) {
        total += i == shrink ? placeW : w[i];
        shown++;
      }
    total += shown > 1 ? (shown - 1) * gap : 0;
    if (total <= width || !shown)
      return placeW;
    if (shrink >= 0 && keep[shrink] && placeW > shrinkMin) { // the place gives up what it can, down to its least
      int cut = total - width;
      placeW = placeW - cut < shrinkMin ? shrinkMin : placeW - cut;
      if (total - (w[shrink] - placeW) <= width)
        return placeW;
    }
    placeW = shrink >= 0 ? w[shrink] : 0; // not enough: something goes, and the place tries again at full width
    int drop = -1;
    for (int i = 0; i < n; i++)
      if (keep[i] && (drop < 0 || BEZEL_KEEP[item[i]] <= BEZEL_KEEP[item[drop]]))
        drop = i;
    if (drop < 0 || shown == 1)
      return placeW;
    keep[drop] = false;
  }
}
/* An old bar's three places as a list: those (NONE left out); the old defaults become the new ones. Returns the count. */
static inline int bezel_list_from_slots(const int *slots, bool top, int *out) {
  static const int OLD_TOP[3] = {BEZEL_LOCATION, BEZEL_CLOCK_BATTERY, BEZEL_NEXT_BOSS},
                   OLD_BOTTOM[3] = {BEZEL_BADGES, BEZEL_PLAY_TIME, BEZEL_REPEL_MONEY};
  if (!memcmp(slots, top ? OLD_TOP : OLD_BOTTOM, sizeof OLD_TOP)) {
    const int *d = top ? BEZEL_DEFAULT_TOP : BEZEL_DEFAULT_BOTTOM;
    int n = top ? (int)(sizeof BEZEL_DEFAULT_TOP / sizeof *d) : (int)(sizeof BEZEL_DEFAULT_BOTTOM / sizeof *d);
    memcpy(out, d, (size_t)n * sizeof *d);
    return n;
  }
  int n = 0;
  for (int i = 0; i < 3; i++)
    if (slots[i] > BEZEL_NONE && slots[i] < BEZEL_ITEMS)
      out[n++] = slots[i];
  return n;
}
/* A wild battle's catch hint from the foe's HP and status (the Gen 3 catch formula's two levers the player has). */
static inline const char *bezel_catch_hint(int hp, int maxHp, bool sleepOrFrozen, bool otherStatus) {
  if (maxHp <= 0)
    return "";
  int pct = hp * 100 / maxHp;
  if (sleepOrFrozen && pct <= 50)
    return "CATCH: GREAT CHANCE";
  if (pct <= 20 || (pct <= 50 && (sleepOrFrozen || otherStatus)))
    return "CATCH: GOOD CHANCE";
  if (pct <= 50 || sleepOrFrozen)
    return "CATCH: FAIR CHANCE";
  return "CATCH: WEAKEN IT FIRST";
}

/* The best multiplier (percent) of the moves out against the foe as a word: what the moves can do to it. */
static inline const char *bezel_matchup(int bestPct) {
  return bestPct < 0    ? ""
         : bestPct >= 200 ? "SUPER EFFECTIVE MOVE"
         : bestPct >= 100 ? "NEUTRAL MOVES"
         : bestPct > 0    ? "RESISTED MOVES"
                          : "NO MOVE HITS IT";
}
