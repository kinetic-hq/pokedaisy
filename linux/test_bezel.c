/* The bezel bars' facts: what each item says, what it leaves out when the game doesn't give it. */
#include "bezel.h"
#include "wing_toast.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define EXPECT(item, facts, want)                                                                                      \
  do {                                                                                                                 \
    char got[64];                                                                                                      \
    bezel_item_text(item, facts, got, sizeof got);                                                                     \
    if (strcmp(got, want)) {                                                                                           \
      printf("item %d: got '%s', want '%s'\n", item, got, want);                                                       \
      failures++;                                                                                                      \
    }                                                                                                                  \
  } while (0)

int main(void) {
  struct bezel_facts f = {.place = "VIRIDIAN CITY", .hour = 9, .minute = 5, .battery = 82, .charging = true,
                          .badges = 1, .playHours = 3, .playMinutes = 7, .hints = true, .companion = true,
                          .quickMenu = true, .ff = 4, .zoom = 1, .saved = true, .money = 3300, .repel = 0};
  EXPECT(BEZEL_NONE, &f, "");
  EXPECT(BEZEL_LOCATION, &f, "VIRIDIAN CITY");
  EXPECT(BEZEL_CLOCK_BATTERY, &f, "09:05  +82%");
  EXPECT(BEZEL_CLOCK, &f, "09:05");
  EXPECT(BEZEL_BATTERY, &f, "+82%");
  EXPECT(BEZEL_BADGES_TIME, &f, "BADGES 1  3:07");
  EXPECT(BEZEL_HINTS, &f, "Y QUICK MENU  X COMPANION  R3 VIEW");
  EXPECT(BEZEL_CHIPS, &f, "FF x4  ZOOM -1  SAVED");
  EXPECT(BEZEL_REPEL_MONEY, &f, "$3300");
  f.repel = 87;
  EXPECT(BEZEL_REPEL_MONEY, &f, "REPEL 87");
  EXPECT(BEZEL_MONEY, &f, "$3300");

  // Unknown: left out, never made up.
  struct bezel_facts u = {.place = NULL, .hour = -1, .battery = -1, .badges = -1, .playHours = -1, .money = -1,
                          .leadToNext = -1, .savedMinutes = -1};
  for (int i = 0; i < BEZEL_ITEMS; i++)
    EXPECT(i, &u, "");
  u.hour = 23;
  u.minute = 59;
  EXPECT(BEZEL_CLOCK_BATTERY, &u, "23:59");
  u.playHours = 0;
  u.playMinutes = 0;
  u.badges = 0;
  EXPECT(BEZEL_BADGES_TIME, &u, "BADGES 0"); // 0:00 is a card that wasn't read

  // The new facts, and an empty place given a filler the bar doesn't show yet.
  struct bezel_facts k = {.place = "ROUTE 1", .hour = -1, .battery = -1, .badges = -1, .playHours = -1, .money = 500,
                          .boss = "BROCK", .cap = 14, .caughtHere = 2, .kindsHere = 5, .lead = "PIKACHU", .leadHp = 12,
                          .leadMaxHp = 20, .leadToNext = 85, .savedMinutes = 7};
  EXPECT(BEZEL_NEXT_BOSS, &k, "NEXT BROCK  CAP Lv14");
  EXPECT(BEZEL_CAUGHT_HERE, &k, "CAUGHT HERE 2/5");
  EXPECT(BEZEL_LEAD, &k, "PIKACHU 12/20  85 EXP TO GO");
  EXPECT(BEZEL_LAST_SAVED, &k, "SAVED 7 MIN AGO");
  k.savedMinutes = 45;
  EXPECT(BEZEL_LAST_SAVED, &k, "NOT SAVED FOR 45 MIN");
  {
    int items[3] = {BEZEL_HINTS, BEZEL_NEXT_BOSS, BEZEL_NONE};
    char bar[3][64];
    bezel_bar_text(items, NULL, &k, bar, NULL);
    // The faded hints take the first filler not shown yet (NEXT BOSS is): CAUGHT HERE. NONE stays empty.
    if (strcmp(bar[0], "CAUGHT HERE 2/5") || strcmp(bar[1], "NEXT BROCK  CAP Lv14") || bar[2][0]) {
      printf("bar fill: '%s' '%s' '%s'\n", bar[0], bar[1], bar[2]);
      failures++;
    }
    // What the other bar shows is never a filler here: CAUGHT HERE is the other bar's, so LEAD comes next.
    int other[3] = {BEZEL_CAUGHT_HERE, BEZEL_NONE, BEZEL_NONE};
    bool shown[BEZEL_ITEMS] = {false};
    bezel_bar_text(items, other, &k, bar, shown);
    if (strcmp(bar[0], "PIKACHU 12/20  85 EXP TO GO")) {
      printf("bar fill other: '%s'\n", bar[0]);
      failures++;
    }
    // What the bar drew is reported, so the wings don't say it again; an empty NONE isn't.
    if (!shown[BEZEL_LEAD] || !shown[BEZEL_NEXT_BOSS] || shown[BEZEL_NONE] || shown[BEZEL_CAUGHT_HERE]) {
      puts("bar shown");
      failures++;
    }
  }
  if (bezel_style_resolve(-1, true) != BEZEL_STYLE_BLACK || bezel_style_resolve(-1, false) != BEZEL_STYLE_THEME ||
      bezel_style_resolve(BEZEL_STYLE_GLOW, true) != BEZEL_STYLE_GLOW) {
    puts("style resolve");
    failures++;
  }
  if (strcmp(bezel_catch_hint(30, 30, false, false), "CATCH: WEAKEN IT FIRST") ||
      strcmp(bezel_catch_hint(5, 30, false, false), "CATCH: GOOD CHANCE") ||
      strcmp(bezel_catch_hint(10, 30, true, false), "CATCH: GREAT CHANCE") || strcmp(bezel_catch_hint(1, 0, 0, 0), "")) {
    puts("catch hint");
    failures++;
  }
  if (strcmp(bezel_matchup(200), "SUPER EFFECTIVE MOVE") || strcmp(bezel_matchup(-1), "") ||
      strcmp(bezel_matchup(0), "NO MOVE HITS IT")) {
    puts("matchup");
    failures++;
  }
  {
    // The wings' toasts: nothing on the first frame, then a new area, a level up, the spray, a save.
    struct wing_toasts t = {0};
    struct wing_toast_in in = {.valid = true, .mapsec = 3, .place = "ROUTE 1", .repel = 40, .partyCount = 1};
    in.party[0] = (struct wing_toast_mon){.key = 77, .level = 5};
    snprintf(in.party[0].name, sizeof in.party[0].name, "EMBERPUP");
    wing_toast_watch(&t, &in, 100);
    if (wing_toast_alpha(&t, 120)) {
      puts("toast on the first frame");
      failures++;
    }
    in.mapsec = 4;
    in.place = "ROUTE 2";
    wing_toast_watch(&t, &in, 200);
    if (strcmp(t.head, "NEW AREA") || strcmp(t.text, "ROUTE 2") || wing_toast_alpha(&t, 200 + 1000) != 255 ||
        wing_toast_alpha(&t, 200 + WING_TOAST_MS) != 0 || wing_toast_alpha(&t, 200 + 40) <= 0) {
      printf("area toast '%s'\n", t.text);
      failures++;
    }
    in.battle = true;
    in.mapsec = 5;
    in.party[0].level = 6;
    wing_toast_watch(&t, &in, 300);
    if (strcmp(t.head, "LEVEL UP") || strcmp(t.text, "EMBERPUP Lv6")) {
      printf("level toast '%s'\n", t.text);
      failures++;
    }
    in.battle = false;
    in.repel = 0;
    wing_toast_watch(&t, &in, 400);
    if (strcmp(t.head, "REPEL")) {
      printf("repel toast '%s'\n", t.text);
      failures++;
    }
    in.saved = true;
    wing_toast_watch(&t, &in, 500);
    if (strcmp(t.head, "SAVED") || t.at != 500) {
      printf("save toast '%s'\n", t.text);
      failures++;
    }
  }
  if (failures)
    printf("%d failures\n", failures);
  else
    puts("bezel ok");
  return failures != 0;
}
