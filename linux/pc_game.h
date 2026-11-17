/* GPL-3.0, see ../NOTICE. PC FAN GAMES: Pokemon fan games made in RPG Maker XP / VX Ace with Pokemon Essentials,
 * played from their own folders. A game folder is found in the game folders like a ROM (pc_game_dir), shown in the
 * grid with a PC tag, and played through its own player:
 *
 * - mkxp-z games (most of them): PokeDaisy makes a small folder of its own in DATA/pc/<game> with a copy of the
 *   game's player and its DLLs and a mkxp.json of its own: the game's own settings, gameFolder pointing at the
 *   game's folder (so nothing in it changes and its saves stay where they always were) and PokeDaisy's companion
 *   script (pc_companion.rb) added to its preloadScript. The companion writes the party, the place and the battle
 *   to DATA/pc/<game>/companion.json, which the wings show.
 * - games on the original RGSS player (Insurgence, Phoenix Rising): their own player runs as it is, PLAY ONLY.
 *
 * Windows only for now (pc_launch and friends do nothing elsewhere). The parsing is plain C, tested by
 * test_pc_game.c. */
#ifndef POCKET_PC_GAME_H
#define POCKET_PC_GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum pc_player {
  PC_PLAYER_NONE,  // not a game this can play
  PC_PLAYER_MKXPZ, // the game ships mkxp-z (a large player exe and a Ruby DLL): wings and all
  PC_PLAYER_RGSS,  // the original RPG Maker player: play only
};

struct pc_game_info {
  char dir[1024];   // the game's folder (the one with Game.ini)
  char title[128];  // Game.ini's Title, else the folder's name
  char exe[260];    // the player's file name, in dir (in playerDir when that's set)
  char ownExe[260]; // the game's own program in dir (what its folder starts), whichever player PLAY uses
  char version[48]; // "v19.5.1" where it's known, else ""
  int player;       // enum pc_player
  int rgss;         // 1 XP, 3 VX Ace, 0 unknown
  bool companion;   // the companion can run (mkxp-z): PARTLY, else PLAY ONLY
  // A game made for the original XP player (Insurgence) runs on the mkxp-z player of another game in the same
  // folder, one with Ruby 1.8 as the XP player had (Awakening, Bushido, Empire): its folder, else "" (the game's).
  char playerDir[1024];
};

/* DIR is an RPG Maker game's folder: Game.ini with its scripts or an archive beside it, or a known player alone. */
bool pc_game_dir(const char *dir);
/* What's known about the game in DIR (false: not one). */
bool pc_game_info(const char *dir, struct pc_game_info *out);
/* PATH (a game list entry) is a PC game's folder. */
bool pc_game_path(const char *path);

/* mkxp.json as PokeDaisy writes it: the game's own (TEXT, may be NULL; comments and trailing commas allowed) with
 * these keys set over it, the companion added to its preloadScript list. Returns the length written, or -1. */
struct pc_mkxp_overrides {
  const char *gameFolder;
  const char *preload;      // the companion script's path
  const char *extraPreload; // another (POCKET_PC_PRELOAD, for checks), or NULL
  int windowW, windowH;     // the window's size at start
  bool smooth;              // sharp bilinear: whole-number scale, then smooth to the window
  const char *standIns;     // the stand-ins for an XP game's DLLs (pc_rgss_standins.rb), first, or NULL
  int rgssVersion;          // 1-3 to set it (a borrowed player), 0 to leave the file's
};
int pc_mkxp_json(const char *text, const struct pc_mkxp_overrides *o, char *out, size_t size);

/* The companion's JSON (pc_companion.rb's dump), read into what the wings draw. */
#define PC_PARTY_MAX 6
#define PC_WILD_MAX 16
#define PC_FOES_MAX 3
/* The game's buttons (RPG Maker's Input letters, F5 and the arrows), as the companion names them in pad_keys and
 * pad_roles. */
enum pc_pad_button {
  PC_PAD_C, PC_PAD_B, PC_PAD_A, PC_PAD_X, PC_PAD_Y, PC_PAD_Z, PC_PAD_L, PC_PAD_R, PC_PAD_F5,
  PC_PAD_UP, PC_PAD_DOWN, PC_PAD_LEFT, PC_PAD_RIGHT, PC_PAD_BUTTONS
};
enum pc_pad_mode {
  PC_PAD_UNKNOWN, // not said yet (or no companion)
  PC_PAD_NATIVE,  // the game reads the controller itself (mkxp-z's own Input)
  PC_PAD_KEYS,    // the game reads only the keyboard: PokeDaisy presses its keys for the controller's buttons
};
struct pc_mon {
  char species[40], name[40], item[40], status[4], icon[200], id[24];
  char types[2][12];
  int level, hp, maxHp, form;
  bool egg, shiny;
};
struct pc_wild {
  char species[40], id[24];
  char types[2][12];
  int min, max;
};
struct pc_state {
  bool valid;
  char player[40], map[64], scene[48];
  int mapId, badges;
  long money; // -1 unknown
  long seconds;
  int partyCount;
  struct pc_mon party[PC_PARTY_MAX];
  int wildCount;
  struct pc_wild wild[PC_WILD_MAX];
  bool battle;
  int foeCount;
  struct pc_mon foes[PC_FOES_MAX];
  int foeTeam, foeAlive; // a trainer's party size and those still standing (0 / 0: wild)
  int screenW, screenH;  // the size the game draws at (0: not known)
  int seen, caught;      // the dex's counts (-1... 0: not known)
  int pad;               // enum pc_pad_mode
  unsigned char padVk[PC_PAD_BUTTONS];  // PC_PAD_KEYS: the key (a Windows key code) the game ties to each button
  char padRole[PC_PAD_BUTTONS][24];     // what the game does with each button ("CONFIRM", "BACK/MENU"), else ""
  bool padOurs;                         // PC_PAD_KEYS from pc_pad_mkxpz: mkxp-z's own keys, not the game's choice
  bool padFeed;                         // the companion reads the buttons from PokeDaisy's pad file (pc_pad_feed)
};
bool pc_state_parse(const char *json, struct pc_state *out);
/* An mkxp-z game: mkxp-z reads only the first controller Windows lists (SDL's joystick 0), which on a handheld can be
 * another device than its own pad, so the game then never sees the pad. PokeDaisy hides every controller from the
 * game's player (pc_pad_env's HIDE) and presses mkxp-z's own keys for the buttons instead (PC_PAD_KEYS), unless the
 * companion already said which keys the game reads (a game that asks Windows for the keyboard itself). */
void pc_pad_mkxpz(struct pc_state *s);
/* A 32-bit player (Bushido's mkxp-z) has 2 GB to work in unless its file says it can use more: a big window on a
 * handheld's graphics driver leaves too little, and the game stops as it loads its battle animations ("failed to
 * allocate memory"). PokeDaisy's copy of the player (pc_prepare) is marked LARGE_ADDRESS_AWARE, 4 GB on 64-bit Windows.
 * 1 when marked now, 0 when there was nothing to do (64-bit, or marked already), -1 when it isn't a player. */
int pc_large_address(const char *path);
/* The controller's buttons as PokeDaisy reads them (BUTTON LAYOUT already applied: A is the confirming one), and the
 * game's button each one is (enum pc_pad_button), or -1: what PokeDaisy presses in PC_PAD_KEYS, and what mkxp-z's own
 * reading does in PC_PAD_NATIVE (A C, B B, X A, Y X, the shoulders L and R, R3 Z). With keys, START is the game's
 * menu button and SELECT its registered item key (F5) where it has one, else Y. */
enum pc_pad_in {
  PC_IN_A, PC_IN_B, PC_IN_X, PC_IN_Y, PC_IN_L, PC_IN_R, PC_IN_R3, PC_IN_SELECT, PC_IN_START,
  PC_IN_UP, PC_IN_DOWN, PC_IN_LEFT, PC_IN_RIGHT, PC_INS
};
int pc_pad_target(const struct pc_state *s, int in);
/* PC_PAD_KEYS: the keys to hold down (DOWN[vk]) for the buttons held (HELD: 1 << enum pc_pad_in). SELECT and START
 * together (GAME MENU's chord) press neither. */
void pc_pad_keys(const struct pc_state *s, unsigned held, bool down[256]);
/* The game's buttons held (1 << enum pc_pad_button) for the controller's buttons held (HELD: 1 << enum pc_pad_in),
 * by the same rules as pc_pad_keys. */
unsigned pc_pad_mask(const struct pc_state *s, unsigned held);
/* PAD FEED (an mkxp-z game whose companion says pad_feed): the game's buttons held (pc_pad_mask) written to PATH
 * ("m<mask>;"), which the companion reads at each of the game's Input.update and answers press?, trigger?, repeat?,
 * release?, dir4 and dir8 from, besides the game's own reading. No key is pressed, so it reaches the game whichever
 * window has the foreground and whatever its player's own key bindings (F1) say. False when it couldn't be written. */
bool pc_pad_feed(const char *path, unsigned mask);
/* CONTROL HINTS: the buttons to show under the picture and what each does in this game, in the pad's order; the
 * count. XBOX names the shoulders LB and RB. */
struct pc_hint {
  char button[8];
  char label[24];
};
int pc_pad_hints(const struct pc_state *s, bool xbox, struct pc_hint *out, int max);

/* A type's id ("FIRE", "Fire") as PokeDaisy's type number: the GBA's order with Fairy as 9 (pd_ds_types); -1 for one
 * the wings don't know (a fan game's own). */
int pc_type_index(const char *id);
/* A status word ("PSN") as the GBA's status bits pd_status_label reads. */
unsigned pc_status_bits(const char *word);

/* Where the game's picture goes on an OW x OH screen for a game drawing BW x BH (its own size; 512 x 384 for most),
 * by WING ROOM (view_layout.h's VIEW_WING_ROOM_*: 0 GAME FIRST, 1 BALANCED, 2 WINGS FIRST, 3 AUTO):
 * - GAME FIRST: the screen's full height, the wings what's left.
 * - BALANCED and AUTO: the largest size in quarter steps of the game's own that leaves each wing a fifth of the
 *   screen's height wide and a bar a twentieth of it tall above and below (1920 x 1200: 2.75x, 1408 x 1056;
 *   1920 x 1080: 2.5x, 1280 x 960).
 * - WINGS FIRST: the same with wings 27% of the height wide.
 * The bars are the room above and below the picture (0 for GAME FIRST). */
struct pc_layout {
  int x, y, w, h; // the picture
  int bar;        // the bars' height (above and below)
  int wing;       // each wing's width
};
void pc_layout_for(int ow, int oh, int bw, int bh, int room, struct pc_layout *out);
/* WIDE VIEW: the picture as tall as pc_layout_for puts a 512 x 384 game (the same bars), and as wide as leaves each
 * wing a tenth of the screen's height (120 pixels at least): on the map the game draws that shape, so more of it
 * fills the room beside; its menus and battles keep their own shape inside it. */
void pc_wide_layout(int ow, int oh, int room, struct pc_layout *out);

/* A game on its own player that draws its art at half size and doubles it (Phoenix Rising: 320 x 240 drawn as
 * 640 x 480): true when nearly every 2 x 2 block of the W x H picture PX (pitch in pixels) is one colour, and then
 * OUT (W/2 x H/2) gets the picture back at its own size, each block's average (the few undoubled bits, a scaled logo,
 * keep their look); with ALWAYS, OUT gets it whatever the answer. xBRZ on the real pixels rounds the art's edges instead of the doubled blocks'. */
bool pc_undouble(const uint32_t *px, int pitch, int w, int h, uint32_t *out, bool always);

/* The zoomed part of a W x H picture: SCALE (1 = all of it, 2 = half its width) around the middle, moved by PAN
 * (-1..1 each way: as far as the zoom lets it go toward that edge). X, Y, VW, VH in the picture's pixels. */
void pc_zoom_view(float scale, float panX, float panY, int w, int h, float *x, float *y, float *vw, float *vh);

/* ---- playing (Windows) ---- */
/* DATA/pc/<stem>: the game's own folder of PokeDaisy's, made or brought up to date for the game in INFO: the
 * player's copy, its DLLs, mkxp.json and the companion. *dir gets the folder; false (and *err why) when it can't. */
struct pc_launch_opts {
  const char *root;        // DATA
  const char *stem;        // the folder's name in DATA/pc
  int windowW, windowH;    // the game's picture on screen
  const char *companionRb; // the companion's source
  size_t companionLen;
  const char *standInsRb;  // the XP DLL stand-ins' source (a borrowed player)
  size_t standInsLen;
};
/* A folder's mkxp-z player runs Ruby 1.8 (the XP player's Ruby): the one an XP game can borrow. */
bool pc_ruby18_player(const char *dir, char *exe, size_t exeSize);
bool pc_prepare(const struct pc_game_info *info, const struct pc_launch_opts *o, char *dir, size_t dirSize,
                char *err, size_t errSize);
/* Starts the game (from DIR, made by pc_prepare; RGSS games from their own folder): false when it couldn't. */
bool pc_start(const struct pc_game_info *info, const char *dir, const char *jsonOut, const char *cmdFile);
bool pc_running(void);
/* Its window, once it has one: borderless at (x, y, w, h) in screen pixels, above OWNER (a native window handle) and
 * given the keyboard. Called again whenever the game moves or resizes it. True once placed. */
bool pc_place(void *owner, int x, int y, int w, int h, bool focus);
/* A game that sizes its own window (its own XP / VX Ace player): the window kept as it is but see-through, above
 * OWNER and given the keyboard, and its picture read each frame for PokeDaisy to draw (SHADER, COLORS, zoom). */
bool pc_capture_place(void *owner, bool focus);
/* The game window's picture: XRGB words, w x h, valid until the next call. NULL when there is none. */
const uint32_t *pc_capture(int *w, int *h);
void pc_capture_free(void);
/* The game shows a message box (its player's script error). */
bool pc_error_box(void);
/* The game window's drawing area on the screen (for the screen check). */
bool pc_client_rect(int *x, int *y, int *w, int *h);
/* PokeDaisy's window (SELF, OW x OH) as it looks with the game's window over it, into OUT (XRGB), whatever else is in
 * front of them on the desktop (for the screen check). */
bool pc_compose(void *self, int ow, int oh, uint32_t *out);
/* BUTTON LAYOUT for the game's own player (an SDL one: mkxp-z, the player older games borrow): every pad read by
 * the buttons' places, as PokeDaisy reads them (SDL would otherwise swap a Nintendo-labelled pad's), and with
 * NINTENDO, MAPPINGS (SDL mapping lines with the face buttons swapped, pc_swap_face) so the right button confirms.
 * Set in PokeDaisy's environment just before the game starts, which the game's player inherits. HIDE (an mkxp-z
 * game, pc_pad_mkxpz): no controller at all for the game's player, PokeDaisy presses its keys instead; pc_pad_env(NULL,
 * false) after the start takes that away from PokeDaisy's own environment again. */
void pc_pad_env(const char *mappings, bool hide);
/* One SDL mapping line with A and B, X and Y swapped (the NINTENDO layout), a bus-and-IDs GUID's CRC part zeroed so
 * any SDL version matches it (SDL's own XInput GUID kept as it is). False when it isn't a mapping line or OUT is too
 * small. */
bool pc_swap_face(const char *mapping, char *out, size_t size);
/* PokeDaisy's own window (SELF) has the foreground: switched back to (Alt+Tab, the taskbar, a side app), so the
 * game's window gets it again. */
bool pc_self_foreground(void *self);
/* PC_PAD_KEYS: the game's keys pressed and let go so that the ones held are DOWN (NULL: none), by their scan codes
 * (so a game asking Windows for the keys' state sees them), and only while the game's window has the foreground:
 * switched away, every key is let go. */
void pc_keys_send(const bool *down);
/* Hides the game's window (PokeDaisy's menu is over it) or shows it again. */
void pc_show(bool shown);
/* Asks the game to close as its own close button would; force: ends it. */
void pc_close(bool force);
/* The game's last exit code (after it ended). */
int pc_exit_code(void);
#endif
