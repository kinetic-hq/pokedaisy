// The companion on the 3DS's 320x240 bottom screen: PARTY (with a summary on
// tap), BAG, MAP, GUIDE, POKéDEX, BATTLE (cards + your moves' verdicts) and
// SETTINGS, in the app's
// FireRed OPTION-screen look (app/.../companion/ui/GbaMenu.kt). Immediate
// mode: every pd_ui_draw records where its tappable parts went, and
// pd_ui_touch looks a tap up there - no layout pass to keep in sync.
//
// The host owns the emulator: it shows what pd_ui_draw drew, feeds the stylus
// and buttons in, and carries out what pd_ui_take_action hands back (fast-
// forward, save states, leaving the game, saving changed settings).
#ifndef PD_UI_H
#define PD_UI_H

#include <stdbool.h>

#include "pd_canvas.h"
#include "pd_game.h"
#include "pd_snapshot.h"

struct pd_layout_option; // pd_layout_pick.h
struct pd_nuzlocke;      // pd_nuzlocke.h

#define PD_UI_WIDTH 320
#define PD_UI_HEIGHT 240

enum pd_tab {
    PD_TAB_PARTY,
    PD_TAB_BAG,
    PD_TAB_MAP,
    PD_TAB_GUIDE,
    PD_TAB_DEX,
    PD_TAB_BATTLE,
    PD_TAB_SETTINGS,
    PD_TAB_CARD,
    PD_TAB_STATES,
    PD_TAB_BOXES,
    PD_TAB_NUZLOCKE,
    PD_TAB_ACHIEVEMENTS,
    PD_TAB_COUNT,
};

// How the game fills the top screen.
enum pd_screen_mode {
    PD_SCREEN_PIXEL,   // 1x, pixel for pixel
    PD_SCREEN_SHARP_125, // 1.25x, sharp bilinear
    PD_SCREEN_SHARP,   // 1.5x (fills the height), sharp bilinear
    PD_SCREEN_STRETCH, // the whole 400x240, sharp bilinear
    PD_SCREEN_MODES,
};

// How the game fills a host's screen, where the host can draw past the game's
// edges (pd_host_info.hasExpand; the Linux host: the app's ASPECT > EXPAND).
enum pd_aspect {
    PD_ASPECT_ORIGINAL, // the game's own picture, centred
    PD_ASPECT_EXPAND,   // the overworld rebuilt to the screen's edges, same scale
    PD_ASPECT_ZOOM_OUT, // EXPAND one whole scale smaller: more of the map
    PD_ASPECTS,
};

#define PD_FF_DEFAULT 200 // fast-forward's speed out of the box, in percent
// SCENE SPEEDS: SAME (GAME SPEED's) or 1x ... 4x, for each scene in this order.
enum { PD_SCENE_BATTLE, PD_SCENE_TEXT, PD_SCENE_MENU, PD_SCENE_BIKE, PD_SCENE_SURF, PD_SCENE_COUNT };
// plorpOS: the fast-forward speeds offered (the app's RATES less 1.5x), 0 = MAX: as many frames as a
// screen refresh has time for.
#define PD_FF_CHOICES 10
extern const int PD_FF_SPEEDS[PD_FF_CHOICES];
void pd_speed_label(char* out, size_t n, int pct);
int pd_ff_index(int speed);

// plorpOS: how ORIGINAL's picture fills the screen - whole pixels, the largest 3:2, or all of it.
enum pd_scaling { PD_SCALE_INTEGER, PD_SCALE_FIT, PD_SCALE_STRETCH, PD_SCALINGS };

// What the player set; the host loads and saves it (pd_settings.h).
// BUTTON LAYOUT: which face button is the game's A on a controller.
enum pd_button_layout {
    PD_LAYOUT_NINTENDO, // by position: the right button is A, the bottom one B (a Nintendo-style pad's labels)
    PD_LAYOUT_XBOX,     // by label: the button printed A is A (the bottom one on an Xbox-style pad)
    PD_BUTTON_LAYOUTS,
};
// DISPLAY ORIENTATION: the whole picture turned for a screen mounted another way (clockwise, a quarter each).
enum pd_orientation { PD_ORIENT_LANDSCAPE, PD_ORIENT_PORTRAIT, PD_ORIENT_LANDSCAPE_FLIPPED, PD_ORIENT_PORTRAIT_FLIPPED,
                      PD_ORIENTATIONS };
// GAME TOUCH: what a finger or the mouse does on the game itself (menus, the game list and the companion always
// take it): press a DS game's touch screen, or nothing.
enum pd_game_touch { PD_GAME_TOUCH_DS_SCREEN, PD_GAME_TOUCH_OFF, PD_GAME_TOUCHES };
// The desktop host's HOTKEYS buttons, stored in this order (GUIDE, SHARE, PADDLE 1-4, L3, R3, then the triggers L2,
// R2: a file from before 1.11 has the first eight) and its actions (linux/main.c's HOTKEY_ACTIONS).
#define PD_HOTKEY_BUTTONS 10
#define PD_HOTKEY_ACTIONS 14
enum { PD_HOTKEY_L3 = 6, PD_HOTKEY_R3 = 7, PD_HOTKEY_L2 = 8, PD_HOTKEY_R2 = 9 };
enum { PD_HOTKEY_DO_FF = 3, PD_HOTKEY_DO_ZOOM_IN = 11, PD_HOTKEY_DO_ZOOM_OUT = 12 };
#define PD_BAR_LIST_MAX 32
#define PD_WING_LIST_MAX 24
struct pd_settings {
    int screenMode; // enum pd_screen_mode
    int ffSpeed;    // FF SPEED in percent (a PD_FF_SPEEDS step: 150 = 1.5x), 0 = MAX
    int guideNotice; // a bit per game kind whose GUIDE notice was accepted
    int aspect;      // enum pd_aspect (hosts with hasExpand only)
    bool aspectPicked; // ASPECT chosen by the player (pd_settings_expand_default leaves a chosen ORIGINAL alone)
    bool autoZoom;   // AUTO ZOOM IN: ZOOM OUT steps back in off the plain overworld
    bool lookAround; // LOOK AROUND (on): with EXPAND, the right stick moves the view (hasLookAround hosts)
    bool dayNight;   // DAY AND NIGHT (off): the overworld outdoors follows the clock (hasDayNight hosts)
    bool wildOff;    // WILD ENCOUNTERS off (hosts that can: the Linux host's quick menu)
    int tabBar;      // TAB BAR: a bit per enum pd_tab on the bar (PARTY .. DEX, CARD); the rest open from MORE
    bool showPerf;   // FPS / CPU in the corner of the game (hosts with hasPerf)
    bool hud;        // HUD over the overworld (hosts with hasHud) ...
    int hudHidden;   // ... less these parts: a bit per enum pd_hud_part
    int hudLayout;   // enum pd_hud_layout (the app's FieldHudLayout)
    int hudLook;     // enum pd_hud_look (FieldHudLook)
    int hudSize;     // enum pd_hud_size (FieldHudSize)
    // plorpOS (its in-game menu writes these too):
    int scaling;     // enum pd_scaling, ASPECT ORIGINAL's picture
    bool ffAudio;    // FF MUSIC: sound under fast-forward (STEADY or SPED-UP), else OFF
    bool ffSteady;   // with ffAudio: STEADY, the song at its normal speed where the host has it; else SPED-UP
    bool rewind;     // REWIND: the last ~20 s kept for the rewind hotkey
    bool runIndoors; // RUN INDOORS: every map lets the player run
    bool instantText; // INSTANT TEXT: a text box's words appear at once (pd_rom_tweaks.c)
    bool reusableTms; // REUSABLE TMS: a TM stays in the bag after it teaches a move
    bool tradeEvolutions; // TRADE EVOS: a Pokémon that evolves by trading gets EVOLVE on its summary
    int shinyOdds;        // SHINY ODDS: an index into PD_SHINY_LABELS, 0 = NORMAL
    int batterySaver; // BATTERY SAVER: enum pd_battery_saver
    bool rumble;      // RUMBLE: the controller buzzes as a battle starts, harder for a shiny foe, and for rumble carts
    bool resumeGames; // RESUME GAMES: a state on leaving a game, offered back (CONTINUE) at its next start
    bool levelCap;   // LEVEL CAP: party levels past the next boss's top level show orange (a Nuzlocke's rule)
    bool lcdFilter;  // hosts with hasDisplayFilter: FILTER is SUBPIXEL (kept in step with filter, for older builds)
    int filter;      // ... FILTER: enum pd_filter (the app's SHADERS > FILTER, and the desktop's SUBPIXEL mask)
    int grid;        // ... GRID: LCD and LCD PAPER's lines, 0 SOFT / 1 MEDIUM / 2 STRONG
    bool quickGrid;  // hosts with hasQuickMenu: false = RING, true = GRID
    // Hosts with hasButtonRows: the pad buttons for the quick menu, the companion and the HUD cursor (enum pd_button).
    int quickBtn, companionBtn, hudBtn;
    // The 3DS's (the companion's SETTINGS rows on hosts that show them):
    bool hideHints;  // BATTLE HINTS off
    bool oneTapMoves; // ONE-TAP MOVES: the moves on the action menu where FIGHT is
    bool foeIvs;      // FOE IVS: battle INFO's STATS, the foe's IVs, EVs and nature beside yours
    bool battleInfo;  // BATTLE INFO (on): off, the BATTLE tab is the battle buttons only, with no ratings
    bool battleViewInfo; // BATTLE VIEW: INFO, the BATTLE tab opens on INFO rather than the buttons
    bool battleFocus;    // BATTLE FOCUS: INFO as one page about one Pokemon, the foe or yours
    int gameSpeed;   // GAME SPEED: an index into PD_GAME_SPEEDS
    int sceneSpeed[PD_SCENE_COUNT]; // SCENE SPEEDS (BATTLE, TEXT, MENU, BIKE, SURF): an index into PD_SCENE_SPEEDS, 0 = SAME
    int gameVolume;  // GAME VOLUME: 20 ... 100
    bool gameMuted;  // MUTE GAME
    bool autoRun;    // AUTO-RUN: B held for the player while walking
    bool darkMode;   // DARK MODE
    bool ffNormal;   // FF MODE: NORMAL; off = SMART, 1x while the game waits for the player
    bool cheats;     // CHEATS: the game's file's codes on
    int theme;       // THEME: a pd_themes[] id
    // Hosts with hasCompanionView (the Linux / Windows host): where the companion goes, enum pd_companion_view, and
    // SIDE PANEL's lock (the panel kept beside the game).
    int companionView;
    bool panelLocked;
    bool updateCheck; // hosts with hasUpdateCheck: look for a newer release at start
    int shader;        // hosts with hasShaders: the game picture's SHADER (OFF / SHARP / xBRZ)
    int colors;        // ... and COLORS (ORIGINAL / HANDHELD / OLED)
    bool dimCompanion; // ... and DIM COMPANION (its panels drawn darker, against OLED burn-in)
    bool reduceMotion; // ... and REDUCE MOTION (no easing HP bars or sliding cards in the wings)
    bool boxartAuto;   // ... and BOX ART DOWNLOAD (the game list looks for box art by itself)
    bool savesBeside;  // hosts with hasSavesBeside: SAVES BESIDE GAMES (on), an added folder's game saves next to it
    int fullscreen;    // hosts with hasFullscreen: FULLSCREEN, 1 / 0, -1 = not chosen yet (the host picks by screen size)
    int buttonLayout;  // hosts with hasButtonLayout: BUTTON LAYOUT, enum pd_button_layout
    // The desktop host's extra pad buttons (GUIDE, SHARE, the four back paddles, L3, R3): an action each, 0 = their
    // usual use (linux/main.c's HOTKEY_ACTIONS).
    unsigned char hotkeys[PD_HOTKEY_BUTTONS];
    int orientation;   // hosts with hasOrientation: DISPLAY ORIENTATION, enum pd_orientation
    int gameTouch;     // hosts with hasGameTouch: GAME TOUCH, enum pd_game_touch
    int background;    // the desktop host: BACKGROUND, linux/backdrop.h's enum, -1 = by COLORS (OLED = BLACK)
    int brightness;    // ... and BRIGHTNESS, a percent (80 ... 150, linux/shader.h)
    int bezelStyle;    // ... and the bezel bars (linux/bezel.h): BEZEL STYLE, -1 = by BACKGROUND
    int bezelSlots[6]; // ... and what each of their six places shows (BEZEL_* items), the lists' old form
    // ... and LAYOUT: each bar's items in order (the first three its places, the rest stepping in when one has
    // nothing to say; BEZEL_* items), count -1 = not chosen yet (the host builds them from bezelSlots) ...
    int barList[4][PD_BAR_LIST_MAX]; // the top and bottom bar walking, then in a battle
    int barCount[4];
    // ... the right wing's sections in order (enum pd_wing_section), -1 = the default ...
    int wingList[PD_WING_LIST_MAX];
    int wingCount;
    int battleWingList[PD_WING_LIST_MAX]; // the right wing in a battle (enum pd_battle_section); -1 count = default
    int battleWingCount;
    bool partyTypes;   // ... and PARTY TYPES: the party wing's type pills under each name
    int wingLook;      // ... and WING LOOK (linux/backdrop.h): 0 COMPACT, 1 COMFY, 2 BIG
    int uiSize;        // ... and UI SIZE: 0 SMALL, 1 MEDIUM, 2 LARGE (the menus, HUD, wings and companion)
    int wingRoom;      // ... and WING ROOM (linux/view_layout.h): 0 GAME FIRST, 1 BALANCED, 2 WINGS FIRST, 3 AUTO
    bool stretch;      // ... and STRETCH: the game a little past its shape (at most VIEW_STRETCH_PERCENT) to fill
};

// BATTERY SAVER (the desktop host): AUTO saves only while the machine runs on its battery.
enum pd_battery_saver { PD_SAVER_AUTO, PD_SAVER_ON, PD_SAVER_OFF, PD_SAVER_COUNT };

// COMPANION VIEW (SETTINGS > SCREEN on the Linux / Windows host): where the companion goes on a 16:9 / 16:10 screen.
enum pd_companion_view {
    PD_VIEW_BATTLE_WINGS, // the field fills the screen; a battle gets the party and the matchup beside it
    PD_VIEW_WINDOW,       // a window over the dimmed game
    PD_VIEW_SIDE_PANEL,   // slides in from the right; locked, it keeps the right half
    PD_VIEW_WINGS,        // the party left, the map or battle right, all the time
    PD_VIEW_GLASS_DRAWER, // a see-through sheet over the bottom of the screen
    PD_VIEW_STEP_BACK,    // the game two sizes smaller with the companion round it
    PD_COMPANION_VIEWS,
};
const char* pd_companion_view_name(int view);

enum pd_button { PD_BTN_X, PD_BTN_Y, PD_BTN_L2, PD_BTN_R2, PD_BTN_NONE, PD_BUTTONS };

// SHADER and COLORS' values (the Linux / Windows host's linux/shader.h uses the same numbers).
#define PD_SHADERS 3
#define PD_COLORS 3
// FILTER's values: the app's (OFF, LCD, LCD PAPER, SCANLINES, CRT), then the desktop's own SUBPIXEL (the window-wide
// mask that was its only LCD before). GRID: SOFT, MEDIUM, STRONG.
enum pd_filter { PD_FILTER_OFF, PD_FILTER_LCD, PD_FILTER_LCD_PAPER, PD_FILTER_SCANLINES, PD_FILTER_CRT,
                 PD_FILTER_SUBPIXEL, PD_FILTERS };
#define PD_GRIDS 3
extern const char* const pd_filter_names[PD_FILTERS];
extern const char* const pd_grid_names[PD_GRIDS];

// GAME SPEED's steps: 1x, the small steps up to 4x, then the slow two. PD_GAME_SPEED_PCT is each in percent.
#define PD_GAME_SPEED_COUNT 11
extern const int PD_GAME_SPEED_PCT[PD_GAME_SPEED_COUNT];
int pd_game_speed_index(int pct);
#define PD_SCENE_SPEED_COUNT 6
#define PD_SHINY_CHOICES 5
extern const char* const PD_SHINY_LABELS[PD_SHINY_CHOICES];
extern const char* const PD_SCENE_SPEEDS[PD_SCENE_SPEED_COUNT];
extern const char* const PD_GAME_SPEEDS[PD_GAME_SPEED_COUNT];

// TAB BAR: at most this many tabs beside the gear (the app's MAX_BAR_TABS).
#define PD_MAX_BAR_TABS 5
#define PD_TAB_BAR_DEFAULT (1 << PD_TAB_PARTY | 1 << PD_TAB_DEX | 1 << PD_TAB_MAP | 1 << PD_TAB_BAG | 1 << PD_TAB_GUIDE)

// The HUD's parts (the app's FieldHudPart, in its order) and SETTINGS > HUD's LAYOUT / LOOK / SIZE
// (FieldHudSettings.kt). The buttons (PC, BAG, WILD) are reached with the HUD hotkey on a host without touch.
enum pd_hud_part { PD_HUD_PARTY, PD_HUD_MAP, PD_HUD_BALLS, PD_HUD_REPEL, PD_HUD_PC, PD_HUD_BAG, PD_HUD_WILD, PD_HUD_PARTS };
enum pd_hud_layout { PD_HUD_CORNERS, PD_HUD_CORNERS_2, PD_HUD_RAILS, PD_HUD_DOCK, PD_HUD_LAYOUTS };
enum pd_hud_look { PD_HUD_GLASS, PD_HUD_WHITE_GLASS, PD_HUD_WINDOW, PD_HUD_LOOKS };
enum pd_hud_size { PD_HUD_SMALL, PD_HUD_NORMAL, PD_HUD_LARGE, PD_HUD_SIZES };
const char* pd_hud_part_name(int part);
const char* pd_hud_layout_name(int layout);
const char* pd_hud_look_name(int look);
const char* pd_hud_size_name(int size);

enum pd_action {
    PD_ACTION_NONE,
    PD_ACTION_TOGGLE_FF,
    PD_ACTION_SAVE_STATE,
    PD_ACTION_LOAD_STATE,
    PD_ACTION_LEAVE_GAME,
    PD_ACTION_SETTINGS_CHANGED,
    PD_ACTION_STATE_SLOT, // STATES picked another slot: ui.actionSlot
    PD_ACTION_RESTART_GAME,
    PD_ACTION_ITEM_PICK, // USE / GIVE: who it can go to (ui.itemId, ui.itemGive) is wanted
    PD_ACTION_ITEM_APPLY, // ... and the pick: ui.itemSlot, ui.itemMove
    PD_ACTION_ITEM_TAKE, // TAKE from ui.summarySlot's Pokémon
    PD_ACTION_UNDO_SAVE,
    PD_ACTION_UNDO_LOAD,
    PD_ACTION_BOX_WITHDRAW, // the PC: ui.boxShown, ui.boxSlot
    PD_ACTION_BOX_SWAP,     // ... with party slot ui.boxPartySlot
    PD_ACTION_BOX_DEPOSIT,  // party slot ui.boxPartySlot into box ui.boxTarget
    PD_ACTION_FLY_ASK,   // MAP's FLY: where it can go is wanted
    PD_ACTION_FLY_TO,    // ... and the pick: ui.flyPick
    PD_ACTION_PER_GAME,  // THIS GAME ONLY flipped (host->perGame says which way it was)
    PD_ACTION_TRADE_EVOLVE, // TRADE EVOLUTIONS: EVOLVE on ui.summarySlot's Pokémon
};

// A party slot in USE / GIVE's pick: dimmed, or which of its moves it would help.
struct pd_item_slot {
    bool ok;
    int heldItem;
    bool moves[4];
};

// STATES: the slots, as the host keeps them. thumb: the game at a quarter of its size (120x80 RGB, 0xRRGGBB), or NULL.
#define PD_STATE_SLOTS 10
#define PD_STATE_THUMB_W 120
#define PD_STATE_THUMB_H 80
struct pd_state_slot {
    bool used;
    char when[24];
    const uint32_t* thumb;
};

// What the host tells the companion about itself.
#define PD_UI_HOST_LINES 2
struct pd_host_info {
    const char* labels[PD_UI_HOST_LINES]; // extra SETTINGS info rows (FILE, SPEED ...)
    char values[PD_UI_HOST_LINES][40];
    int count;
    bool ffOn;
    bool hideScreenMode; // Optional: host owns display scaling. Zero keeps the 3DS row.
    bool hasExpand;      // Optional: the host can EXPAND (ASPECT / AUTO ZOOM IN rows) ...
    bool expandGame;     // ... and does for this game (else ASPECT reads N/A)
    bool hasHud;         // Optional: the host draws the HUD over the game (its SETTINGS row)
    bool hasLookAround;  // Optional: the host's EXPAND follows the right stick (LOOK AROUND's row)
    bool hasCompanionView; // Optional: COMPANION VIEW's row (settings.companionView) ...
    // ... opening LAYOUT's drawn picker when the host hands its layouts over (else a plain pick-list), one per
    // enum pd_companion_view in order (pd_layout_pick.h).
    const struct pd_layout_option* layouts;
    int layoutCount;
    bool hasUpdateCheck;   // Optional: CHECK FOR UPDATES' row (settings.updateCheck)
    bool hasResume;        // Optional: RESUME GAMES' row (settings.resumeGames)
    bool hasRumble;        // Optional: RUMBLE's row in CONTROLS (settings.rumble)
    bool hasSaver;         // Optional: BATTERY SAVER's row in APP (settings.batterySaver) ...
    bool saving;           // ... and whether it is saving right now (AUTO on battery, or ON)
    bool hasPerGame;       // Optional: THIS GAME ONLY's row in APP ...
    bool perGame;          // ... on: this game keeps its own settings
    bool hasShaders;       // Optional: SHADER, COLORS and DIM COMPANION's rows
    bool hasSavesBeside;   // Optional: SAVES BESIDE GAMES' row (settings.savesBeside)
    bool hasFullscreen;    // Optional: FULLSCREEN's row (settings.fullscreen)
    bool hasButtonLayout;  // Optional: BUTTON LAYOUT's row (settings.buttonLayout)
    bool hasOrientation;   // Optional: ORIENTATION's row (settings.orientation)
    bool hasGameTouch;     // Optional: GAME TOUCH's row (settings.gameTouch)
    bool hasDisplayFilter; // host implements FILTER (enum pd_filter) and GRID
    bool hasQuickMenu;   // host implements the selectable RING / GRID quick menu
    bool hasButtonRows;  // Optional: QUICK MENU / COMPANION / HUD CURSOR button rows (the host reads settings.*Btn)
    bool hasFfChip;      // Optional: a fast-forward chip on the tab bar (the Linux host's; the 3DS has its own hotkeys)
    bool frontendOwnsPlayback; // Optional: the frontend runs fast-forward, save states and leaving the game: no such rows
    bool hasPerf;        // Optional: the host can show FPS / CPU (its SETTINGS row)
    bool hasState;      // a save state exists for LOAD STATE
    char stateWhen[24]; // when it was made, shown beside LOAD STATE
    bool fullSettings;  // Optional: SETTINGS' groups and the rows the 3DS has
    // Optional, with fullSettings: the app's companion SETTINGS (its groups' order and sub-pages), showing only the
    // rows this host does something with: these say which (the desktop host's).
    bool androidMenus;
    bool hasGameSpeed, hasSound, hasAutoRun, hasFfMode, hasRewind, hasFfMusic;
    bool hasSteady; // FF MUSIC offers STEADY
    bool hasShinyOdds, shinyOddsFits; // SHINY ODDS' row, and whether it works in this game (else N/A)
    bool hasDayNight, dayNightFits;   // MAKEOVER's DAY AND NIGHT row, and whether it fits (no day and night of its own)
    bool wildSupported, fieldSupported; // WILD ENCOUNTERS / the field read works on this game
    // SCENE SPEEDS (androidMenus): the rows shown, and which scenes this game can be read in (else N/A).
    bool hasSceneSpeeds;
    bool sceneKnown[PD_SCENE_COUNT];
    int batteryPct;     // -1 = unknown
    bool flyReady;      // MAP's FLY button: someone in the party can
    bool canTake;       // the summary's TAKE
    bool (*itemCanUse)(int item);  // Optional: what the bag's USE / GIVE can do with an item
    bool (*itemCanGive)(int item);
    bool hasStates;     // Optional: the STATES tab's slots below (the 3DS)
    struct pd_state_slot slots[PD_STATE_SLOTS];
    bool undoSave, undoLoad; // STATES' UNDO has something to put back
    bool hasCheats;          // the game has a .cheats file
    bool hasRomTweaks;       // the host can patch the ROM: the INSTANT TEXT and REUSABLE TMS rows
    bool instantTextFits;    // the ROM's text code is known
    bool reusableTmsFits;    // the ROM's code that uses a TM up is known
    bool reusableTmsBuiltIn; // the game's own TMs never run out: ON, greyed
    bool hasTradeEvos;       // the host shows TRADE EVOS (N/A unless tradeEvoFits)
    bool tradeEvoFits;       // the game's evolution code is known
    int evolveTarget[6];     // what each party slot would become by trade now (0 = no EVOLVE)
    int slotSelected;
};

#define PD_UI_MAX_HITS 64
struct pd_hit {
    int x, y, w, h;
    int id;
    bool scroll; // part of a list that a drag scrolls
};

enum pd_overlay {
    PD_OVERLAY_NONE,
    PD_OVERLAY_PICK_SCREEN,
    PD_OVERLAY_PICK_FF,
    PD_OVERLAY_CONFIRM_SAVE,
    PD_OVERLAY_CONFIRM_LOAD,
    PD_OVERLAY_CONFIRM_LEAVE,
    PD_OVERLAY_PLACES,       // the MAP's list of places
    PD_OVERLAY_GUIDE_NOTICE, // the GUIDE's "AI-written, may be wrong" notice
    PD_OVERLAY_PICK_ASPECT,
    PD_OVERLAY_TAB_BAR, // SETTINGS > TAB BAR: each tab ON / OFF
    PD_OVERLAY_HUD,     // SETTINGS > HUD: the HUD and its parts ON / OFF
    PD_OVERLAY_MORE,    // the tabs off the bar
    PD_OVERLAY_PICK_SPEED,
    PD_OVERLAY_PICK_THEME,
    PD_OVERLAY_PICK_BOX,    // the boxes, BOXES' name or DEPOSIT's where to
    PD_OVERLAY_CONFIRM_BOX, // WITHDRAW / DEPOSIT asked
    PD_OVERLAY_PICK_VOLUME,
    PD_OVERLAY_CONFIRM_RESTART,
    PD_OVERLAY_FLY,     // MAP's FLY: the towns
    PD_OVERLAY_PICK_VIEW, // COMPANION VIEW's pick-list
    PD_OVERLAY_LAYOUT,    // COMPANION VIEW's drawn picker (host->layouts)
    PD_OVERLAY_PICK_SCENE, // a SCENE SPEEDS row's pick-list (ui->sceneRow)
    PD_OVERLAY_PICK_SHINY, // SHINY ODDS' pick-list
    PD_OVERLAY_NUZ_PICK,    // NUZLOCKE: an encounter's CAUGHT / MISSED / NOT USED (ui->nuzKey: its area)
    PD_OVERLAY_NUZ_CONFIRM, // NUZLOCKE: a Pokemon marked fainted or brought back, or NEW RUN (ui->nuzConfirm)
};

// Optional presentation data for another emulator; IDs stay native to that backend.
struct pd_party_slot_colors {
    uint32_t outline, light, fill, shade, band, stripe;
};
struct pd_party_source_palette {
    struct pd_party_slot_colors normal, selected, fainted, selectedFainted, empty;
    uint32_t text, shadow, hpFrame, hpInner, hpLabel, hpLabelShade, ball[4];
    bool hideBall, chamfer;
};
struct pd_party_source {
    void* ctx;
    bool (*egg)(void*, const struct pd_mon*);
    const char* (*species)(void*, int);
    const char* (*move)(void*, int);
    int (*moveType)(void*, int);
    bool (*icon)(void*, struct pd_canvas*, const struct pd_mon*, int, int, int, bool);
    const char* (*info)(void*, const struct pd_mon*, int);
    const struct pd_party_source_palette* palette;
};

enum pd_summary_page { PD_SUMMARY_MOVES, PD_SUMMARY_STATS, PD_SUMMARY_LEARN };
struct pd_summary_learn_row { int level; const char* name; };
struct pd_summary_learn {
    const void* ctx;
    struct pd_summary_learn_row (*row)(const void*, int index, char* scratch, size_t capacity);
    int count, first;
};
struct pd_summary_move { const char* name; int type, power, pp, lowPp; }; // lowPp: pd_move_low_pp()
struct pd_summary_view {
    const struct pd_game* game; // type names and effectiveness; NULL for standard DS types
    void* ctx;
    bool (*icon)(void*, struct pd_canvas*, int slot, int x, int y, bool small);
    const char* name;
    const char* primary;
    const char* secondary;
    bool secondaryLearn, handheldControls, learnPage;
    const char* speciesName;
    const char* heldItem;
    int friendship; // -1 for unavailable
    const struct pd_mon_stats* stats;
    bool dvs, gen1;
    const struct pd_summary_learn* learn;
    int selected, count, level, hp, maxHp, type1, type2;
    uint32_t status, experience, toNext;
    int expPercent; // -1 when only the current EXP is known
    bool egg, hasExperience;
    struct pd_summary_move moves[PD_NUM_MOVES];
};
struct pd_bag_palette;

// Optional read-only bag presentation; item IDs remain backend-owned.
struct pd_bag_source {
    void* ctx;
    int tabs, count;
    const char* (*pocketName)(void*, int);
    bool (*item)(void*, int row, int* pocket, struct pd_item*, bool* quantity);
    const char* (*name)(void*, int);
};
// A list-only dex source: no ROM entry/sprite data is implied by a name.
struct pd_dex_source {
    void* ctx;
    int count, seenCount, caughtCount;
    const char* filterLabel;
    int (*national)(void*, int);
    bool (*seen)(void*, int);
    bool (*caught)(void*, int);
    const char* (*name)(void*, int);
};
// Backend-composited map art in native pixels. Cursor coordinates use those same pixels.
// No game config, ROM offsets or ownership of the supplied buffer is implied.
struct pd_map_source {
    const uint32_t* pixels;
    int width, height;
    const char* name;
    int cursorX, cursorY, cursorW, cursorH;
};
// Host-owned option rows use the established Settings frame and row widgets.
struct pd_option_row { const char* label; const char* value; };
struct pd_option_source { const char* title; const struct pd_option_row* rows; int count, infoCount; };
struct pd_guide_row {
    const char *title, *detail, *hint, *answer;
    bool owned, heading;
};
struct pd_guide_source {
    const char* const* pages;
    int pageCount;
    const struct pd_guide_row* rows;
    int rowCount;
    int noticeBit;
    bool verified;
};
// The handheld Dex Moves pane. Providers keep cartridge ids and decoding local.
struct pd_dex_move_row {
    int number; // level, or the TM / HM number
    bool hm;
    char name[48], type[24];
};
struct pd_dex_moves_source {
    const void* ctx;
    int levels, machines;
    bool hasLevels;
    const char* machinesLabel; // NULL when the cartridge has no compatibility mapping
    void (*row)(const void*, bool machine, int index, struct pd_dex_move_row*);
};
enum pd_dex_section { PD_DEX_INFO, PD_DEX_EVOLVE, PD_DEX_AREA, PD_DEX_MOVES, PD_DEX_SECTIONS };
struct pd_dex_card {
    const char *name, *types, *measurements, *category, *abilities;
    const uint32_t* pixels;
    int pictureSize, pictureScale;
    uint32_t clear;
    int number, row, count;
    bool caught;
    unsigned pages;
    enum pd_dex_section page;
};
struct pd_dex_link_row { char name[64], requirement[80]; };
struct pd_dex_links {
    const void* ctx;
    void (*row)(const void*, int index, struct pd_dex_link_row*);
    int count;
};
struct pd_dex_pane { int x, y, w, h; };
struct pd_ui;
int pd_ui_summary_learn_last(int canvasHeight, int count); // canvas above the tab bar
void pd_ui_summary_page(struct pd_canvas*, const struct pd_summary_view*, enum pd_summary_page);
void pd_ui_summary_footer(struct pd_ui*, struct pd_canvas*, const struct pd_summary_view*, bool stats);
// Draw the four Dex filters and return the x origin of the list beside them.
int pd_ui_dex_filters(struct pd_ui*, struct pd_canvas*, const int counts[4],
                      int active, int focus, const char* region);
struct pd_dex_pane pd_ui_dex_card(struct pd_ui*, struct pd_canvas*, const struct pd_dex_card*);
void pd_ui_dex_links(struct pd_canvas*, struct pd_dex_pane, const struct pd_dex_links*, int selected);
void pd_ui_dex_info(struct pd_canvas*, struct pd_dex_pane, const char* text, const int* stats, bool gen1);
void pd_ui_dex_list_row(struct pd_canvas*, int x, int y, int w, int number, const char* name,
                        bool seen, bool caught, bool selected, const uint32_t* icon, uint32_t clear);
int pd_ui_dex_machine_row(int levels);
void pd_ui_dex_moves(struct pd_canvas*, int x, int y, int w, int h,
                     const struct pd_dex_moves_source*, int selected);

// Battle readers supply their own move IDs and species data.
struct pd_battle_source {
    void* ctx;
    const char* (*species)(void*, int);
    const char* (*move)(void*, int);
    int (*moveType)(void*, int);
    int (*movePower)(void*, int);
    void (*speciesTypes)(void*, int, int*, int*);
    bool (*icon)(void*, struct pd_canvas*, int, int, int, bool);
};

struct pd_ui {
    // Optional backend content inside the shared companion frame and tab bar.
    unsigned availableTabs; // 0 preserves the original complete tab set.
    void* contentCtx;
    void (*content)(void*, struct pd_ui*, struct pd_canvas*);
    const struct pd_option_source* optionSource;
    int optionSelected;
    const struct pd_dex_source* dexSource;
    const struct pd_bag_source* bagSource;
    const struct pd_bag_palette* bagPalette;
    bool keepBackdrop;
    bool bagDetails;
    bool handheldDex, handheldParty;
    bool shoulderTabs; // handheld tab cycle includes More, without D-pad tab focus
    const struct pd_party_source* partySource;
    int summaryPage;
    bool pixelOff; // SCREEN doesn't offer PIXEL 1x: EXPAND can't hold its speed that wide
    const struct pd_battle_source* battleSource;
    struct pd_binput* battleInput; // the host's, NULL = no touch controls
    const struct pd_snapshot* drawn; // what the last pd_ui_draw showed, for the taps on it
    const struct pd_game* drawnGame;
    bool hostHasStates;              // the host's STATES tab has slots to show
    enum pd_tab tab;
    enum pd_tab tabBeforeBattle; // the tab a battle took over from
    bool battleOnBar;            // BATTLE leads the bar: a battle is on
    bool battleWasOn;
    // BATTLE: INFO (0) or SUGGESTIONS (1); the FOE TEAM's slots seen so far
    // this battle (a bit each), the one tapped (-1 = follow the battle) and
    // the one shown (set by the draw); what the last read said was out / next.
    int battlePane;
    bool battleInfoOpen;     // INFO over the touch controls (their way back shows)
    bool battleChoosingMon;  // POKéMON tapped: our party pane, not yet the game's
    bool battleChoosingMove; // FIGHT tapped: our move grid, FIGHT not sent yet
    unsigned foeSeen;
    int foeSelected, foeShown;
    // BATTLE FOCUS: your side shown (back to the foe when a new one comes in), and which of yours (-1 = the one out).
    bool focusYours;
    int focusSlot, focusFoeKey;
    int lastFoeActive, lastFoeNext;
    int summarySlot; // party slot whose summary is open, -1 = none
    bool summaryLearn, summaryCanLearn;
    int summaryLearnRow, summaryLearnLast;
    bool summaryStats; // STATS replaces MOVES, as in MonDetailScreen
    bool summaryInfo;  // INFO (EXP, held item, type matchups) replaces them
    // BOXES: what the host read (NULL = the tab isn't here), the box asked for, the picked slot and the box summary.
    const struct pd_boxes* boxes;
    // NUZLOCKE: the host's tracker for this game (NULL = no tab), its list's scroll and the row being changed.
    struct pd_nuzlocke* nuzlocke;
    int nuzScroll, nuzKey, nuzConfirm;
    bool summaryEvolve; // the open summary's Pokémon gets EVOLVE (set from host->evolveTarget)
    bool boxMoves;   // WITHDRAW / DEPOSIT / SWAP work here
    int boxShown, boxSlot, boxSummary, boxPartySlot, boxTarget;
    bool boxSwapPick, boxDepositing;
    char boxMessage[64];
    int bagPocket;   // the open pocket tab (native ID for bagSource, 0 is always ALL)
    int bagTabCount;
    bool bagTabs;    // the D-pad cursor is on the pocket tab bar rather than the list ...
    int bagTabKey;   // ... on this tab: its native pocket ID for a bagSource, else its place (0 is always ALL)
    unsigned bagPocketMask; // bagSource: occupied native pocket IDs plus ALL
    int bagSelected; // the item's row in that tab, -1 = none
    int bagScroll;   // pixels
    // USE / GIVE on the open item: the host's answer, the pick (itemPicking), a move's pick for that slot (-1 = none).
    int itemId, bagItemShown;
    bool itemGive, itemPicking;
    int itemSlot, itemMove, itemMoveSlot;
    struct pd_item_slot itemSlots[PD_PARTY_SIZE];

    // POKéDEX: the list's scroll, the open entry (national number, 0 = the
    // list), and which dex shows (-1 = the save's own: National once it has
    // it, else the regional one).
    int dexScroll, dexOpen, dexNational;
    int bagSort; // the bag's SORT: DEFAULT, NAME, COUNT
    int dexSection, dexPageScroll, dexPageMax; // the entry's INFO / EVOLVE / AREA / MOVES
    int dexReturn; // national number to focus on return, or -1 for the first filtered row
    unsigned dexPages;
    int dexFilter, dexPage, dexDetailRow, dexLevelCount, dexMachineCount, dexLinkCount, dexSpotCount;
    // MAP: the page shown (-1 = the player's), the tapped place, the cursor's blink.
    int mapPage, mapSel, mapDungeon, mapSelTx, mapSelTy;
    bool mapBlink;
    unsigned blinkMs;
    int placesScroll;
    const char* const* flyNames; // FLY's towns, as the host found them
    int flyCount, flyPick;
    // GUIDE: the page, its scroll, the open entry and how far it's revealed.
    int guidePage, guideScroll, guideOpen, guideLevel;
    bool guideController, guideFollow, guideNoticeConfirm, guideVerified;
    int guidePageCount, guidePageFocus, guideRowFocus, guideNoticeBit;
    // TRAINER CARD: the side shown, a flip running (ms into it, 0 = none) and
    // the play time's blinking colon.
    bool cardBack, cardColon;
    int cardFlipMs;
    unsigned cardColonMs;

    enum pd_overlay overlay;
    int sceneRow; // PD_OVERLAY_PICK_SCENE: which scene (PD_SCENE_*)
    bool steadyOffered; // FF MUSIC's row last drawn offered STEADY
    bool moreOpened; // the open tab came from MORE (BACK doesn't send the CARD to SETTINGS then)
    int settingsNavId, settingsNavCount, settingsNavHeight;
    struct { int id, top; } settingsNav[PD_UI_MAX_HITS];
    int settingsScroll; // SETTINGS' list, in canvas pixels
    int settingsPage;   // fullSettings: the sub-page open in SETTINGS (0 = its home)
    int layoutScroll;   // LAYOUT's picker: its first row shown
    const struct pd_layout_option* layoutHost; // the host's layouts, kept from the last SETTINGS draw
    int layoutHostCount;
    struct pd_settings* settings;
    enum pd_action action;
    int actionSlot;

    bool touching, dragging, touchScroll;
    int touchX0, touchY0, touchLastY;
    int pressedId; // what the stylus went down on (-1 = nothing)
    bool barDragging;                 // the stylus is on a list's bar
    int barY, barH, barThumb, barMax; // that bar, as last drawn
    bool keyFocus; // the game's buttons are on the companion: pd_ui_focus_*, L / R keys at the tab bar's ends
    int focusId;   // the hit a host's D-pad focus is on, drawn as that part's own cursor (-1 = none)
    int startId;   // set by each draw: where a D-pad starts on this screen (-1 = its first item)
    struct pd_hit hits[PD_UI_MAX_HITS];
    int hitCount;
};

// Shared modal choice window for backend-owned preferences (selection is host-owned).
void pd_ui_choices_view(struct pd_canvas*, const char*, const char* const*, int, int);

// Shared party body, including its original frames, HP bars, status and summary.
struct pd_map_viewport { int x, y, w, h; };
struct pd_map_viewport pd_ui_map_view(struct pd_canvas*, const struct pd_map_source*);
enum pd_map_input {
    PD_MAP_IDLE, PD_MAP_MOVE, PD_MAP_CENTER, PD_MAP_OPEN_PLACES, PD_MAP_CHOOSE, PD_MAP_CLOSE,
};
// action is -1 on the grid, or the index of the focused button.
enum pd_map_input pd_ui_map_control(int* action, int count, int dx, int dy, bool accept, bool back, bool places);
int pd_ui_list_step(int selected, int count, int step);
bool pd_ui_settings_control(struct pd_ui*, int dy, bool accept);
void pd_ui_format_money(long, char*, size_t);
void pd_ui_options_view(struct pd_ui*,struct pd_canvas*,const struct pd_game*,const struct pd_snapshot*);
void pd_ui_guide_view(struct pd_ui*, struct pd_canvas*, const struct pd_guide_source*);
void pd_ui_guide_reset(struct pd_ui*);
void pd_ui_guide_control(struct pd_ui*, int dx, int dy, bool confirm);
void pd_ui_guide_boss_note(char*, size_t, const char* place, int count, int ace, int best);
// The level cap: the next boss not beaten yet's top level (its first team), 0 with no boss data or none left.
int pd_ui_level_cap(const struct pd_game* g, const struct pd_snapshot* s);
// The next unbeaten boss's name (into title) and its team's top level, 0 when none is known.
int pd_ui_next_boss(const struct pd_game* g, const struct pd_snapshot* s, char* title, size_t len);
// The same boss's team (up to PD_TEAM_MAX into team, struct pd_team_mon of pd_guide.h); returns how many, 0 = none.
struct pd_team_mon;
int pd_ui_next_boss_team(const struct pd_game* g, const struct pd_snapshot* s, char* title, size_t len,
                         struct pd_team_mon* team);
// The items still to pick up in the player's area (TMS: only the TMs and HMs, gifts included): how many, and the
// first MAX names (a hidden one as HIDDEN ITEM) into names. -1 when the game has no area data.
int pd_ui_area_items(const struct pd_game* g, const struct pd_snapshot* s, bool tms, char (*names)[32], int max);
// The GUIDE's first thing still to do in the player's area, its hint if it has one: false when there's none.
bool pd_ui_area_tip(const struct pd_game* g, const struct pd_snapshot* s, char* out, size_t len);
// The wild POKéMON of the player's area: how many kinds, and how many of them are caught. False with none known.
bool pd_ui_caught_here(const struct pd_game* g, const struct pd_snapshot* s, int* caught, int* total);
// This area's wild kinds (every method, each once, in the guide's order) and whether each is caught; up to max.
int pd_ui_wild_here(const struct pd_game* g, const struct pd_snapshot* s, int* species, bool* caught, int max);
// The same with each kind's level range over every way it's met here (MIN / MAX may be NULL).
int pd_ui_wild_levels_here(const struct pd_game* g, const struct pd_snapshot* s, int* species, bool* caught, int* minLv,
                           int* maxLv, int max);
void pd_ui_dex_view(struct pd_ui*,struct pd_canvas*,const struct pd_game*,const struct pd_snapshot*,int selected);
void pd_ui_bag_cycle_sort(struct pd_ui*);
// The tab bar's D-pad step: the key after / before `key` in `keys` (the tabs in the order they are drawn), the ends
// stopping the cursor; a key not among them starts from the first.
int pd_ui_bag_tab_step(const int* keys, int count, int key, int dy);
void pd_ui_bag_view(struct pd_ui*, struct pd_canvas*, const struct pd_game*, const struct pd_snapshot*);
void pd_ui_party_view(struct pd_ui*, struct pd_canvas*, const struct pd_game*,
                      const struct pd_snapshot*, int selected);
void pd_ui_init(struct pd_ui* ui, struct pd_settings* settings);
// Follows the game: a battle opens BATTLE, its end goes back.
void pd_ui_update(struct pd_ui* ui, const struct pd_snapshot* s);
void pd_ui_draw(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g,
                const struct pd_snapshot* s, const struct pd_host_info* host);
// Stylus: down / held / up at (x, y). Returns true if the screen needs a redraw.
bool pd_ui_touch(struct pd_ui* ui, int x, int y, bool down);
// Buttons the GBA doesn't have (the 3DS's X / Y): next / previous tab, BACK.
// Controller access to the same TAB BAR / MORE overlays used by touch hosts.
int pd_ui_tab_menu_count(const struct pd_ui*);
void pd_ui_tab_menu_focus(struct pd_ui*, int row);
void pd_ui_tab_menu_choose(struct pd_ui*, int row);
void pd_ui_next_tab(struct pd_ui* ui, int dir);
void pd_ui_party_summary_control(struct pd_ui*, const struct pd_snapshot*, int dx, int dy, bool confirm, bool select);
// The buttons on the companion (keyFocus): a direction (0 up, 1 down, 2 left, 3 right), A on what the cursor's on.
void pd_ui_focus_move(struct pd_ui* ui, int direction);
void pd_ui_focus_press(struct pd_ui* ui);
bool pd_ui_back(struct pd_ui* ui);
// The last thing the player asked the host for (once), or PD_ACTION_NONE.
enum pd_action pd_ui_take_action(struct pd_ui* ui);
// Time passing (milliseconds since the last call): true when something on
// screen animates and needs a redraw (the MAP cursor's blink, the TRAINER
// CARD's colon and flip).
bool pd_ui_tick(struct pd_ui* ui, unsigned ms);

// COMPANION VIEW's narrow panels (the Linux / Windows host's wings), each filling the canvas it is given: the party
// down a column, the battle's matchup (the foe, its types, your moves with their verdicts), the map round the player,
// and a strip of up to three texts (left, centre, right) for above and below the game.
void pd_ui_party_panel(struct pd_canvas*, const struct pd_game*, const struct pd_snapshot*);
// The wings' motion: the host sets the clock (ms) and REDUCE MOTION before a draw; a panel that is still easing (an HP
// bar moving, a card sliding in) sets pd_ui_panel_moving, and the host draws it again next frame.
extern uint32_t pd_ui_now_ms;
extern bool pd_ui_reduce_motion;
// What the host's bezel bars show right now (PD_BAR_*): the right wing leaves those facts out, so nothing is said twice.
enum { PD_BAR_MONEY = 1, PD_BAR_BADGES = 2, PD_BAR_TIME = 4, PD_BAR_BOSS = 8, PD_BAR_HERE = 16, PD_BAR_DEX = 32 };
extern uint32_t pd_ui_bar_facts;
// The map wing's two buttons (the desktop host's, under the wing's facts): drawn when pd_ui_wing_buttons_on, and where
// each landed in the wing's canvas after a draw (w 0 = not drawn: no room).
enum { PD_WING_SAVE_STATE, PD_WING_CLOSE_GAME, PD_WING_BUTTONS };
// The right wing's sections (LAYOUT > RIGHT WING), in the order they're drawn: the map at the top and the buttons at
// the foot wherever they sit in the list; what doesn't fit is dropped from the bottom, and what the game doesn't give
// is left out with its room going to the next. The values are stored: append only.
enum pd_wing_section {
    PD_WS_MAP, PD_WS_BADGES, PD_WS_PLAY_TIME, PD_WS_NEXT_BOSS, PD_WS_CAUGHT_HERE, PD_WS_CAUGHT_SEEN, PD_WS_MONEY,
    PD_WS_HERE, PD_WS_QUICK_BAG, PD_WS_BUTTONS, PD_WS_BOSS_TEAM, PD_WS_ITEMS_HERE, PD_WS_NUZLOCKE, PD_WS_TIP,
    PD_WS_PARTY_TYPES, PD_WS_EVOLVE, PD_WS_TMS_HERE, PD_WS_TIP_OR_WEAK, PD_WING_SECTIONS
};
extern const char* const pd_wing_section_names[PD_WING_SECTIONS];
extern const int pd_wing_default[];
extern const int pd_wing_default_count;
// The sections the right wing draws (NULL = pd_wing_default), and the NUZLOCKE run its section reads (NULL = off).
extern const int* pd_ui_wing_list;
extern int pd_ui_wing_count;
extern const struct pd_nuzlocke* pd_ui_wing_nuzlocke;
// PARTY TYPES: the party wing's type pills under each name (on unless the host turns it off).
extern bool pd_ui_party_types;
// LAYOUT's BATTLE RIGHT WING: pd_ui_battle_panel's sections, in order (NULL = pd_battle_default). When they don't all
// fit, the moves lose their power and accuracy line, then SWITCH TO goes, then FOE TEAM, then the rest from the
// bottom; the cards, the moves and FOE IVS stay.
enum pd_battle_section { PD_BS_CARDS, PD_BS_MOVES, PD_BS_SPEED, PD_BS_FOE_STATS, PD_BS_SWITCH, PD_BS_FOE_TEAM,
                         PD_BATTLE_SECTIONS };
extern const char* const pd_battle_section_names[PD_BATTLE_SECTIONS];
extern const int pd_battle_default[];
extern const int pd_battle_default_count;
extern const int* pd_ui_battle_list;
extern int pd_ui_battle_count;
extern bool pd_ui_foe_ivs;     // FOE IVS on: the battle wing may show the foe's
extern int pd_ui_switch_slot;  // the party card SWITCH TO points at (the left wing marks it), -1 none
// SWITCH TO's pick in a battle: the party slot, -1 when the best is already out.
int pd_ui_switch_pick(const struct pd_game* g, const struct pd_snapshot* s);
struct pd_ui_rect { int x, y, w, h; };
extern bool pd_ui_wing_buttons_on;
extern struct pd_ui_rect pd_ui_wing_buttons[PD_WING_BUTTONS];
// REPEL's steps left as the host reads them (QUICK BAG's line): 0 = none working, -1 = unknown.
extern int pd_ui_repel_steps;
extern bool pd_ui_panel_moving;
// The same, each slot's icon drawn by ICON (false: a Poke Ball stands in) for a host whose species aren't the GBA's
// (the DS games); the game may be NULL, then the names are the snapshot's nicknames.
typedef bool (*pd_ui_icon_fn)(struct pd_canvas*, void* ctx, int slot, int x, int y, bool fainted);
void pd_ui_party_panel_icons(struct pd_canvas*, const struct pd_game*, const struct pd_snapshot*, pd_ui_icon_fn icon,
                             void* ctx);
// A narrow panel of facts under a title, each row "LABEL\tVALUE" (the value under its label).
void pd_ui_facts_panel(struct pd_canvas*, const char* title, const char* const* rows, int count);
void pd_ui_battle_panel(struct pd_canvas*, const struct pd_game*, const struct pd_snapshot*, const struct pd_battle_source*);
void pd_ui_map_panel(struct pd_canvas*, const struct pd_game*, const struct pd_snapshot*, bool blink);
void pd_ui_strip(struct pd_canvas*, const char* left, const char* middle, const char* right);

const char* pd_screen_mode_name(int mode);
const char* pd_aspect_name(int aspect);

#endif
