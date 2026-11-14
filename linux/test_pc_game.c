/* GPL-3.0, see ../NOTICE. PC FAN GAMES: the mkxp.json PokeDaisy writes, the companion's JSON, the type numbers. */
#undef NDEBUG
#include "pc_game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_state(void) {
  static const char json[] =
      "{\"v\":1,\"scene\":\"Scene_Map\",\"player\":\"Daisy\",\"party\":[{\"species\":\"Sparkit\",\"id\":\"SPARKIT\","
      "\"form\":0,\"name\":\"Zap\",\"level\":12,\"hp\":33,\"maxhp\":33,\"status\":\"\",\"egg\":false,\"shiny\":false,"
      "\"types\":[\"ELECTRIC\"],\"item\":\"\",\"icon\":\"\"},{\"species\":\"Leafa\",\"id\":\"LEAFA\",\"form\":0,"
      "\"name\":\"Leafa\",\"level\":10,\"hp\":0,\"maxhp\":31,\"status\":\"PSN\",\"egg\":false,\"shiny\":true,"
      "\"types\":[\"GRASS\",\"POISON\"],\"item\":\"Berry\",\"icon\":\"Graphics/Icons/icon001.png\"}],"
      "\"badges\":2,\"money\":12345,\"time\":81,\"map_id\":51,\"map\":\"Intro \\\"Town\\\" \\u00e9\",\"wild\":[],"
      "\"battle\":false,\"extra\":{\"nested\":[1,2,{\"a\":null}]}}";
  struct pc_state s;
  assert(pc_state_parse(json, &s));
  assert(!strcmp(s.scene, "Scene_Map"));
  assert(!strcmp(s.player, "Daisy"));
  assert(s.partyCount == 2);
  assert(!strcmp(s.party[0].name, "Zap") && s.party[0].level == 12 && s.party[0].maxHp == 33);
  assert(!strcmp(s.party[0].types[0], "ELECTRIC") && !s.party[0].types[1][0]);
  assert(!strcmp(s.party[1].types[1], "POISON") && s.party[1].shiny && !strcmp(s.party[1].status, "PSN"));
  assert(!strcmp(s.party[1].item, "Berry") && s.party[1].hp == 0);
  assert(s.badges == 2 && s.money == 12345 && s.seconds == 81 && s.mapId == 51);
  assert(!strncmp(s.map, "Intro \"Town\"", 12));
  assert(s.wildCount == 0 && !s.battle);

  static const char battle[] = "{\"scene\":\"Scene_Map\",\"battle\":true,\"foes\":[{\"species\":\"Ratto\",\"level\":5,"
                               "\"hp\":10,\"maxhp\":20,\"types\":[\"NORMAL\"]}],\"foe_team\":{\"count\":3,\"alive\":2},"
                               "\"wild\":[{\"species\":\"Ratto\",\"id\":\"RATTO\",\"min\":2,\"max\":5,"
                               "\"types\":[\"NORMAL\"]}]}";
  assert(pc_state_parse(battle, &s));
  assert(s.battle && s.foeCount == 1 && s.foes[0].level == 5 && s.foes[0].hp == 10);
  assert(s.foeTeam == 3 && s.foeAlive == 2);
  assert(s.wildCount == 1 && s.wild[0].min == 2 && s.wild[0].max == 5);
  assert(s.money == -1);

  assert(!pc_state_parse("{\"party\":[", &s));
  assert(!pc_state_parse("", &s));
  assert(!pc_state_parse(NULL, &s));
}

static void test_mkxp(void) {
  static const char game[] = "{\n  // the game's own\n  \"pathCache\": true,\n  \"rubyLoadpath\": [\"x\"],\n"
                             "  \"preloadScript\": [\"own.rb\"],\n  \"defScreenW\": 640,\n  \"smoothScaling\": 0,\n}\n";
  struct pc_mkxp_overrides o = {"D:/games/My Game", "D:/data/pc/companion.rb", NULL, 1600, 1200, true, NULL, 0};
  char out[4096];
  int n = pc_mkxp_json(game, &o, out, sizeof out);
  assert(n > 0 && (size_t)n == strlen(out));
  assert(strstr(out, "\"pathCache\": true"));
  assert(strstr(out, "\"rubyLoadpath\": [\"x\"]"));
  assert(strstr(out, "\"gameFolder\": \"D:/games/My Game\""));
  assert(strstr(out, "\"defScreenW\": 1600") && !strstr(out, "640"));
  assert(strstr(out, "\"preloadScript\": [\"own.rb\", \"D:/data/pc/companion.rb\"]"));
  assert(!strstr(out, "// the game's own"));
  // No file of the game's own, and a second preload.
  o.extraPreload = "C:/t.rb";
  n = pc_mkxp_json(NULL, &o, out, sizeof out);
  assert(n > 0 && strstr(out, "[\"D:/data/pc/companion.rb\", \"C:/t.rb\"]"));
  // Backslashes in a path come out as forward slashes (mkxp-z's JSON reads them as escapes).
  o.gameFolder = "D:\\games\\x";
  o.extraPreload = NULL;
  n = pc_mkxp_json("{}", &o, out, sizeof out);
  assert(n > 0 && strstr(out, "\"gameFolder\": \"D:/games/x\""));
  assert(pc_mkxp_json(game, &o, out, 20) < 0);
  // The game's own RGSS version stays, unless PokeDaisy picks one.
  n = pc_mkxp_json("{\"rgssVersion\": 1}", &o, out, sizeof out);
  assert(n > 0 && strstr(out, "\"rgssVersion\": 1"));
  o.rgssVersion = 3;
  n = pc_mkxp_json("{\"rgssVersion\": 1}", &o, out, sizeof out);
  assert(n > 0 && strstr(out, "\"rgssVersion\": 3") && !strstr(out, "\"rgssVersion\": 1"));
}

static void test_types(void) {
  assert(pc_type_index("NORMAL") == 0 && pc_type_index("fairy") == 9 && pc_type_index("DARK") == 17);
  assert(pc_type_index("QMARKS") < 0 && pc_type_index("") < 0 && pc_type_index(NULL) < 0);
  assert(pc_status_bits("SLP") == 1 && pc_status_bits("PAR") == 1u << 6 && pc_status_bits("") == 0);
}

static void test_layout(void) {
  struct pc_layout l;
  pc_layout_for(1920, 1200, 512, 384, 3, &l); // AUTO on the OneXPlayer 3
  assert(l.w == 1408 && l.h == 1056 && l.x == 256 && l.y == 72 && l.bar == 72);
  pc_layout_for(1920, 1080, 512, 384, 3, &l); // AUTO on the ROG Xbox Ally
  assert(l.w == 1280 && l.h == 960 && l.wing == 320 && l.bar == 60);
  pc_layout_for(1920, 1200, 512, 384, 0, &l); // GAME FIRST
  assert(l.w == 1600 && l.h == 1200 && l.bar == 0 && l.wing == 160);
  pc_layout_for(1920, 1200, 512, 384, 2, &l); // WINGS FIRST
  assert(l.wing >= 324 && l.w == 1152);
  pc_layout_for(1920, 1080, 640, 360, 1, &l); // a 16:9 game
  assert(l.wing >= 216 && l.bar >= 54 && l.w * 9 == l.h * 16);
  pc_layout_for(400, 300, 512, 384, 3, &l); // too small for the rule: the full height
  assert(l.h == 300 && l.w == 400);
  pc_layout_for(1920, 1080, 0, 0, 3, &l); // size not known yet: 512 x 384
  assert(l.w == 1280);
}

int main(void) {
  test_layout();
  test_state();
  test_mkxp();
  test_types();
  puts("pc game: ok");
  return 0;
}
