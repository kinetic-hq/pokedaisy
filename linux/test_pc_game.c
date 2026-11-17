/* GPL-3.0, see ../NOTICE. PC FAN GAMES: the mkxp.json PokeDaisy writes, the companion's JSON, the type numbers. */
#undef NDEBUG
#include "pc_game.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define rmdir _rmdir
#else
#include <unistd.h>
#endif

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
  // F1's key binding window stays shut, whatever the game's file says.
  assert(strstr(out, "\"enableSettings\": false"));
  n = pc_mkxp_json("{\"enableSettings\": true}", &o, out, sizeof out);
  assert(n > 0 && strstr(out, "\"enableSettings\": false") && !strstr(out, "\"enableSettings\": true"));
  // Neither vsync nor syncToRefreshrate, whatever the game's file says (they stalled Indigo at 7 frames a second).
  n = pc_mkxp_json("{\"vsync\": true, \"syncToRefreshrate\": true, \"fixedFramerate\": 60}", &o, out, sizeof out);
  assert(n > 0 && strstr(out, "\"vsync\": false") && strstr(out, "\"syncToRefreshrate\": false"));
  assert(!strstr(out, "\"vsync\": true") && !strstr(out, "\"syncToRefreshrate\": true"));
  assert(strstr(out, "\"fixedFramerate\": 60"));
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

static void test_undouble(void) {
  static uint32_t big[8 * 6], half[4 * 3];
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < 8; x++)
      big[y * 8 + x] = 0x00102030u + (uint32_t)((y / 2) * 4 + x / 2);
  assert(pc_undouble(big, 8, 8, 6, half, false));
  assert((half[0] & 0xffffff) == 0x102030 && (half[4 * 2 + 3] & 0xffffff) == 0x102030 + 11);
  // One block of twelve not doubled (a scaled logo): still the doubled kind, that block averaged.
  big[0] = 0x00000000, big[1] = 0x00ffffff, big[8] = 0x00000000, big[9] = 0x00ffffff;
  assert(pc_undouble(big, 8, 8, 6, half, false) && (half[0] & 0xffffff) == 0x808080);
  // Drawn at full size: left alone.
  for (int i = 0; i < 8 * 6; i++)
    big[i] = (uint32_t)i * 0x010203u;
  half[0] = 1;
  assert(!pc_undouble(big, 8, 8, 6, half, false) && half[0] == 1);
  assert(!pc_undouble(big, 8, 8, 6, half, true) && half[0] != 1);
  float x, y, vw, vh;
  struct pc_layout a, b;
  pc_layout_for(1920, 1200, 512, 384, 3, &a);
  pc_wide_layout(1920, 1200, 3, &b);
  assert(b.h == a.h && b.y == a.y && b.bar == a.bar && b.w == 1920 - 240 && b.x == 120);
  pc_wide_layout(1920, 1080, 3, &b);
  assert(b.w == 1920 - 240 && b.h == 960);
  pc_zoom_view(1, 1, 1, 640, 480, &x, &y, &vw, &vh);
  assert(x == 0 && y == 0 && vw == 640 && vh == 480);
  pc_zoom_view(2, 0, 0, 640, 480, &x, &y, &vw, &vh);
  assert(x == 160 && y == 120 && vw == 320 && vh == 240);
  pc_zoom_view(2, 1, -1, 640, 480, &x, &y, &vw, &vh);
  assert(x == 320 && y == 0);
}

static void test_swap_face(void) {
  char out[400];
  assert(pc_swap_face("030012345e0400008e02000000007200,Xbox 360 Controller,a:b0,b:b1,back:b6,x:b2,y:b3,"
                      "leftx:a0,platform:Windows,",
                      out, sizeof out));
  assert(!strcmp(out, "030000005e0400008e02000000007200,Xbox 360 Controller,b:b0,a:b1,back:b6,y:b2,x:b3,leftx:a0,"
                      "platform:Windows,"));
  assert(!pc_swap_face("xinput,XInput Controller,a:b0", out, sizeof out)); // no GUID
  assert(pc_swap_face("78696e70757401000000000000000000,XInput Controller,a:b0,b:b1,", out, sizeof out) &&
         !strcmp(out, "78696e70757401000000000000000000,XInput Controller,b:b0,a:b1,"));
  assert(!pc_swap_face("030012345e0400008e02000000007200,X,a:b0", out, 30));
}

static void test_pad(void) {
  struct pc_state s;
  // An older game on its own keyboard Input: its keys, the run and menu buttons read from its scripts.
  assert(pc_state_parse("{\"v\":1,\"pad\":\"keys\",\"pad_keys\":\"C:67,B:88,A:90,X:65,Y:83,Z:68,L:81,R:87,F5:116,"
                        "UP:38,DOWN:40,LEFT:37,RIGHT:39\",\"pad_roles\":\"C:CONFIRM,B:BACK/MENU,A:RUN,F5:ITEMS\"}",
                        &s));
  assert(s.pad == PC_PAD_KEYS && s.padVk[PC_PAD_C] == 67 && s.padVk[PC_PAD_F5] == 116 && s.padVk[PC_PAD_RIGHT] == 39);
  assert(!strcmp(s.padRole[PC_PAD_B], "BACK/MENU") && !strcmp(s.padRole[PC_PAD_A], "RUN") && !s.padRole[PC_PAD_X][0]);
  assert(pc_pad_target(&s, PC_IN_A) == PC_PAD_C && pc_pad_target(&s, PC_IN_X) == PC_PAD_A);
  assert(pc_pad_target(&s, PC_IN_START) == PC_PAD_B && pc_pad_target(&s, PC_IN_SELECT) == PC_PAD_F5);
  bool down[256];
  pc_pad_keys(&s, (1u << PC_IN_A) | (1u << PC_IN_LEFT), down);
  assert(down[67] && down[37] && !down[88]);
  int held = 0;
  for (int i = 0; i < 256; i++)
    held += down[i];
  assert(held == 2);
  pc_pad_keys(&s, (1u << PC_IN_SELECT) | (1u << PC_IN_START), down); // GAME MENU's chord: neither
  assert(!down[116] && !down[88]);
  struct pc_hint h[9];
  int n = pc_pad_hints(&s, true, h, 9);
  assert(n == 9);
  assert(!strcmp(h[0].button, "A") && !strcmp(h[0].label, "CONFIRM"));
  assert(!strcmp(h[1].button, "B") && !strcmp(h[1].label, "BACK/MENU"));
  assert(!strcmp(h[2].button, "X") && !strcmp(h[2].label, "RUN"));
  assert(!strcmp(h[3].button, "Y") && !strcmp(h[3].label, "A KEY"));
  assert(!strcmp(h[4].button, "START") && !strcmp(h[4].label, "MENU"));
  assert(!strcmp(h[5].button, "SELECT") && !strcmp(h[5].label, "ITEMS"));
  assert(!strcmp(h[6].button, "LB") && !strcmp(h[6].label, "Q KEY"));
  n = pc_pad_hints(&s, false, h, 9);
  assert(!strcmp(h[6].button, "L") && !strcmp(h[7].button, "R"));
  // A game reading the pad itself: nothing pressed, its own buttons named.
  assert(pc_state_parse("{\"pad\":\"native\",\"pad_roles\":\"C:CONFIRM,B:BACK,A:MENU\"}", &s));
  assert(s.pad == PC_PAD_NATIVE);
  pc_pad_keys(&s, 1u << PC_IN_A, down);
  assert(!down[67] && pc_pad_target(&s, PC_IN_START) == -1);
  n = pc_pad_hints(&s, true, h, 9);
  assert(n == 3 && !strcmp(h[2].button, "X") && !strcmp(h[2].label, "MENU"));
  // Not said yet: no hints.
  assert(pc_state_parse("{\"v\":1}", &s) && s.pad == PC_PAD_UNKNOWN && !pc_pad_hints(&s, true, h, 9));
  // On mkxp-z, which sees no controller: its own keys pressed (ENTER confirms, ESC goes back), from the start.
  pc_pad_mkxpz(&s);
  assert(s.pad == PC_PAD_KEYS && s.padOurs && s.padVk[PC_PAD_C] == 0x0D && s.padVk[PC_PAD_B] == 0x1B);
  pc_pad_keys(&s, (1u << PC_IN_A) | (1u << PC_IN_UP), down);
  assert(down[0x0D] && down[0x26] && !down[0x1B]);
  pc_pad_keys(&s, 1u << PC_IN_START, down); // the menu: Essentials' own (B) when the game names none
  assert(down[0x1B]);
  assert(pc_state_parse("{\"pad\":\"native\",\"pad_roles\":\"C:CONFIRM,B:BACK,A:MENU\"}", &s));
  pc_pad_mkxpz(&s);
  assert(s.pad == PC_PAD_KEYS && pc_pad_target(&s, PC_IN_START) == PC_PAD_A);
  pc_pad_keys(&s, 1u << PC_IN_START, down);
  assert(down[0x10] && !down[0x1B]);
  n = pc_pad_hints(&s, true, h, 9); // the game's own buttons named, never mkxp-z's keys
  assert(n == 4 && !strcmp(h[0].label, "CONFIRM") && !strcmp(h[2].label, "MENU") && !strcmp(h[3].button, "START"));
  for (int i = 0; i < n; i++)
    assert(!strstr(h[i].label, "KEY"));
  // A game on mkxp-z that asks Windows for the keyboard itself: the keys the companion read stay.
  assert(pc_state_parse("{\"pad\":\"keys\",\"pad_keys\":\"C:67,B:88\"}", &s));
  pc_pad_mkxpz(&s);
  assert(!s.padOurs && s.padVk[PC_PAD_C] == 67 && s.padVk[PC_PAD_B] == 88);
  // PAD FEED: the companion says it reads the buttons; the game's buttons held as bits, by the keys' own rules.
  assert(pc_state_parse("{\"pad\":\"native\",\"pad_roles\":\"C:CONFIRM,B:BACK/MENU\",\"pad_feed\":1}", &s));
  pc_pad_mkxpz(&s);
  assert(s.padFeed && s.pad == PC_PAD_KEYS);
  assert(pc_pad_mask(&s, (1u << PC_IN_A) | (1u << PC_IN_LEFT)) == ((1u << PC_PAD_C) | (1u << PC_PAD_LEFT)));
  assert(pc_pad_mask(&s, 1u << PC_IN_START) == 1u << PC_PAD_B);
  assert(pc_pad_mask(&s, (1u << PC_IN_SELECT) | (1u << PC_IN_START)) == 0); // GAME MENU's chord
  assert(pc_state_parse("{\"pad\":\"native\"}", &s) && !s.padFeed);
  char feed[512];
  snprintf(feed, sizeof feed, "%s/pd_pad_feed_test.txt", getenv("TEMP") ? getenv("TEMP") : "/tmp");
  assert(pc_pad_feed(feed, (1u << PC_PAD_C) | (1u << PC_PAD_UP)));
  FILE *f = fopen(feed, "rb");
  char got[16] = {0};
  assert(f && fread(got, 1, sizeof got - 1, f) > 0);
  assert(!strcmp(got, "m00513;\n"));
  // Written again in place, the reader's own handle (the companion keeps one) sees it without opening anew.
  assert(pc_pad_feed(feed, 0));
  memset(got, 0, sizeof got);
  assert(!fseek(f, 0, SEEK_SET) && fread(got, 1, sizeof got - 1, f) == 8);
  assert(!strcmp(got, "m00000;\n"));
  fclose(f);
  assert(pc_pad_feed(NULL, 0)); // closed, so it can go
  remove(feed);
}

static void test_large_address(void) {
  unsigned char exe[256] = {'M', 'Z'};
  exe[0x3C] = 0x80;
  exe[0x80] = 'P', exe[0x81] = 'E';
  exe[0x84] = 0x4C, exe[0x85] = 0x01; // i386
  exe[0x96] = 0x02;                   // EXECUTABLE_IMAGE
  const char *path = "test_large_address.exe";
  FILE *f = fopen(path, "wb");
  assert(f && fwrite(exe, 1, sizeof exe, f) == sizeof exe && !fclose(f));
  assert(pc_large_address(path) == 1 && pc_large_address(path) == 0);
  f = fopen(path, "rb");
  assert(f && fread(exe, 1, sizeof exe, f) == sizeof exe && !fclose(f));
  assert(exe[0x96] == 0x22 && exe[0x84] == 0x4C);
  exe[0x84] = 0x64, exe[0x85] = 0x86, exe[0x96] = 0x02; // x64: left alone
  f = fopen(path, "wb");
  assert(f && fwrite(exe, 1, sizeof exe, f) == sizeof exe && !fclose(f));
  assert(pc_large_address(path) == 0);
  f = fopen(path, "wb");
  assert(f && fwrite("not a player", 1, 12, f) == 12 && !fclose(f));
  assert(pc_large_address(path) == -1 && pc_large_address("no such file.exe") == -1);
  remove(path);
}

/* RESTART: the companion's note is taken once, and only while it's fresh. */
static void test_restart(void) {
  const char *dir = ".";
  remove("./restart.txt");
  assert(!pc_restart_asked(dir));
  FILE *f = fopen("./restart.txt", "wb");
  assert(f && fputs("restart\n", f) >= 0 && !fclose(f));
  assert(pc_restart_asked(dir));
  assert(!pc_restart_asked(dir)); // taken away
}

/* THE LEFT STICK as the arrow keys (Phoenix Rising): a deadzone, hysteresis at the rim and at the diagonal. */
static void test_stick(void) {
  const unsigned U = 1u << PC_IN_UP, D = 1u << PC_IN_DOWN, L = 1u << PC_IN_LEFT, R = 1u << PC_IN_RIGHT;
  assert(pc_stick_dir(0, 0, 0) == 0);
  assert(pc_stick_dir(9000, -9000, 0) == 0);           // inside the deadzone (about 12700 out)
  assert(pc_stick_dir(-32768, 0, 0) == L && pc_stick_dir(32767, 0, 0) == R);
  assert(pc_stick_dir(0, -32768, 0) == U && pc_stick_dir(0, 32767, 0) == D);
  assert(pc_stick_dir(0, -15000, 0) == 0);             // not yet on
  assert(pc_stick_dir(0, -15000, U) == U);             // held: stays on down to PC_STICK_OFF
  assert(pc_stick_dir(0, -9999, U) == 0);              // and off inside it
  assert(pc_stick_dir(6000, -8000, U) == U);           // 10000 out: still on
  // The diagonal: a held direction is kept until the other axis is a quarter past it.
  assert(pc_stick_dir(20000, -21000, 0) == U);
  assert(pc_stick_dir(21000, -20000, 0) == R);
  assert(pc_stick_dir(24000, -20000, U) == U);         // 1.2x: kept
  assert(pc_stick_dir(26000, -20000, U) == R);         // 1.3x: switches
  assert(pc_stick_dir(-24000, 20000, R) == L);         // flipped over: the other side, never both
  assert(pc_stick_dir(20000, 24000, R) == R && pc_stick_dir(20000, 26000, R) == D);
  // One key at a time, the arrows' own codes, none when centred.
  bool down[256];
  pc_stick_keys(U, down);
  assert(down[0x26] && !down[0x28] && !down[0x25] && !down[0x27]);
  pc_stick_keys(L, down);
  assert(down[0x25] && !down[0x26]);
  pc_stick_keys(0, down);
  for (int i = 0; i < 256; i++)
    assert(!down[i]);
}

int main(void) {
  test_stick();
  test_restart();
  test_large_address();
  test_pad();
  test_swap_face();
  test_undouble();
  test_layout();
  test_state();
  test_mkxp();
  test_types();
  puts("pc game: ok");
  return 0;
}
