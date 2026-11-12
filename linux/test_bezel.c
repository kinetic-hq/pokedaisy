/* The bezel bars' facts: what each item says, what it leaves out when the game doesn't give it. */
#include "bezel.h"
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
  struct bezel_facts u = {.place = NULL, .hour = -1, .battery = -1, .badges = -1, .playHours = -1, .money = -1};
  for (int i = 0; i < BEZEL_ITEMS; i++)
    EXPECT(i, &u, "");
  u.hour = 23;
  u.minute = 59;
  EXPECT(BEZEL_CLOCK_BATTERY, &u, "23:59");
  u.playHours = 0;
  u.playMinutes = 0;
  u.badges = 0;
  EXPECT(BEZEL_BADGES_TIME, &u, "BADGES 0"); // 0:00 is a card that wasn't read

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
  if (failures)
    printf("%d failures\n", failures);
  else
    puts("bezel ok");
  return failures != 0;
}
