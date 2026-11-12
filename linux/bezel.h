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
  BEZEL_CHIPS,       // FF, the zoom, SAVED after a save
  BEZEL_REPEL_MONEY, // the repel's steps while one works, else the money
  BEZEL_MONEY,
  BEZEL_NEXT_BOSS,   // the next unbeaten boss and the level cap (its team's top level)
  BEZEL_CAUGHT_HERE, // how many of this area's wild kinds are caught
  BEZEL_LEAD,        // the lead POKéMON: its HP, and the EXP to its next level where the game's curve is known
  BEZEL_LAST_SAVED,  // how long since the game was last saved
  BEZEL_ITEMS
};
enum { BEZEL_TOP_LEFT, BEZEL_TOP_CENTRE, BEZEL_TOP_RIGHT, BEZEL_BOTTOM_LEFT, BEZEL_BOTTOM_CENTRE, BEZEL_BOTTOM_RIGHT,
       BEZEL_SLOTS };

#define BEZEL_HINT_MS 5000u
#define BEZEL_SAVED_MS 3000u

static const char *const BEZEL_ITEM_NAMES[BEZEL_ITEMS] = {
    "NONE", "LOCATION", "CLOCK AND BATTERY", "CLOCK", "BATTERY", "BADGES AND PLAY TIME", "BADGES", "PLAY TIME",
    "BUTTON HINTS", "STATUS CHIPS", "REPEL OR MONEY", "MONEY", "NEXT BOSS AND CAP", "CAUGHT HERE", "LEAD POKEMON",
    "LAST SAVED"};
static const char *const BEZEL_SLOT_NAMES[BEZEL_SLOTS] = {"TOP LEFT", "TOP CENTRE", "TOP RIGHT",
                                                          "BOTTOM LEFT", "BOTTOM CENTRE", "BOTTOM RIGHT"};
static const char *const BEZEL_STYLE_NAMES[BEZEL_STYLES] = {"BLACK", "GLOW", "THEME"};
static const int BEZEL_DEFAULT_SLOTS[BEZEL_SLOTS] = {BEZEL_LOCATION, BEZEL_CLOCK_BATTERY, BEZEL_BADGES_TIME,
                                                     BEZEL_LEAD,     BEZEL_NEXT_BOSS,     BEZEL_REPEL_MONEY};
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
  int zoom;            // EXPAND's zoom step out, 0 = the closest
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
      snprintf(a, sizeof a, "BADGES %d", f->badges);
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
    if (f->zoom > 0 && at < n)
      at += (size_t)snprintf(out + at, n - at, "%sZOOM -%d", at ? "  " : "", f->zoom);
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
      snprintf(out, n, "NEXT %s  CAP Lv%d", f->boss, f->cap);
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
 * neither this bar nor the other one (other: its three items, or NULL) shows yet. The battle's bar is its own. */
static inline void bezel_bar_text(const int items[3], const int *other, const struct bezel_facts *f,
                                  char out[3][64], bool *shown /* [BEZEL_ITEMS], what was drawn; may be NULL */) {
  bool used[BEZEL_ITEMS] = {false};
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
