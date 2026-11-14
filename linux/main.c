/* Native single-screen host for PokeDaisy. GPL-3.0, see ../NOTICE. */
#include "../3ds/desktop/pd_png.h"
#include "boxart.h"
#include "stretch.h"
#include "game_grid.h"
#include "game_support.h"
#include "game_name.h"
#include "pd_menu.h"
#include "pd_layout_pick.h"
#include "view_layout.h"
#include "pd_romart.h"
#include "pd_settings.h"
#include "pd_rom_tweaks.h"
#include "pd_ui.h"
#include "pd_experience.h"
#include "pd_ui_internal.h"
#include "pd_map.h"
#include "host_io.h"
#include "rom_list.h"
#include "field_items.h"
#include "ff_music.h"
#include "shiny_odds.h"
#include "fly.h"
#include "display_filter.h"
#include "updater.h"
#include "ra.h"
#include "screen_filter.h"
#include "link_play.h"
#include "legality.h"
#include "stat_tools.h"
#include "presets.h"
#include "../app/src/main/cpp/pk_link.h"
#include "backdrop.h"
#include "wing_look.h"
#include "wide_camera.h"
#include "day_night.h"
#include "bps.h"
#include "bezel.h"
#include "wing_toast.h"
#include "shader.h"
#include "hud.h"
#include "pd_dex.h"
#include "pd_nuzlocke.h"
#include "pd_guide.h"
#include "../app/src/main/cpp/pk_call.h"
#include "../app/src/main/cpp/pk_host.h"
#include "quick_menu.h"
#include "wild.h"
#include "../app/src/main/cpp/pk_wide.h"
#include "../app/src/main/cpp/pk_rewind.h"
#include "pocket_os.h"
#include "save_edit.h"
#include <SDL.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/input.h>
#include <unistd.h>
#endif
#include <mgba-util/vfs.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba/gb/core.h>
#include <mgba/internal/gb/gb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "view_layout.h"
// The host's name in its window title, menu and SETTINGS.
#ifdef _WIN32
#define POCKET_HOST_NAME "PokeDaisy Windows"
#else
#define POCKET_HOST_NAME "PokeDaisy Linux"
#endif
// The window's size without POCKET_FULLSCREEN / POCKET_WINDOW, and the companion's
// canvas until a screen says otherwise (companion_size).
#define W 512
#define H 384
#define CANVAS_MAX_W 1024
#define CANVAS_MAX_H 768
#define PANEL_MAX_H 1440 // the wings' canvases: WING LOOK's COMPACT draws a full-height wing a pixel a pixel
static uint32_t pixels[CANVAS_MAX_W * CANVAS_MAX_H];
static int canvasW = W, canvasH = H;
static color_t video[256 * 256];
static struct pd_canvas canvas;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture, *gameTexture;
static SDL_Rect lastGameRect;
/* EXPAND (the app's pk_wide.c): the overworld rebuilt past the game's edges
 * into a window the screen's shape, at the scale the game's own picture gets
 * (ZOOM OUT: one whole scale less), so the original pixels stay where they were. */
static SDL_Texture *wideTexture;
static uint32_t wideSource[240 * 160], wideFrame[PK_WIDE_MAX_VIEW_W * PK_WIDE_MAX_VIEW_H];
static bool wideSupported, wideReady;
static unsigned wideW, wideH;
static int wideBoxW, wideBoxH; // the game's area beside the wings (0: the whole screen), EXPAND's window sized to it
static struct wide_camera wideCam; // EXPAND's zoom (wide_camera.h)
static float wideScale = 1;        // screen pixels per game pixel of the window now
static float wideShiftX, wideShiftY; // LOOK AROUND: the window's centre past the view's, game px (under one)
static float wideLevels[WIDE_CAMERA_LEVELS];
static int wideLevelCount = 1, wideCamAspect = -1;
/* A zoom level as a share of the game's picture at the closest (100%): ZOOM 75%, not a step number. */
static int zoom_percent(int level) {
  if (level <= 0 || level >= wideLevelCount || wideLevels[0] <= 0)
    return 100;
  return (int)(100.0f * wideLevels[level] / wideLevels[0] + 0.5f);
}
static SDL_AudioDeviceID audio;
static volatile sig_atomic_t running = 1;
static bool muted;
static int appError;
#ifdef POCKET_TEST
static unsigned audioNonzero;
#endif
static const char *root = ".";
/* plorpOS: --save FILE, the battery save another emulator keeps (its backups
 * then go to DATA/backups); NULL = DATA/saves/ROM.sav. */
static const char *saveFile;
static char message[128];
static unsigned messageUntil;
enum {
  A,
  B,
  SELECT,
  START,
  RIGHT,
  LEFT,
  UP,
  DOWN,
  R,
  L,
  X,
  Y,
  L2,
  R2,
  MENU,
  L3,
  SUP, // plorpOS: the left stick's directions apart (they're also the D-pad's), and the Brick's Fn keys
  SDOWN,
  SLEFT,
  SRIGHT,
  FN1,
  FN2,
  R3, // COMPANION VIEW: a tap = WINGS <-> BATTLE WINGS, a hold = the views to pick from
  NKEYS
};
static SDL_GameController *controller; // the first open pad (any open pad plays: pads[])
#define MAX_PADS 8
static SDL_GameController *pads[MAX_PADS];
static int padCount;
static bool rawTrimui;
static unsigned held, pressed;
static unsigned repeatAt[NKEYS];
static void stop(int sig) {
  (void)sig;
  running = 0;
}
static void notice(const char *s) {
  snprintf(message, sizeof message, "%s", s);
  messageUntil = SDL_GetTicks() + 2500;
  fprintf(stderr, "%s\n", s);
}
/* A newer release (updater.h) found while a game runs: said once; the prompt waits for the game list. */
static bool updateAnnounced;
static void update_announce(void) {
  if (updateAnnounced || updater_state() != UPDATER_AVAILABLE)
    return;
  updateAnnounced = true;
  char s[96];
  snprintf(s, sizeof s, "PokeDaisy %s is out: back on the game list to update", updater_latest());
  notice(s);
  messageUntil = SDL_GetTicks() + 6000;
}
/* mGBA logs every DMA and BIOS call at DEBUG; keep warnings and errors only, so
 * launch.sh's log stays readable. */
static void quiet_log(struct mLogger *l, int category, enum mLogLevel level,
                      const char *format, va_list args) {
  (void)l;
  if (!(level & (mLOG_FATAL | mLOG_ERROR | mLOG_WARN)))
    return;
  fprintf(stderr, "%s: ", mLogCategoryName(category));
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
}
static struct mLogger quietLogger = {.log = quiet_log};
/* The audio device. A handheld's ALSA device can go bad under the app (the
 * screen turned off and on: "ALSA write failed (unrecoverable)"), and SDL then
 * closes it for good; it's reopened as soon as SDL says so, or within a second
 * of it no longer playing. */
static bool audioLost;
static void audio_open(void) {
  SDL_AudioSpec want = {0};
  want.freq = 32768;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 512;
  audio = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
  if (audio)
    SDL_PauseAudioDevice(audio, 0);
  else
    fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());
}
static void audio_check(void) {
  static Uint32 next;
  bool stopped = audio && SDL_GetAudioDeviceStatus(audio) == SDL_AUDIO_STOPPED;
  if (!audioLost && !stopped)
    return;
  if (SDL_GetTicks() < next)
    return;
  next = SDL_GetTicks() + 1000;
  if (audio)
    SDL_CloseAudioDevice(audio);
  audio = 0;
  audio_open();
  audioLost = !audio;
  if (audio)
    fprintf(stderr, "Audio device reopened\n");
}
/* GAME VOLUME and MUTE GAME: the game's sound (and FF MUSIC's recording) at this many 256ths; silence still queues,
 * so the screen's pacing by the queue holds. */
static int gameGain = 256;
static void game_gain(int16_t *s, int n) {
  if (gameGain == 256)
    return;
  for (int i = 0; i < n; i++)
    s[i] = (int16_t)(s[i] * gameGain / 256);
}
/* GAME SPEED away from 1x: the sound keeps its pitch (stretch.h); 1 lets it straight through. */
static double stretchSpeed = 1;
static struct stretch gameStretch = {.speed = 1};
static void post_audio(struct mAVStream *s, blip_t *l, blip_t *r) {
  (void)s;
  int16_t out[2048];
  int n = blip_samples_avail(l);
  if (n > 1024)
    n = 1024;
  blip_read_samples(l, out, n, true);
  blip_read_samples(r, out + 1, n, true);
  game_gain(out, n * 2);
  if (stretchSpeed != 1 || gameStretch.speed != 1) {
    static int16_t stretched[2 * 8192];
    stretch_speed(&gameStretch, stretchSpeed);
    if (stretchSpeed != 1) {
      int made = stretch_run(&gameStretch, out, n, stretched, 8192);
      if (audio && !muted && SDL_GetQueuedAudioSize(audio) < 32768)
        SDL_QueueAudio(audio, stretched, made * 4);
      return;
    }
  }
#ifdef POCKET_TEST
  for (int i = 0; i < n * 2; i++)
    if (out[i])
      audioNonzero++;
#endif
  if (audio && !muted && SDL_GetQueuedAudioSize(audio) < 32768)
    SDL_QueueAudio(audio, out, n * 4);
}
static bool read_memory(void *ctx, uint32_t a, void *out, size_t n) {
  struct mCore *c = ctx;
  unsigned char *p = out;
  if (c->platform(c) == mPLATFORM_GB && a >= 0x10000) {
    // WRAM bank b's D000-DFFF as (b << 16) | address: Crystal switches the window mid-frame, so read the bank itself.
    unsigned bank = a >> 16, addr = a & 65535;
    struct GB *gb = c->board;
    if (bank > 7 || addr < 0xd000 || addr >= 0xe000 || n > 0xe000 - addr || !gb->memory.wram)
      return false;
    memcpy(p, gb->memory.wram + bank * 0x1000 + addr - 0xd000, n);
    return true;
  }
  if (c->platform(c) == mPLATFORM_GBA && a >= 0x08000000 && a < 0x0e000000) {
    size_t size = 0;
    const unsigned char *r = c->getMemoryBlock(c, 0x08, &size);
    size_t start = (a - 0x08000000) & 0x01ffffff;
    if (!r || start > size || n > size - start)
      return false;
    memcpy(p, r + start, n);
    return true;
  }
  for (size_t i = 0; i < n; i++)
    p[i] = c->busRead8(c, a + (uint32_t)i);
  return true;
}
static int keyboard(SDL_Keycode k) {
  switch (k) {
  case SDLK_z:
    return A;
  case SDLK_x:
    return B;
  case SDLK_BACKSPACE:
    return SELECT;
  case SDLK_RETURN:
    return START;
  case SDLK_RIGHT:
    return RIGHT;
  case SDLK_LEFT:
    return LEFT;
  case SDLK_UP:
    return UP;
  case SDLK_DOWN:
    return DOWN;
  case SDLK_e:
    return R;
  case SDLK_q:
    return L;
  case SDLK_TAB:
    return X;
  case SDLK_f:
    return Y;
  case SDLK_1:
    return L2;
  case SDLK_2:
    return R2;
  case SDLK_ESCAPE:
    return MENU;
  case SDLK_v:
    return R3;
  case SDLK_k:
    return L3;
  default:
    return -1;
  }
}
/* BUTTON LAYOUT and HOTKEYS (DATA/settings.ini): read at start, again when CONTROLS or the companion's SETTINGS change
 * them. SDL reports every pad's face buttons by position (the hint at start), so NINTENDO is "the right one is A" and
 * XBOX "the bottom one is A". */
static int buttonLayout;
static unsigned char hotkeyMap[PD_HOTKEY_BUTTONS];
/* The pad buttons HOTKEYS can give an action, in settings.ini's order. The sticks (L3, R3) and the triggers (L2, R2)
 * aren't SDL buttons there (-1): the game loop reads them (stick_hotkeys), a stick's action on its release, so L3 + R3
 * stays FULLSCREEN. */
static const int HOTKEY_BUTTONS[PD_HOTKEY_BUTTONS] = {
    SDL_CONTROLLER_BUTTON_GUIDE,   SDL_CONTROLLER_BUTTON_MISC1,   SDL_CONTROLLER_BUTTON_PADDLE1,
    SDL_CONTROLLER_BUTTON_PADDLE2, SDL_CONTROLLER_BUTTON_PADDLE3, SDL_CONTROLLER_BUTTON_PADDLE4, -1, -1, -1, -1};
static const char *const HOTKEY_BUTTON_NAMES[PD_HOTKEY_BUTTONS] = {
    "GUIDE / HOME", "SHARE", "PADDLE 1", "PADDLE 2", "PADDLE 3", "PADDLE 4", "L3", "R3", "L2 / LT", "R2 / RT"};
// HOTKEYS' rows: the triggers and sticks first, then the extra buttons (indexes into the lists above).
static const int HOTKEY_ROW_ORDER[PD_HOTKEY_BUTTONS] = {PD_HOTKEY_L2, PD_HOTKEY_R2, PD_HOTKEY_L3, PD_HOTKEY_R3, 0, 1, 2,
                                                        3,            4,            5};
enum {
  HA_USUAL, HA_MENU, HA_COMPANION, HA_FF, HA_SAVE_STATE, HA_LOAD_STATE, HA_SCREENSHOT, HA_FULLSCREEN,
  HA_DS_LAYOUT_NEXT, HA_DS_LAYOUT_BACK, HA_SWAP_SCREENS, HA_ZOOM_IN, HA_ZOOM_OUT, HA_REWIND,
};
// The app's HOTKEYS words (Hotkeys.kt's Action titles) where it has the same action.
static const char *const HOTKEY_ACTIONS[PD_HOTKEY_ACTIONS] = {
    "USUAL USE",  "GAME MENU",  "COMPANION", "FF TOGGLE",      "SAVE STATE",   "LOAD STATE", "SCREENSHOT",
    "FULLSCREEN", "DS LAYOUT", "DS LAYOUT BACK", "SWAP SCREENS", "ZOOM IN",    "ZOOM OUT",   "REWIND HOLD"};
static unsigned hostActions;  // HA_* bits pressed since the game loop last looked
static bool screenshotWanted; // the next present writes DATA/screenshots/...
static int gameTouch; // GAME TOUCH, enum pd_game_touch
static void orientation_set(int o);
static int saverMode = PD_SAVER_AUTO; // BATTERY SAVER, kept by controls_load and the game loop
static void controls_load(void) {
  char path[1100];
  snprintf(path, sizeof path, "%s/settings.ini", root);
  struct pd_settings st;
  pd_settings_load(&st, path);
  buttonLayout = st.buttonLayout;
  memcpy(hotkeyMap, st.hotkeys, sizeof hotkeyMap);
  gameTouch = st.gameTouch;
  orientation_set(st.orientation);
  saverMode = st.batterySaver;
}
/* REWIND HOLD: held, not pressed - the HOTKEYS button (or trigger, or stick) bound to it, or R on a keyboard while
 * REWIND is on. Any button bound to it keeps the ring running, as the setting does. */
static bool rewindHeld, rewindKey;
static bool rewind_hotkey(void) {
  for (int i = 0; i < PD_HOTKEY_BUTTONS; i++)
    if (hotkeyMap[i] == HA_REWIND)
      return true;
  return false;
}
static bool host_action(int action) {
  bool on = hostActions & (1u << action);
  hostActions &= ~(1u << action);
  return on;
}
static int controller_button(int button) {
  if (buttonLayout == PD_LAYOUT_XBOX)
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A:
      return A;
    case SDL_CONTROLLER_BUTTON_B:
      return B;
    case SDL_CONTROLLER_BUTTON_X:
      return X;
    case SDL_CONTROLLER_BUTTON_Y:
      return Y;
    }
  switch(button) {
    case SDL_CONTROLLER_BUTTON_A:return B;
    case SDL_CONTROLLER_BUTTON_B:return A;
    case SDL_CONTROLLER_BUTTON_X:return Y;
    case SDL_CONTROLLER_BUTTON_Y:return X;
    case SDL_CONTROLLER_BUTTON_BACK:return SELECT;
    case SDL_CONTROLLER_BUTTON_START:return START;
    case SDL_CONTROLLER_BUTTON_GUIDE:return MENU;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:return L3;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:return R3;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:return L;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:return R;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:return UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:return DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:return LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:return RIGHT;
    default:return -1;
  }
}
static int joy(int n) {
  static const int map[] = {B, A, Y, X, L, R, SELECT, START, MENU, -1, L2, R2};
  return n >= 0 && n < 12 ? map[n] : -1;
}
static void key(int k, bool down) {
  if (k < 0)
    return;
  unsigned bit = 1u << k;
  if (down) {
    if (!(held & bit)) {
      pressed |= bit;
      repeatAt[k] = SDL_GetTicks() + 400;
    }
    held |= bit;
  } else
    held &= ~bit;
}
/* Controllers come and go (a USB or Bluetooth pad, a handheld whose built-in
 * one shows up late): SDL reports the ones it has a mapping for as game
 * controllers (its own database, DATA/gamecontrollerdb.txt, or
 * SDL_GAMECONTROLLERCONFIG, which PortMaster sets). Every one is opened and any
 * of them plays: a handheld's built-in pad and an Xbox / Bluetooth pad side by
 * side (SDL sends button events only for an opened pad, so opening the first
 * alone left a second one dead). An unmapped device is named in the log so a
 * mapping can be added. */
static void controller_added(int index) {
  if (!SDL_IsGameController(index)) {
    char guid[64];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof guid);
    fprintf(stderr, "Controller without a mapping: %s (%s); add one to "
            "DATA/gamecontrollerdb.txt or SDL_GAMECONTROLLERCONFIG\n",
            SDL_JoystickNameForIndex(index) ? SDL_JoystickNameForIndex(index) : "?", guid);
    return;
  }
  SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
  for (int i = 0; i < padCount; i++)
    if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == id)
      return; // already open (a JOYDEVICEADDED and a CONTROLLERDEVICEADDED for one pad)
  if (padCount == MAX_PADS)
    return;
  SDL_GameController *pad = SDL_GameControllerOpen(index);
  if (!pad)
    return;
  pads[padCount++] = pad;
  controller = pads[0];
  fprintf(stderr, "Controller: %s\n", SDL_GameControllerName(pad));
}
static void controller_removed(SDL_JoystickID id) {
  for (int i = 0; i < padCount; i++) {
    if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) != id)
      continue;
    fprintf(stderr, "Controller gone: %s\n", SDL_GameControllerName(pads[i]));
    SDL_GameControllerClose(pads[i]);
    pads[i] = pads[--padCount];
    pads[padCount] = NULL;
    controller = padCount ? pads[0] : NULL;
    held = 0; // nothing stays pressed on a pad that's gone
    return;
  }
}
static void controllers_close(void) {
  while (padCount)
    SDL_GameControllerClose(pads[--padCount]);
  controller = NULL;
}
/* A pad turned on or off while a DS game runs (its own event loop). */
static void controller_device_event(const SDL_Event *e) {
  if (e->type == SDL_JOYDEVICEADDED && !SDL_IsGameController(e->jdevice.which))
    controller_added(e->jdevice.which);
  if (e->type == SDL_CONTROLLERDEVICEADDED)
    controller_added(e->cdevice.which);
  if (e->type == SDL_CONTROLLERDEVICEREMOVED)
    controller_removed(e->cdevice.which);
}
/* plorpOS: keys SDL's joystick never reports (below BTN_MISC: the Brick's Fn keys, KEY_F1 / KEY_F2), read
 * from the pad's own node beside SDL: POCKET_FN_KEYS=NODE:FN1CODE:FN2CODE. */
static void fn_keys(void) {
#ifdef __linux__
  static int fd = -2, code[2];
  if (fd == -2) {
    const char *spec = getenv("POCKET_FN_KEYS");
    char node[256];
    fd = -1;
    if (spec && sscanf(spec, "%255[^:]:%d:%d", node, &code[0], &code[1]) == 3)
      fd = open(node, O_RDONLY | O_NONBLOCK);
  }
  struct input_event ev;
  while (fd >= 0 && read(fd, &ev, sizeof ev) == (ssize_t)sizeof ev)
    for (int i = 0; i < 2; i++)
      if (ev.type == EV_KEY && ev.code == code[i] && ev.value != 2)
        key(FN1 + i, ev.value != 0);
#endif
}
/* FULLSCREEN: borderless, the desktop's own size (no mode change), switched any time by F11, Alt+Enter, L3 + R3 on a
 * pad, the GAME menu or SETTINGS; remembered in DATA/settings.ini. */
static bool fullscreenOn;
static void fullscreen_set(bool on) {
  if (!window || SDL_SetWindowFullscreen(window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0)
    return;
  fullscreenOn = on;
  char path[1100];
  snprintf(path, sizeof path, "%s/settings.ini", root);
  struct pd_settings st;
  pd_settings_load(&st, path);
  if (st.fullscreen != (on ? 1 : 0)) {
    st.fullscreen = on ? 1 : 0;
    pd_settings_save(&st, path);
  }
}
static void fullscreen_toggle(void) { fullscreen_set(!fullscreenOn); }
static bool stickChord; // L3 + R3 went to FULLSCREEN since both sticks were last up
#define R3_HOLD_MS 450u // R3 held this long opens the views to pick from instead of switching the view
static int layout_pick(const char *title, const struct pd_layout_option *options, int count, int current);
/* F11 / Alt+Enter on a keyboard, L3 + R3 on a pad (the second stick pressed while the first is held). True when the
 * event was the toggle's (the game doesn't get it). */
static bool fullscreen_event(const SDL_Event *e) {
  if (e->type == SDL_KEYDOWN && !e->key.repeat &&
      (e->key.keysym.sym == SDLK_F11 ||
       ((e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER) && (e->key.keysym.mod & KMOD_ALT)))) {
    fullscreen_toggle();
    return true;
  }
  if ((e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) &&
      (e->key.keysym.sym == SDLK_F11 ||
       ((e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER) && (e->key.keysym.mod & KMOD_ALT))))
    return true;
  if (e->type == SDL_CONTROLLERBUTTONDOWN &&
      (e->cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK || e->cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK)) {
    SDL_GameController *pad = SDL_GameControllerFromInstanceID(e->cbutton.which);
    int other = e->cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK ? SDL_CONTROLLER_BUTTON_RIGHTSTICK
                                                                      : SDL_CONTROLLER_BUTTON_LEFTSTICK;
    if (pad && SDL_GameControllerGetButton(pad, other)) {
      stickChord = true; // neither stick's HOTKEYS action when they're let go
      fullscreen_toggle();
      return true;
    }
  }
  return false;
}
/* Is this a handheld's screen (under 10 inches across, by the display's DPI)? An unknown size is not. */
static bool handheld_screen(void) {
  float ddpi = 0, hdpi = 0, vdpi = 0;
  SDL_DisplayMode mode;
  if (SDL_GetDesktopDisplayMode(0, &mode) || SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) || hdpi < 50 || vdpi < 50)
    return false;
  float w = mode.w / hdpi, h = mode.h / vdpi;
  return w * w + h * h < 100.0f;
}
/* A HOTKEYS button with an action: FULLSCREEN and SCREENSHOT happen here, the rest wait in hostActions for the game
 * loop (or the DS loop, through pd_ds_host_actions). True = the button was the hotkey's, the game doesn't see it. */
static void hotkey_fire(int action) {
  if (action == HA_FULLSCREEN)
    fullscreen_toggle();
  else if (action == HA_SCREENSHOT)
    screenshotWanted = true;
  else if (action)
    hostActions |= 1u << action;
}
static bool hotkey_event(const SDL_Event *e) {
  if (e->type != SDL_CONTROLLERBUTTONDOWN && e->type != SDL_CONTROLLERBUTTONUP)
    return false;
  for (int i = 0; i < PD_HOTKEY_BUTTONS; i++) {
    if (HOTKEY_BUTTONS[i] < 0 || e->cbutton.button != HOTKEY_BUTTONS[i] || !hotkeyMap[i])
      continue;
    if (hotkeyMap[i] == HA_REWIND) {
      rewindHeld = e->type == SDL_CONTROLLERBUTTONDOWN;
      return true;
    }
    if (e->type == SDL_CONTROLLERBUTTONDOWN) {
      if (hotkeyMap[i] == HA_FULLSCREEN)
        fullscreen_toggle();
      else if (hotkeyMap[i] == HA_SCREENSHOT)
        screenshotWanted = true;
      else
        hostActions |= 1u << hotkeyMap[i];
    }
    return true;
  }
  return false;
}
/* Touch and the mouse on the host's own screens (the game list, its menus, the game menu, the companion): presses in
 * the pixels of the canvas last drawn, where it was drawn (canvasAt; none while only the game shows). A finger or
 * the left button; tap_take hands them to whichever screen is up, and the rest are dropped at the next events(). */
void pd_host_point(float nx, float ny, float *px, float *py);
static SDL_Rect canvasAt;
static int canvasAtW, canvasAtH;
struct tap {
  int x, y;
  bool down; // down / still held (moved), else let go
};
static struct tap taps[32];
static int tapCount;
static bool tapHeld;
static SDL_FingerID tapFinger = -1;
/* The game list's extras: a file dropped on the window (SET BOX ART), the mouse wheel, a right click (a game's
 * options) and typing (RENAME), each taken by the screen that wants it. */
static char *droppedFile;
static int wheelSteps;
static bool rightClicked;
static int rightClickX, rightClickY;
static bool wingClicked; // a click / touch this frame, in the renderer's pixels: on a wing, it opens that page
static float wingClickX, wingClickY;
static bool textEntry;   // RENAME's keyboard is up: the keyboard types instead of pressing buttons
static char textTyped[64];
static int textBackspaces;
static bool textEnter, textEscape;
static bool window_to_canvas(int mx, int my, int *x, int *y);
static void canvas_at(SDL_Rect at, int w, int h) {
  canvasAt = at;
  canvasAtW = w;
  canvasAtH = h;
}
static void tap_push(float nx, float ny, int phase) { // phase: 0 down, 1 moved, 2 up
  if (tapCount >= (int)(sizeof taps / sizeof *taps))
    return;
  float px, py;
  pd_host_point(nx, ny, &px, &py);
  bool inside = canvasAt.w > 0 && canvasAt.h > 0 && px >= canvasAt.x && py >= canvasAt.y &&
                px < canvasAt.x + canvasAt.w && py < canvasAt.y + canvasAt.h;
  if (phase == 0 && !inside)
    return;
  if (phase != 0 && !tapHeld)
    return;
  int x = canvasAt.w > 0 ? (int)((px - canvasAt.x) * canvasAtW / canvasAt.w) : 0;
  int y = canvasAt.h > 0 ? (int)((py - canvasAt.y) * canvasAtH / canvasAt.h) : 0;
  taps[tapCount++] = (struct tap){x, y, phase != 2};
  tapHeld = phase != 2;
}
static bool window_to_canvas(int mx, int my, int *x, int *y) {
  int ww = 1, wh = 1;
  SDL_GetWindowSize(window, &ww, &wh);
  float px, py;
  pd_host_point((float)mx / (ww > 0 ? ww : 1), (float)my / (wh > 0 ? wh : 1), &px, &py);
  if (canvasAt.w <= 0 || canvasAt.h <= 0 || px < canvasAt.x || py < canvasAt.y || px >= canvasAt.x + canvasAt.w ||
      py >= canvasAt.y + canvasAt.h)
    return false;
  *x = (int)((px - canvasAt.x) * canvasAtW / canvasAt.w);
  *y = (int)((py - canvasAt.y) * canvasAtH / canvasAt.h);
  return true;
}
/* Where a mouse event happened, in window points. SDL_GetMouseState is where the mouse is when the event is read, after
 * any motion queued behind it (a click while the mouse moves, a touchpad tap), so a click near a wing button could land
 * on the wing; the event's own x and y are the renderer's logical size when it has one, turned back here. */
static void mouse_event_at(int ex, int ey, int *mx, int *my) {
  int lw = 0, lh = 0;
  SDL_RenderGetLogicalSize(renderer, &lw, &lh);
  if (lw <= 0 || lh <= 0) {
    *mx = ex;
    *my = ey;
    return;
  }
  SDL_RenderLogicalToWindow(renderer, (float)ex, (float)ey, mx, my);
}
/* A finger's own place in the window. SDL's renderer moves a finger event's x / y into its viewport (and clamps a
 * touch outside it to the viewport's edge) before the event reaches us, unlike a mouse's (#143 turns those back): with
 * the game letterboxed in a logical size, a tap on the right wing's SAVE STATE or CLOSE GAME landed on the wing's
 * edge and opened the companion. This watch, added before the renderer is made (so it runs before the renderer's),
 * keeps each finger event's x / y as the window had them; finger_at() finds them again. */
#define RAW_FINGERS 64
static struct {
  Uint32 type, timestamp;
  SDL_FingerID id;
  float x, y;
} rawFingers[RAW_FINGERS];
static unsigned rawFingerNext;
static int SDLCALL raw_finger_watch(void *userdata, SDL_Event *e) {
  (void)userdata;
  if (e->type == SDL_FINGERDOWN || e->type == SDL_FINGERUP || e->type == SDL_FINGERMOTION) {
    unsigned i = rawFingerNext++ % RAW_FINGERS;
    rawFingers[i].type = e->type;
    rawFingers[i].timestamp = e->tfinger.timestamp;
    rawFingers[i].id = e->tfinger.fingerId;
    rawFingers[i].x = e->tfinger.x;
    rawFingers[i].y = e->tfinger.y;
  }
  return 1;
}
static void finger_at(const SDL_Event *e, float *x, float *y) {
  *x = e->tfinger.x;
  *y = e->tfinger.y;
  for (unsigned k = 1; k <= RAW_FINGERS && k <= rawFingerNext; k++) {
    unsigned i = (rawFingerNext - k) % RAW_FINGERS;
    if (rawFingers[i].type == e->type && rawFingers[i].timestamp == e->tfinger.timestamp &&
        rawFingers[i].id == e->tfinger.fingerId) {
      *x = rawFingers[i].x;
      *y = rawFingers[i].y;
      return;
    }
  }
}
/* A left click or a finger's touch (not SDL's mouse copy of a touch: the finger's own event stands for it) in the
 * renderer's pixels, where the wings and their buttons are measured. False for any other event. */
static bool click_point(const SDL_Event *e, float *px, float *py) {
  if (!((e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT && e->button.which != SDL_TOUCH_MOUSEID) ||
        e->type == SDL_FINGERDOWN))
    return false;
  int mx = 0, my = 0, ww = 1, wh = 1;
  if (e->type != SDL_FINGERDOWN)
    mouse_event_at(e->button.x, e->button.y, &mx, &my);
  SDL_GetWindowSize(window, &ww, &wh);
  float nx = (float)mx / (ww > 0 ? ww : 1), ny = (float)my / (wh > 0 ? wh : 1);
  if (e->type == SDL_FINGERDOWN)
    finger_at(e, &nx, &ny);
  pd_host_point(nx, ny, px, py);
  return true;
}
/* The renderer, with the finger watch ahead of its own (finger_at). */
static SDL_Renderer *make_renderer(SDL_Window *w, Uint32 flags) {
  static bool watching;
  if (!watching)
    SDL_AddEventWatch(raw_finger_watch, NULL);
  watching = true;
  return SDL_CreateRenderer(w, -1, flags);
}
static void tap_event(const SDL_Event *e) {
  if ((e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) && e->button.button == SDL_BUTTON_LEFT &&
      e->button.which != SDL_TOUCH_MOUSEID) {
    int mx, my, ww = 1, wh = 1;
    mouse_event_at(e->button.x, e->button.y, &mx, &my);
    SDL_GetWindowSize(window, &ww, &wh);
    tap_push((float)mx / (ww > 0 ? ww : 1), (float)my / (wh > 0 ? wh : 1), e->type == SDL_MOUSEBUTTONDOWN ? 0 : 2);
  } else if (e->type == SDL_MOUSEMOTION && (e->motion.state & SDL_BUTTON_LMASK) && e->motion.which != SDL_TOUCH_MOUSEID &&
             tapHeld && tapFinger < 0) {
    int mx, my, ww = 1, wh = 1;
    mouse_event_at(e->motion.x, e->motion.y, &mx, &my);
    SDL_GetWindowSize(window, &ww, &wh);
    tap_push((float)mx / (ww > 0 ? ww : 1), (float)my / (wh > 0 ? wh : 1), 1);
  } else if (e->type == SDL_FINGERDOWN && !tapHeld) {
    float fx, fy;
    finger_at(e, &fx, &fy);
    tap_push(fx, fy, 0);
    if (tapHeld)
      tapFinger = e->tfinger.fingerId;
  } else if ((e->type == SDL_FINGERMOTION || e->type == SDL_FINGERUP) && tapHeld && e->tfinger.fingerId == tapFinger) {
    float fx, fy;
    finger_at(e, &fx, &fy);
    tap_push(fx, fy, e->type == SDL_FINGERUP ? 2 : 1);
    if (e->type == SDL_FINGERUP)
      tapFinger = -1;
  }
}
/* POCKET_TAPS="N:X:Y,..." (headless checks): a finger down at (X, Y) in hundredths of the window on events() call
 * N, up on the next. */
static int tapScriptTick;
static void tap_script(void) {
  const char *s = getenv("POCKET_TAPS");
  if (!s)
    return;
  int tick = ++tapScriptTick;
  while (*s) {
    int n, x, y, used = 0;
    if (sscanf(s, "%d:%d:%d%n", &n, &x, &y, &used) != 3 || !used)
      break;
    if (tick == n || tick == n + 1) {
      SDL_Event f = {0};
      f.type = tick == n ? SDL_FINGERDOWN : SDL_FINGERUP;
      f.tfinger.fingerId = 7;
      f.tfinger.x = x / 100.0f;
      f.tfinger.y = y / 100.0f;
      SDL_PushEvent(&f);
    }
    s += used;
    if (*s == ',')
      s++;
  }
}
/* The row a tap picked on a pick-list drawn as the canvas (or -1). */
/* COMPANION VIEW's drawings for LAYOUT: each view worked out by view_layout() on a 16:9 screen, in the state that
 * shows it best (the companion open, BATTLE WINGS and WINGS in a battle). WINDOW and SIDE PANEL are the app's. */
static const struct pd_layout_option *companion_layouts(void) {
  static struct pd_layout_option options[POCKET_VIEWS];
  static bool made;
  if (made)
    return options;
  made = true;
  const int sw = 1280, sh = 720;
  for (int v = 0; v < POCKET_VIEWS; v++) {
    struct view_state st = {.screenW = sw, .screenH = sh, .frameW = 240, .frameH = 160,
                            .companionOpen = v != POCKET_VIEW_BATTLE_WINGS && v != POCKET_VIEW_WINGS,
                            .battle = v == POCKET_VIEW_BATTLE_WINGS || v == POCKET_VIEW_WINGS,
                            .panelLocked = v == POCKET_VIEW_SIDE_PANEL};
    struct view_layout l;
    view_layout(v, &st, &l);
    struct pd_layout_option *o = &options[v];
    o->name = pd_companion_view_name(v);
    o->original = v == POCKET_VIEW_WINDOW || v == POCKET_VIEW_SIDE_PANEL;
#define BOX(r, p)                                                                                                      \
  do {                                                                                                                 \
    if ((r).w > 0 && (r).h > 0 && o->boxCount < PD_LAYOUT_BOXES)                                                       \
      o->box[o->boxCount++] = (struct pd_layout_box){(short)((r).x * 1000 / sw), (short)((r).y * 1000 / sh),          \
                                                     (short)((r).w * 1000 / sw), (short)((r).h * 1000 / sh), (p)};    \
  } while (0)
    BOX(l.game, l.dim ? PD_PART_DIM : PD_PART_GAME);
    BOX(l.left, PD_PART_PANEL);
    BOX(l.right, PD_PART_PANEL);
    BOX(l.top, PD_PART_PANEL);
    BOX(l.bottom, PD_PART_PANEL);
    BOX(l.companion, l.companionGlass ? PD_PART_GLASS : PD_PART_COMPANION);
#undef BOX
  }
  return options;
}
static int menu_taps(struct pd_menu *m) {
  int row = -1;
  for (int i = 0; i < tapCount; i++) {
    int r = pd_menu_touch(m, taps[i].x, taps[i].y, taps[i].down);
    if (r >= 0)
      row = r;
  }
  tapCount = 0;
  return row;
}
/* POCKET_KEYS="N:K:LEN,..." (headless checks): button K (A 0, B 1, SELECT 2, START 3, RIGHT 4, LEFT 5, UP 6, DOWN 7,
 * R 8, L 9, ...) held from events() call N for LEN calls; K 100 + HA_* fires that HOTKEYS action instead. */
static int keyScriptTick;
static void key_script(void) {
  const char *s = getenv("POCKET_KEYS");
  if (!s)
    return;
  int tick = ++keyScriptTick;
  while (*s) {
    int n, k, len, used = 0;
    if (sscanf(s, "%d:%d:%d%n", &n, &k, &len, &used) != 3 || !used)
      break;
    if (k >= 100) { // 100 + HA_*: a HOTKEYS action (zoom, the companion...), fired once
      if (tick == n)
        hotkey_fire(k - 100);
    } else {
      if (tick == n)
        key(k, true);
      if (tick == n + len)
        key(k, false);
    }
    s += used;
    if (*s == ',')
      s++;
  }
}
static uint32_t wingActivity; // the last input or wing change (draw_panel: the still wings dim)
static uint32_t lastInputAt;  // the last input (the bezel bars' BUTTON HINTS)
static void events(void) {
   ra_pump(); // RetroAchievements: the server's answers, on the main thread
 pressed = 0;
  tapCount = 0;
  wingClicked = false;
  tap_script();
  key_script();
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT)
      running = 0;
    if (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_JOYBUTTONDOWN ||
        e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_FINGERDOWN || e.type == SDL_CONTROLLERAXISMOTION)
      wingActivity = lastInputAt = SDL_GetTicks();
    controller_device_event(&e);
    if (click_point(&e, &wingClickX, &wingClickY))
      wingClicked = true;
    tap_event(&e);
    if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
      tapHeld = false;
    if (fullscreen_event(&e))
      continue;
    if (e.type == SDL_AUDIODEVICEREMOVED && !e.adevice.iscapture && audio &&
        e.adevice.which == audio)
      audioLost = true;
    if (e.type == SDL_DROPFILE) {
      free(droppedFile);
      droppedFile = e.drop.file ? strdup(e.drop.file) : NULL;
      SDL_free(e.drop.file);
      continue;
    }
    if (e.type == SDL_MOUSEWHEEL) {
      wheelSteps += e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
      continue;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT && e.button.which != SDL_TOUCH_MOUSEID) {
      int mx, my;
      mouse_event_at(e.button.x, e.button.y, &mx, &my);
      rightClicked = window_to_canvas(mx, my, &rightClickX, &rightClickY);
      continue;
    }
    if (textEntry && e.type == SDL_TEXTINPUT) {
      size_t n = strlen(textTyped);
      snprintf(textTyped + n, sizeof textTyped - n, "%s", e.text.text);
      continue;
    }
    if (textEntry && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)) {
      SDL_Keycode k = e.key.keysym.sym;
      bool arrow = k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT;
      if (!arrow) {
        if (e.type == SDL_KEYDOWN) {
          if (k == SDLK_BACKSPACE)
            textBackspaces++;
          else if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
            textEnter = true;
          else if (k == SDLK_ESCAPE)
            textEscape = true;
        }
        continue; // typing, not buttons
      }
    }
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
      if (!e.key.repeat)
        key(keyboard(e.key.keysym.sym), e.type == SDL_KEYDOWN);
      if (e.key.keysym.sym == SDLK_r)
        rewindKey = e.type == SDL_KEYDOWN;
    }
    if (hotkey_event(&e))
      continue;
    if(e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP)
      key(controller_button(e.cbutton.button),e.type == SDL_CONTROLLERBUTTONDOWN);
    if(e.type == SDL_CONTROLLERAXISMOTION) {
      if(e.caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT)key(L2,e.caxis.value>16000);
      if(e.caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT)key(R2,e.caxis.value>16000);
      if(e.caxis.axis==SDL_CONTROLLER_AXIS_LEFTX) {key(LEFT,e.caxis.value < -16000);key(RIGHT,e.caxis.value > 16000);
        key(SLEFT,e.caxis.value < -16000);key(SRIGHT,e.caxis.value > 16000);}
      if(e.caxis.axis==SDL_CONTROLLER_AXIS_LEFTY) {key(UP,e.caxis.value < -16000);key(DOWN,e.caxis.value > 16000);
        key(SUP,e.caxis.value < -16000);key(SDOWN,e.caxis.value > 16000);}
    }
    if (!controller && rawTrimui && (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP))
      key(joy(e.jbutton.button), e.type == SDL_JOYBUTTONDOWN);
    if (!controller && rawTrimui && e.type == SDL_JOYHATMOTION) {
      key(UP, e.jhat.value & SDL_HAT_UP);
      key(DOWN, e.jhat.value & SDL_HAT_DOWN);
      key(LEFT, e.jhat.value & SDL_HAT_LEFT);
      key(RIGHT, e.jhat.value & SDL_HAT_RIGHT);
    }
    if (!controller && rawTrimui && e.type == SDL_JOYAXISMOTION) {
      if (e.jaxis.axis == 2)
        key(L2, e.jaxis.value > 16000);
      if (e.jaxis.axis == 5)
        key(R2, e.jaxis.value > 16000);
    }
    if (e.type == SDL_WINDOWEVENT &&
        e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
      held = 0;
  }
  fn_keys();
}
static void repeat_directions(void) {
  for (int k = RIGHT; k <= DOWN; k++)
    if ((held & (1u << k)) && SDL_GetTicks() >= repeatAt[k]) {
      pressed |= 1u << k;
      repeatAt[k] = SDL_GetTicks() + 90;
    }
}
static bool down(int k) { return (pressed & (1u << k)) != 0; }
/* plorpOS: its GBA hotkeys, in the file POCKET_HOTKEYS_FILE names - Diatom's wire on its first line
 * ("l2:ff,d.x:savestate,select+r1:rewind"), the modifier a binding naming none holds on its second - read
 * at start and again after a pause (its in-game menu can change them). A trigger is held with its modifier,
 * or alone with "d.". The game doesn't get a trigger while it's a hotkey's (hotkeyMask). Set at all, the
 * host owns MENU - its own in-game menu and the modifier - and MENU no longer opens this GAME menu
 * (START+SELECT still does). */
enum { HK_FF, HK_REWIND, HK_SAVE, HK_LOAD, HK_HUD, HK_ACTIONS };
static struct {
  int action, key, mod; // mod -1: alone
} hotkey[16];
static int hotkeyN;
static unsigned hotkeyMask;
static bool hostMenu, rewindBound; // rewindBound: a REWIND hotkey, which keeps the ring on (as plorpOS's games)
static int key_named(const char *s) {
  static const struct {
    const char *name;
    int key;
  } names[] = {{"a", A},       {"b", B},         {"x", X},         {"y", Y},           {"l1", L},
               {"r1", R},      {"l2", L2},       {"r2", R2},       {"up", UP},         {"down", DOWN},
               {"left", LEFT}, {"right", RIGHT}, {"menu", MENU},   {"select", SELECT}, {"l3", L3},
               {"sup", SUP},   {"sdown", SDOWN}, {"sleft", SLEFT}, {"sright", SRIGHT}, {"fn1", FN1},
               {"fn2", FN2},   {"r3", R3}};
  for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
    if (!strcmp(s, names[i].name))
      return names[i].key;
  return -1;
}
static void hotkeys_load(void) {
  static const char *const actions[HK_ACTIONS] = {"ff", "rewind", "savestate", "loadstate", "hud"};
  const char *path = getenv("POCKET_HOTKEYS_FILE");
  char buf[512] = "", modBuf[32] = "menu", *mod = modBuf, *save = NULL;
  FILE *f = path ? fopen(path, "r") : NULL;
  hostMenu = path != NULL;
  hotkeyN = 0;
  rewindBound = false;
  if (f) {
    if (!fgets(buf, sizeof buf, f))
      buf[0] = 0;
    if (!fgets(modBuf, sizeof modBuf, f))
      snprintf(modBuf, sizeof modBuf, "menu");
    fclose(f);
  }
  buf[strcspn(buf, "\r\n")] = 0;
  modBuf[strcspn(modBuf, "\r\n")] = 0;
  for (char *t = strtok_r(buf, ",", &save); t && hotkeyN < 16; t = strtok_r(NULL, ",", &save)) {
    char *colon = strchr(t, ':'), *plus = strchr(t, '+');
    int action = -1, m, k;
    if (!colon)
      continue;
    *colon = 0;
    for (int i = 0; i < HK_ACTIONS; i++)
      if (!strcmp(colon + 1, actions[i]))
        action = i;
    if (plus) {
      *plus = 0;
      m = key_named(t);
      k = key_named(plus + 1);
    } else if (!strncmp(t, "d.", 2)) {
      m = -2;
      k = key_named(t + 2);
    } else {
      m = key_named(mod);
      k = key_named(t);
    }
    // Others (screenshot, turbo, the right stick, the GKD's Home) aren't this host's.
    if (action < 0 || k < 0 || m == -1)
      continue;
    rewindBound |= action == HK_REWIND;
    hotkey[hotkeyN].action = action;
    hotkey[hotkeyN].key = k;
    hotkey[hotkeyN++].mod = m < 0 ? -1 : m;
  }
}
/* The actions held this frame; *fired: those whose trigger went down now. */
static unsigned hotkeys_held(unsigned *fired) {
  unsigned on = 0;
  *fired = 0;
  hotkeyMask = 0;
  for (int i = 0; i < hotkeyN; i++) {
    unsigned bit = 1u << hotkey[i].key;
    bool mod = hotkey[i].mod >= 0 ? (held & (1u << hotkey[i].mod)) != 0
                                  : !(hostMenu && (held & (1u << MENU)));
    // pressed too: a tap shorter than a frame is let go before this sees it.
    if (!((held | pressed) & bit) || !mod)
      continue;
    on |= 1u << hotkey[i].action;
    if (pressed & bit)
      *fired |= 1u << hotkey[i].action;
    pressed &= ~bit;
    hotkeyMask |= bit;
    // A stick direction is the D-pad's too: the game doesn't get that either.
    if (hotkey[i].key >= SUP && hotkey[i].key <= SRIGHT)
      hotkeyMask |= 1u << (hotkey[i].key == SUP ? UP : hotkey[i].key == SDOWN ? DOWN
                           : hotkey[i].key == SLEFT ? LEFT : RIGHT);
  }
  return on;
}
/* HOTKEYS on the triggers and sticks, over the game only (the companion keeps them: L3 locks a SIDE PANEL): a
 * trigger's action as it's pressed, a stick's as it's let go, unless it was half of L3 + R3 (FULLSCREEN). */
static void stick_hotkeys(void) {
  static const int keys[4] = {L2, R2, L3, R3}, slots[4] = {PD_HOTKEY_L2, PD_HOTKEY_R2, PD_HOTKEY_L3, PD_HOTKEY_R3};
  static unsigned was;
  for (int i = 0; i < 4; i++) {
    unsigned bit = 1u << keys[i];
    int action = hotkeyMap[slots[i]];
    if (!action)
      continue;
    if (action == HA_REWIND) { // held as long as the trigger or stick is
      rewindHeld = (held & bit) != 0;
      hotkeyMask |= bit;
      continue;
    }
    if (i < 2 ? (pressed & bit) != 0 : (was & bit) && !(held & bit) && !stickChord)
      hotkey_fire(action);
    hotkeyMask |= bit;
  }
  if ((held & (1u << L3)) && (held & (1u << R3)))
    stickChord = true;
  was = held;
  if (!(held & ((1u << L3) | (1u << R3))))
    stickChord = false;
}
/* The companion's canvas for an ow x oh screen: the largest whole scale that
 * keeps it at least UI SIZE's least size (LARGE: 320x240, the 3DS's, which every tab is laid out for; MEDIUM
 * 427x320; SMALL 560x420), so its pixel text is about as big on every panel: on 1920x1080 LARGE is 480x270 at 4x,
 * MEDIUM 640x360 at 3x, SMALL 960x540 at 2x. The menus, the HUD and the wings all follow this canvas's scale.
 * POCKET_COMPANION_SCALE=N picks the scale instead. */
static int uiSize = 1; // UI SIZE: 0 SMALL, 1 MEDIUM, 2 LARGE
static void companion_size(int ow, int oh, int *w, int *h) {
  static const int leastW[3] = {560, 427, 320}, leastH[3] = {420, 320, 240};
  int u = uiSize >= 0 && uiSize < 3 ? uiSize : 1;
  int scale = ow / leastW[u] < oh / leastH[u] ? ow / leastW[u] : oh / leastH[u];
  const char *forced = getenv("POCKET_COMPANION_SCALE");
  if (forced && atoi(forced) > 0)
    scale = atoi(forced);
  if (scale < 1)
    scale = 1;
  *w = ow / scale;
  *h = oh / scale;
  if (*w > CANVAS_MAX_W)
    *w = CANVAS_MAX_W;
  if (*h > CANVAS_MAX_H)
    *h = CANVAS_MAX_H;
  if (*w < 320)
    *w = 320;
  if (*h < 240)
    *h = 240;
}
/* Before the companion draws: the canvas follows the window (a resize, a
 * rotated panel), with a texture to match. */
static void companion_canvas_for(int ow, int oh) {
  int w, h;
  companion_size(ow, oh, &w, &h);
  if (w == canvasW && h == canvasH && texture)
    return;
  SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                     SDL_TEXTUREACCESS_STREAMING, w, h);
  if (!t) {
    notice(SDL_GetError());
    return;
  }
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
  if (texture)
    SDL_DestroyTexture(texture);
  texture = t;
  canvasW = w;
  canvasH = h;
  pd_canvas_init(&canvas, w, h, pixels);
}
static void companion_canvas(void) {
  int ow = W, oh = H;
  if (SDL_GetRendererOutputSize(renderer, &ow, &oh) < 0 || ow <= 0 || oh <= 0)
    ow = W, oh = H;
  companion_canvas_for(ow, oh);
}
/* How long the last presents waited (the driver holds each one for the screen's
 * refresh), for POCKET_PERF. */
static double presentMs, emuMs, wideMs;
static double ms_since(uint64_t t) {
  return (double)(SDL_GetPerformanceCounter() - t) * 1000.0 / SDL_GetPerformanceFrequency();
}
/* LCD (SETTINGS > FILTER): Keegan Whitacre's output-space aperture mask (pokedaisy-brick dfc62a0), multiplied over the
 * whole presented frame; no emulator or companion buffer changes. */
static bool lcdEnabled;
static SDL_Texture *lcdTexture;
static int lcdW, lcdH;
static void render_lcd(void) {
  if (!lcdEnabled)
    return;
  int w, h;
  if (SDL_GetRendererOutputSize(renderer, &w, &h) < 0 || w <= 0 || h <= 0)
    return;
  if (!lcdTexture || lcdW != w || lcdH != h) {
    SDL_DestroyTexture(lcdTexture);
    lcdTexture = NULL;
    uint32_t *mask = malloc((size_t)w * h * 4);
    if (!mask)
      return;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
        mask[y * w + x] = pocket_lcd_pixel(x, y);
    lcdTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (lcdTexture && (SDL_UpdateTexture(lcdTexture, NULL, mask, w * 4) < 0 ||
                       SDL_SetTextureBlendMode(lcdTexture, SDL_BLENDMODE_MOD) < 0)) {
      SDL_DestroyTexture(lcdTexture);
      lcdTexture = NULL;
    }
    free(mask);
    lcdW = w;
    lcdH = h;
  }
  if (!lcdTexture)
    return;
  int lw, lh;
  SDL_RenderGetLogicalSize(renderer, &lw, &lh);
  SDL_bool integer = SDL_RenderGetIntegerScale(renderer);
  SDL_RenderSetLogicalSize(renderer, w, h);
  SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
  SDL_RenderCopy(renderer, lcdTexture, NULL, NULL);
  SDL_RenderSetLogicalSize(renderer, lw, lh);
  SDL_RenderSetIntegerScale(renderer, integer);
}
/* FILTER's LCD, LCD PAPER, SCANLINES and CRT (screen_filter.h): a mask over the game's picture, lined up with its pixels,
 * made again when the picture's size, the game's or the filter changes. */
static int gameFilter, gameGrid;
static SDL_Texture *filterTexture;
static int filterKey[6];
static void render_game_filter(const SDL_Rect *at, int sw, int sh) {
  if (!screen_filter_on_game(gameFilter) || at->w <= 0 || at->h <= 0 || at->w > 8192 || at->h > 8192)
    return;
  int key[6] = {gameFilter, gameGrid, at->w, at->h, sw, sh};
  if (!filterTexture || memcmp(key, filterKey, sizeof key)) {
    if (filterTexture)
      SDL_DestroyTexture(filterTexture);
    filterTexture = NULL;
    uint32_t *mask = malloc((size_t)at->w * at->h * 4);
    if (!mask)
      return;
    for (int y = 0; y < at->h; y++)
      for (int x = 0; x < at->w; x++)
        mask[(size_t)y * at->w + x] = screen_filter_pixel(gameFilter, gameGrid, x, y, at->w, at->h, sw, sh);
    filterTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, at->w, at->h);
    if (filterTexture && (SDL_UpdateTexture(filterTexture, NULL, mask, at->w * 4) < 0 ||
                          SDL_SetTextureBlendMode(filterTexture, SDL_BLENDMODE_MOD) < 0)) {
      SDL_DestroyTexture(filterTexture);
      filterTexture = NULL;
    }
    free(mask);
    memcpy(filterKey, key, sizeof key);
  }
  if (filterTexture)
    SDL_RenderCopy(renderer, filterTexture, NULL, at);
}
static char shotName[256] = "pokedaisy"; // the game's file name (screenshots are named after it)
static bool capture_frame(const char *path, unsigned *lit);
/* SCREENSHOT: DATA/screenshots/<game>-<date>-<time>.png, of the frame about to be shown. */
static void screenshot_now(void) {
  if (!screenshotWanted)
    return;
  screenshotWanted = false;
  char dir[1100], path[1500], when[32];
  snprintf(dir, sizeof dir, "%s/screenshots", root);
  pocket_mkdir(dir);
  time_t now = time(NULL);
  strftime(when, sizeof when, "%Y%m%d-%H%M%S", localtime(&now));
  snprintf(path, sizeof path, "%s/%s-%s.png", dir, shotName, when);
  notice(capture_frame(path, NULL) ? "Screenshot saved" : "Screenshot failed");
}
/* DISPLAY ORIENTATION (SETTINGS > ORIENTATION, the game list's SCREEN): while turned, everything is drawn into
 * rotTarget, a texture the size of the turned screen (SDL_GetRendererOutputSize then answers its size, so every
 * layout follows by itself), and each present copies it to the window turned a quarter at a time, clockwise. */
static int orientation;
static SDL_Texture *rotTarget;
static int rotW, rotH, winOutW = 1, winOutH = 1;
/* With no target set: (re)make the target for the window's size and draw into it, or drop it when not turned. */
static void rotation_ready(void) {
  if (!renderer)
    return;
  if (SDL_GetRendererOutputSize(renderer, &winOutW, &winOutH) < 0 || winOutW <= 0 || winOutH <= 0)
    winOutW = winOutH = 1;
  if (orientation == PD_ORIENT_LANDSCAPE) {
    SDL_DestroyTexture(rotTarget);
    rotTarget = NULL;
    return;
  }
  int tw = orientation & 1 ? winOutH : winOutW, th = orientation & 1 ? winOutW : winOutH;
  if (!rotTarget || tw != rotW || th != rotH) {
    SDL_DestroyTexture(rotTarget);
    rotTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, tw, th);
    rotW = tw;
    rotH = th;
  }
  if (rotTarget && SDL_SetRenderTarget(renderer, rotTarget) < 0) {
    SDL_DestroyTexture(rotTarget);
    rotTarget = NULL;
  }
}
static void orientation_set(int o) {
  if (o < 0 || o >= PD_ORIENTATIONS || o == orientation)
    return;
  orientation = o;
  if (!rotTarget) {
    rotation_ready();
    return;
  }
  // Turned already: the next present lets go of the old target and makes the new one.
}
static void host_present(void) {
  if (!rotTarget) {
    SDL_RenderPresent(renderer);
    if (orientation != PD_ORIENT_LANDSCAPE)
      rotation_ready();
    return;
  }
  int lw, lh;
  SDL_RenderGetLogicalSize(renderer, &lw, &lh);
  SDL_bool integer = SDL_RenderGetIntegerScale(renderer);
  SDL_SetRenderTarget(renderer, NULL);
  SDL_RenderSetLogicalSize(renderer, 0, 0);
  SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
  int ow, oh;
  SDL_GetRendererOutputSize(renderer, &ow, &oh);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_Rect dst = {(ow - rotW) / 2, (oh - rotH) / 2, rotW, rotH};
  SDL_RenderCopyEx(renderer, rotTarget, NULL, &dst, 90.0 * orientation, NULL, SDL_FLIP_NONE);
  SDL_RenderPresent(renderer);
  rotation_ready();
  SDL_RenderSetLogicalSize(renderer, lw, lh);
  SDL_RenderSetIntegerScale(renderer, integer);
}
/* A point on the window (0..1 across and down) in drawing pixels: the window's output pixels, turned back. */
void pd_host_point(float nx, float ny, float *px, float *py) {
  int ow = winOutW, oh = winOutH;
  if (!rotTarget && SDL_GetRendererOutputSize(renderer, &ow, &oh) < 0)
    ow = oh = 1;
  float x = nx * ow, y = ny * oh;
  if (!rotTarget) {
    *px = x;
    *py = y;
    return;
  }
  float dx = x - ow / 2.0f, dy = y - oh / 2.0f, tx, ty;
  if (orientation == PD_ORIENT_PORTRAIT)
    tx = dy, ty = -dx;
  else if (orientation == PD_ORIENT_LANDSCAPE_FLIPPED)
    tx = -dx, ty = -dy;
  else
    tx = -dy, ty = dx;
  *px = tx + rotW / 2.0f;
  *py = ty + rotH / 2.0f;
}
static void timed_present(void) {
  uint64_t t = SDL_GetPerformanceCounter();
  render_lcd();
  screenshot_now();
  host_present();
  presentMs += (double)(SDL_GetPerformanceCounter() - t) * 1000.0 / SDL_GetPerformanceFrequency();
}
/* The game grid's box art over the canvas, at the screen's own resolution (gridOverlay: while the grid is up). */
static struct game_grid *gridOverlay;
static char **gridStems; // the grid's games' art names, by item
static const char *presentShot; // present(): a screenshot of the window as drawn, then forgotten
// A dialog over the grid this frame (canvas pixels): the art goes under the dim, and the dialog is put back over it.
static bool gridModal;
static SDL_Rect gridModalRect;
static void grid_art_draw(void) {
  if (!gridOverlay || !gridStems || canvasAt.w <= 0 || canvasW <= 0)
    return;
  SDL_RenderSetLogicalSize(renderer, 0, 0); // output pixels
  double k = (double)canvasAt.w / canvasW;
  int limit = SDL_GetTicks() < messageUntil ? canvasH - 50 : canvasH; // the notice stays on top
  for (int i = 0; i < gridOverlay->artCount; i++) {
    const struct game_grid_art *a = &gridOverlay->art[i];
    if (a->index < 0 || a->index >= gridOverlay->count)
      continue;
    int tw, th;
    SDL_Texture *t = boxart_texture(renderer, gridStems[a->index], &tw, &th);
    if (!t || tw <= 0 || th <= 0)
      continue;
    // The whole cover, never cut (a GBA or DS box is about square, SteamGridDB's art 2:3): as large as fits in the
    // well, over a blurred, darkened copy of itself filling the rest.
    double wx = a->w * k, wy = a->h * k, f = wx / tw < wy / th ? wx / tw : wy / th;
    double cf = wx / tw > wy / th ? wx / tw : wy / th;
    int dw = (int)(tw * f + 0.5), dh = (int)(th * f + 0.5), cw = (int)(tw * cf + 0.5), ch = (int)(th * cf + 0.5);
    SDL_Rect dst = {canvasAt.x + (int)(a->x * k + (wx - dw) / 2), canvasAt.y + (int)(a->y * k + (wy - dh) / 2), dw, dh};
    SDL_Rect fill = {canvasAt.x + (int)(a->x * k + (wx - cw) / 2), canvasAt.y + (int)(a->y * k + (wy - ch) / 2), cw, ch};
    int top = a->y > a->clipY ? a->y : a->clipY;
    int bottom = a->y + a->h < a->clipY + a->clipH ? a->y + a->h : a->clipY + a->clipH;
    if (bottom > limit)
      bottom = limit;
    if (bottom <= top)
      continue;
    SDL_Rect clip = {canvasAt.x + (int)(a->x * k), canvasAt.y + (int)(top * k), (int)(a->w * k + 0.5),
                     (int)((bottom - top) * k + 0.5)};
    if (clip.w > wx + 0.5)
      clip.w = (int)(wx + 0.5);
    bool fits = dw >= (int)(wx + 0.5) - 1 && dh >= (int)(wy + 0.5) - 1;
    if (!fits) {
      // The blur: the cover squeezed into a few pixels, then stretched back with smoothing.
      static SDL_Texture *small;
      if (!small) {
        small = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 12, 12);
        if (small)
          SDL_SetTextureScaleMode(small, SDL_ScaleModeLinear);
      }
      if (small && SDL_SetRenderTarget(renderer, small) == 0) {
        SDL_RenderCopy(renderer, t, NULL, NULL);
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        SDL_RenderSetClipRect(renderer, &clip);
        SDL_SetTextureColorMod(small, 150, 150, 150);
        SDL_RenderCopy(renderer, small, NULL, &fill);
      }
    }
    SDL_RenderSetClipRect(renderer, &clip);
    if (gridModal)
      SDL_SetTextureColorMod(t, 64, 64, 64); // pd_dim's 0xC0
    SDL_RenderCopy(renderer, t, NULL, &dst);
    if (gridModal)
      SDL_SetTextureColorMod(t, 255, 255, 255);
  }
  SDL_RenderSetClipRect(renderer, NULL);
  if (gridModal && gridModalRect.w > 0) {
    SDL_Rect to = {canvasAt.x + (int)(gridModalRect.x * k), canvasAt.y + (int)(gridModalRect.y * k),
                   (int)(gridModalRect.w * k + 0.5), (int)(gridModalRect.h * k + 0.5)};
    SDL_RenderCopy(renderer, texture, &gridModalRect, &to);
  }
}
static void present(void) {
  SDL_RenderSetLogicalSize(renderer, canvasW, canvasH);
  SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
  {
    int ow, oh;
    SDL_GetRendererOutputSize(renderer, &ow, &oh);
    int scale = ow / canvasW < oh / canvasH ? ow / canvasW : oh / canvasH;
    if (scale < 1)
      scale = 1;
    canvas_at((SDL_Rect){(ow - canvasW * scale) / 2, (oh - canvasH * scale) / 2, canvasW * scale, canvasH * scale},
              canvasW, canvasH);
  }
  if (SDL_GetTicks() < messageUntil) {
    pd_fill(&canvas, 4, canvasH - 50, canvasW - 8, 24, 0x181818);
    pd_text_fit(&canvas, 10, canvasH - 46, canvasW - 20, message, 0xffffff, PD_NO_SHADOW);
  }
  if (SDL_UpdateTexture(texture, NULL, pixels, canvasW * 4) < 0) {
    notice(SDL_GetError());
    return;
  }
  SDL_RenderClear(renderer);
  if (SDL_RenderCopy(renderer, texture, NULL, NULL) < 0) {
    notice(SDL_GetError());
    return;
  }
  grid_art_draw();
  if (presentShot) {
    // The window as drawn, art and all (the canvas alone hasn't the art).
    if (!capture_frame(presentShot, NULL))
      notice("Capture failed");
    presentShot = NULL;
  }
  timed_present();
}
/* The settled EXPAND window for an ow x oh screen and a fw x fh game: EXPAND at the camera's closest level, ZOOM OUT
 * one step out (wide_camera.h's levels: at least 1.2 times more of the map). */
static __attribute__((unused)) void wide_window(int ow, int oh, unsigned fw, unsigned fh, int aspect, unsigned *w,
                                               unsigned *h) {
  float levels[WIDE_CAMERA_LEVELS];
  int n = wide_camera_levels(ow, oh, (int)fw, (int)fh, PK_WIDE_MAX_VIEW_W, PK_WIDE_MAX_VIEW_H, levels);
  int level = aspect == PD_ASPECT_ZOOM_OUT && n > 1 ? 1 : 0;
  wide_camera_window(ow, oh, (int)fw, (int)fh, PK_WIDE_MAX_VIEW_W, PK_WIDE_MAX_VIEW_H, levels[level], w, h);
}
/* After every frame while EXPAND can run: a frame nobody sees (fast-forward)
 * only keeps pk_wide's RAM history. wideReady = this frame shows the window:
 * the plain overworld (text boxes too with AUTO ZOOM IN off), its edges all
 * map; anything else (battles, menus, transitions) shows the game's picture. */
static void wide_tick(struct mCore *c, bool skip, const struct pd_settings *s) {
  wideReady = false;
  if (!wideSupported)
    return;
  unsigned fw = (unsigned)pk_wide_frame_w(), fh = (unsigned)pk_wide_frame_h();
  for (unsigned y = 0; y < fh; y++)
    memcpy(wideSource + y * fw, video + y * 256, fw * sizeof *wideSource);
  if (skip) {
    pk_wide_skip(c, wideSource);
    return;
  }
  if (s->aspect == PD_ASPECT_ORIGINAL) {
    // Nothing past the picture, but the scene still read (FLY's "on the field"): a one-pixel window.
    static uint32_t px;
    pk_wide_view(c, wideSource, &px, (struct PkWideView){0, 0, 1, 1}, NULL);
    return;
  }
  // The app's camera (wide_camera.h): the zoom picked on the plain overworld, 1x for battles, menus and (with AUTO
  // ZOOM IN) text boxes, eased - by the last frame's scene, the one known before this one is drawn.
  int ow = W, oh = H;
  if (SDL_GetRendererOutputSize(renderer, &ow, &oh) < 0 || ow <= 0 || oh <= 0)
    ow = W, oh = H;
  if (wideBoxW > 0 && wideBoxH > 0)
    ow = wideBoxW, oh = wideBoxH; // WINGS, BATTLE WINGS: zoom inside the game's own area
  wideLevelCount = wide_camera_levels(ow, oh, (int)fw, (int)fh, PK_WIDE_MAX_VIEW_W, PK_WIDE_MAX_VIEW_H, wideLevels);
  if (wideCamAspect != s->aspect) {
    wideCamAspect = s->aspect;
    wideCam.level = s->aspect == PD_ASPECT_ZOOM_OUT ? 1 : 0;
  }
  uint32_t now = SDL_GetTicks() | 1;
  wideScale = wide_camera_frame(&wideCam, pk_wide_scene(), s->autoZoom, wideLevels, wideLevelCount, now);
  // LOOK AROUND: the right stick of any pad (the one pushed furthest), walking = a direction held.
  float sx = 0, sy = 0;
  for (int i = 0; s->lookAround && i < padCount; i++) {
    float x = SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;
    float y = SDL_GameControllerGetAxis(pads[i], SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;
    if (x * x + y * y > sx * sx + sy * sy)
      sx = x, sy = y;
  }
  // POCKET_LOOK="x,y": a right stick held for headless looks.
  static const char *script;
  static bool scriptRead;
  if (!scriptRead)
    script = getenv("POCKET_LOOK"), scriptRead = true;
  if (script && s->lookAround && sx == 0 && sy == 0 && sscanf(script, "%f,%f", &sx, &sy) != 2)
    sx = sy = 0;
  bool walking = held & (1u << UP | 1u << DOWN | 1u << LEFT | 1u << RIGHT);
  wide_camera_look(&wideCam, sx, sy, walking, ow, oh, (int)fw, (int)fh, wideLevels[0], now);
  wide_camera_window(ow, oh, (int)fw, (int)fh, PK_WIDE_MAX_VIEW_W, PK_WIDE_MAX_VIEW_H, wideScale, &wideW, &wideH);
  struct PkWideView v = {((int)fw - (int)wideW) / 2, ((int)fh - (int)wideH) / 2,
                         (int)wideW, (int)wideH};
  wideShiftX = wideShiftY = 0;
  if (wideCam.lookX != 0 || wideCam.lookY != 0) {
    // Off the player: a game pixel more each side, so the view can sit between whole pixels and glide.
    if (wideW + 2 <= PK_WIDE_MAX_VIEW_W)
      wideW += 2;
    if (wideH + 2 <= PK_WIDE_MAX_VIEW_H)
      wideH += 2;
    float cx = (float)fw / 2 + wideCam.lookX, cy = (float)fh / 2 + wideCam.lookY;
    v = (struct PkWideView){(int)floorf(cx - (float)wideW / 2), (int)floorf(cy - (float)wideH / 2), (int)wideW,
                            (int)wideH};
    wideShiftX = (float)v.x + (float)wideW / 2 - cx;
    wideShiftY = (float)v.y + (float)wideH / 2 - cy;
  }
  // Like the app's EXPAND, the window shows whatever the scene: the map past
  // the edges on the overworld (text boxes and the START menu too), the
  // battle's own scenery, and off the map a glow from the frame's own edge
  // pixels, crossfaded - never a cut back to the bare picture.
  pk_wide_view(c, wideSource, wideFrame, v, NULL);
  wideReady = !pk_wide_fault();
}
// Gameplay scales in physical panel pixels; the companion keeps its 512x384
// grid.
static SDL_Rect sdl_rect(struct view_rect r) {
  return (SDL_Rect){r.x, r.y, r.w, r.h};
}
static int gameScaling; // plorpOS: enum pd_scaling, for ASPECT ORIGINAL's picture
static bool oledBlack;    // COLORS OLED: the panels' backdrops true black, like the screen round the game
static bool dimCompanion; // DIM COMPANION: the companion and its panels drawn darker, against OLED burn-in
static bool reduceMotion; // REDUCE MOTION: no easing HP bars or sliding cards in the wings
static int background = POCKET_BACKGROUND_THEME; // BACKGROUND in force (backdrop.h)
static int wingLook = POCKET_WING_COMFY;          // WING LOOK (backdrop.h)

static uint32_t panel_back(void) {
  struct pocket_backdrop b = pocket_backdrop_for(background);
  return b.cards ? b.card : oledBlack ? 0 : 0x101418;
}
static void dim_texture(SDL_Texture *t) {
  uint8_t v = pocket_wing_shade(background, dimCompanion, SDL_GetTicks() - wingActivity);
  SDL_SetTextureColorMod(t, v, v, v);
}
static void set_color(uint32_t c, uint8_t a) {
  SDL_SetRenderDrawColor(renderer, c >> 16, c >> 8 & 0xFF, c & 0xFF, a);
}
static const struct view_layout *gameView; // COMPANION VIEW: where the game goes (NULL = the whole screen)
static int hudRepelSteps; // the bottom strip's REPEL
static uint32_t bezelSavedAt; // the last save (the game's, or a state): the bezel bars' SAVED
static uint32_t gameSavedAt;  // the game's own last save this run, else when the game started: LAST SAVED
static bool gameSavedYet;     // the game has saved since it started
static struct pd_nuzlocke nuzlocke; // NUZLOCKE's run (nuz_load / nuz_store below)
static int bezelFf;           // fast-forward's speed while it runs, in percent, else 0
static int bezelPacePct = 100; // the game's speed off fast-forward, in percent
static uint32_t zoomChangedAt; // when the zoom last changed (ZOOM shows a few seconds after)
/* LAYOUT: the bars' item lists and the right wing's sections, from the settings (pd_settings' barList / wingList). */
static int bezelLists[4][BEZEL_LIST_MAX], bezelCount[4] = {-1, -1, -1, -1}; // walking's top / bottom, the battle's
static int wingSections[PD_WING_LIST_MAX], wingSectionCount = -1;
static int battleSections[PD_WING_LIST_MAX], battleSectionCount = -1;
/* Each bar's items this frame: their texts and which item each is (barN 0 and barListMode false: the battle's bar). */
static char barText[2][BEZEL_LIST_MAX][64];
static int barItem[2][BEZEL_LIST_MAX], barN[2];
static bool barListMode[2];
static bool topDrawn[BEZEL_ITEMS]; // what the top bar drew last: the bottom never says it again
/* A bar's list from the settings (B: 0 / 1 walking's top / bottom, 2 / 3 the battle's): its own, else walking's from
 * the old three places, the battle's top as walking's and its bottom the foe's. */
static int bar_list_of(const struct pd_settings *st, int b, int *out) {
  if (st->barCount[b] >= 0) {
    int n = 0;
    for (int i = 0; i < st->barCount[b] && i < PD_BAR_LIST_MAX; i++)
      if (st->barList[b][i] > BEZEL_NONE && st->barList[b][i] < BEZEL_ITEMS)
        out[n++] = st->barList[b][i];
    return n;
  }
  if (b == 2)
    return bar_list_of(st, 0, out);
  if (b == 3) {
    memcpy(out, BEZEL_DEFAULT_BATTLE_BOTTOM, sizeof BEZEL_DEFAULT_BATTLE_BOTTOM);
    return (int)(sizeof BEZEL_DEFAULT_BATTLE_BOTTOM / sizeof *BEZEL_DEFAULT_BATTLE_BOTTOM);
  }
  return bezel_list_from_slots(&st->bezelSlots[b ? BEZEL_BOTTOM_LEFT : BEZEL_TOP_LEFT], b == 0, out);
}
static int battle_list_of(const struct pd_settings *st, int *out) {
  if (st->battleWingCount < 0) {
    memcpy(out, pd_battle_default, (size_t)pd_battle_default_count * sizeof *out);
    return pd_battle_default_count;
  }
  int n = 0;
  for (int i = 0; i < st->battleWingCount && i < PD_WING_LIST_MAX; i++)
    if (st->battleWingList[i] >= 0 && st->battleWingList[i] < PD_BATTLE_SECTIONS)
      out[n++] = st->battleWingList[i];
  return n;
}
static int wing_list_of(const struct pd_settings *st, int *out) {
  if (st->wingCount < 0) {
    memcpy(out, pd_wing_default, (size_t)pd_wing_default_count * sizeof *out);
    return pd_wing_default_count;
  }
  int n = 0;
  for (int i = 0; i < st->wingCount && i < PD_WING_LIST_MAX; i++)
    if (st->wingList[i] >= 0 && st->wingList[i] < PD_WING_SECTIONS)
      out[n++] = st->wingList[i];
  return n;
}
static int bezelStyle = BEZEL_STYLE_THEME, bezelSlots[BEZEL_SLOTS];
static SDL_Texture *bezelGlow; // GLOW: the game's frame, tiny, stretched back up (a blur for free)
static SDL_Texture *bezelGlowSource;
static SDL_Rect bezelGlowSourceRect;
static bool render_game(unsigned gw, unsigned gh) {
  canvasAt.w = 0; // only the game: nothing to tap until a canvas goes over it
  int ow, oh;
  if (SDL_GetRendererOutputSize(renderer, &ow, &oh) < 0 || ow <= 0 || oh <= 0)
    return false;
  SDL_RenderSetLogicalSize(renderer, ow, oh);
  SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
  int scale = ow / (int)gw;
  if (oh / (int)gh < scale)
    scale = oh / (int)gh;
  if (scale < 1)
    scale = 1;
  lastGameRect =
      (SDL_Rect){(ow - (int)gw * scale) / 2, (oh - (int)gh * scale) / 2,
                 (int)gw * scale, (int)gh * scale};
  if (gameScaling == PD_SCALE_STRETCH)
    lastGameRect = (SDL_Rect){0, 0, ow, oh};
  else if (gameScaling == PD_SCALE_FIT) {
    int w = ow, h = (int)((long)ow * gh / gw);
    if (h > oh) {
      h = oh;
      w = (int)((long)oh * gw / gh);
    }
    lastGameRect = (SDL_Rect){(ow - w) / 2, (oh - h) / 2, w, h};
  }
  // Beside panels (BATTLE WINGS, WINGS, SIDE PANEL locked, STEP BACK): the game's own picture at the view's scale.
  bool placed = gameView && !gameView->expand && gameView->game.w > 0;
  if (placed)
    lastGameRect = sdl_rect(gameView->game);
  wideBoxW = placed ? gameView->game.w : 0;
  wideBoxH = placed ? gameView->game.h : 0;
  // The textures take mGBA's pixels as they are (XBGR: SDL's BGR888).
  if (SDL_UpdateTexture(gameTexture, &(SDL_Rect){0, 0, (int)gw, (int)gh}, video, 256 * 4) < 0) {
    notice(SDL_GetError());
    return false;
  }
  SDL_Texture *drawn = gameTexture;
  if (wideReady) {
    // The same scale: the window's middle lands on the game's own picture.
    gw = wideW;
    gh = wideH;
    // The camera's scale (whole once it settles; between two while it eases), the window centred on the screen.
    float fs = wideScale >= 1 ? wideScale : 1;
    int dw = (int)lroundf((float)gw * fs), dh = (int)lroundf((float)gh * fs);
    lastGameRect = placed ? (SDL_Rect){gameView->game.x + (gameView->game.w - dw) / 2,
                                       gameView->game.y + (gameView->game.h - dh) / 2, dw, dh}
                          : (SDL_Rect){(ow - dw) / 2, (oh - dh) / 2, dw, dh};
    lastGameRect.x += (int)lroundf(wideShiftX * fs);
    lastGameRect.y += (int)lroundf(wideShiftY * fs);
    if (SDL_UpdateTexture(wideTexture, &(SDL_Rect){0, 0, (int)gw, (int)gh},
                          wideFrame, (int)gw * 4) < 0) {
      notice(SDL_GetError());
      return false;
    }
    drawn = wideTexture;
  }
  set_color(pocket_backdrop_for(background).outside, 255);
  SDL_RenderClear(renderer);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  if (pocket_shader_active()) {
    // SHADER / COLORS: a filtered copy of the same pixels (shader.cpp), drawn into the same rectangle.
    bool wide = drawn == wideTexture;
    bezelGlowSource = drawn;
    bezelGlowSourceRect = (SDL_Rect){0, 0, (int)gw, (int)gh};
    if (!pocket_shader_draw(renderer, wide ? wideFrame : video, wide ? (int)gw : 256, (int)gw, (int)gh, false,
                            &lastGameRect)) {
      notice(SDL_GetError());
      return false;
    }
    render_game_filter(&lastGameRect, (int)gw, (int)gh);
    return true;
  }
  SDL_Rect src = {0, 0, (int)gw, (int)gh};
  bezelGlowSource = drawn;
  bezelGlowSourceRect = src;
  if (SDL_RenderCopy(renderer, drawn, &src, &lastGameRect) < 0) {
    notice(SDL_GetError());
    return false;
  }
  render_game_filter(&lastGameRect, (int)gw, (int)gh);
  return true;
}
/* WILD's badge: this map's wild species not caught yet (FieldHud's hudWild); -1 = the game has no wild tables. */
static int hud_wild_uncaught(const struct pd_game *g, const struct pd_snapshot *s) {
  struct pd_wild_set w;
  if (!g->cfg || !g->cfg->wildHeaders)
    return -1;
  if (!pd_wild_here(g, s, &w))
    return 0;
  int seen[PD_WILD_METHODS * PD_WILD_MAX], n = 0, left = 0;
  for (int m = 0; m < PD_WILD_METHODS; m++)
    for (int i = 0; i < w.count[m]; i++) {
      int sp = w.mons[m][i].species, k = 0;
      while (k < n && seen[k] != sp)
        k++;
      if (k < n)
        continue;
      seen[n++] = sp;
      left += !pd_dex_caught(s, pd_dex_national(g, sp));
    }
  return left;
}
/* The HUD over the overworld (hud.c): the screen with EXPAND (its corners), else the game's own picture, at the
 * companion's canvas scale. Its texture is redrawn only when what it shows changed (hud_key). */
static uint32_t hudPixels[CANVAS_MAX_W * CANVAS_MAX_H];
static SDL_Texture *hudTexture;
static uint32_t hudDrawnKey;
static int hudW, hudH;
static bool hudShown;
static struct hud_hits hudHits; // where hud_draw put each target, in HUD canvas pixels
static SDL_Rect hudAt;          // where the HUD was last drawn, in the renderer's pixels (w 0: not shown)
static int hudAtScale = 1;      // ... at this many pixels per HUD canvas pixel
/* The HUD target under a point in the renderer's pixels, or -1. */
static int hud_target_at(float x, float y) {
  if (hudAt.w <= 0 || x < hudAt.x || y < hudAt.y || x >= hudAt.x + hudAt.w || y >= hudAt.y + hudAt.h)
    return -1;
  int cx = (int)((x - hudAt.x) / hudAtScale), cy = (int)((y - hudAt.y) / hudAtScale);
  for (int i = 0; i < hudHits.n; i++)
    if (cx >= hudHits.at[i].x && cy >= hudHits.at[i].y && cx < hudHits.at[i].x + hudHits.at[i].w &&
        cy < hudHits.at[i].y + hudHits.at[i].h)
      return hudHits.at[i].id;
  return -1;
}
static int hudScale = 1;
static void present_hud_game(const struct pd_game *g, const struct pd_snapshot *s, const struct hud_state *h,
                             const struct pd_settings *st, unsigned gw, unsigned gh) {
  if (!render_game(gw, gh))
    return;
  int ow, oh;
  SDL_GetRendererOutputSize(renderer, &ow, &oh);
  int scale = ow / canvasW < oh / canvasH ? ow / canvasW : oh / canvasH;
  // SIZE: the companion's scale a whole pixel smaller or bigger (FieldHudSize's step).
  scale += st->hudSize - PD_HUD_NORMAL;
  if (scale < 1)
    scale = 1;
  hudScale = scale;
  SDL_Rect area = wideReady && !wideBoxW ? (SDL_Rect){0, 0, ow, oh} : lastGameRect;
  int w = area.w / scale, hh = area.h / scale;
  if (w > CANVAS_MAX_W)
    w = CANVAS_MAX_W;
  if (hh > CANVAS_MAX_H)
    hh = CANVAS_MAX_H;
  if (!hudTexture) {
    hudTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, CANVAS_MAX_W,
                                   CANVAS_MAX_H);
    if (hudTexture)
      SDL_SetTextureBlendMode(hudTexture, SDL_BLENDMODE_BLEND);
  }
  uint32_t key = hud_key(g, s, h, st) ^ (uint32_t)(w * 65599 + hh);
  if (hudTexture && (key != hudDrawnKey || w != hudW || hh != hudH)) {
    struct pd_canvas hc;
    pd_canvas_init(&hc, w, hh, hudPixels);
    hudShown = hud_draw(&hc, g, s, h, st, &hudHits);
    quick_finish(&hc);
    SDL_UpdateTexture(hudTexture, &(SDL_Rect){0, 0, w, hh}, hudPixels, w * 4);
    hudDrawnKey = key;
    hudW = w;
    hudH = hh;
  }
  hudAt.w = 0;
  if (hudTexture && hudShown) {
    SDL_Rect at = {area.x + (area.w - w * scale) / 2, area.y + (area.h - hh * scale) / 2, w * scale, hh * scale};
    SDL_RenderCopy(renderer, hudTexture, &(SDL_Rect){0, 0, w, hh}, &at);
    hudAt = at;
    hudAtScale = scale;
  }
  timed_present();
}
/* The game menu (resume, states, quit): an OPTION window over the game's
 * picture, EXPAND's edges and all, dimmed behind it - the companion's canvas
 * scale, its windows only (pd_menu.overlay), drawn into a texture whose
 * untouched pixels stay see-through. */
#define MENU_KEY 0x00FF00FFu
static uint32_t menuPixels[CANVAS_MAX_W * CANVAS_MAX_H];
static SDL_Texture *menuTexture;
static bool overlay_texture(void) {
  if (!menuTexture) {
    menuTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                    CANVAS_MAX_W, CANVAS_MAX_H);
    if (!menuTexture)
      return false;
    SDL_SetTextureBlendMode(menuTexture, SDL_BLENDMODE_BLEND);
  }
  return true;
}
/* The QUICK MENU over the running game: a light dim, the ring at the companion's canvas scale. */
static void present_quick(const struct pd_game *g, const struct quick_item *items, int n, int sel,
                          const char *title, const char *const *rows, int rowCount, int rowSel, unsigned gw,
                          unsigned gh) {
  if (!render_game(gw, gh) || !overlay_texture())
    return;
  int ow, oh;
  SDL_GetRendererOutputSize(renderer, &ow, &oh);
  int scale = ow / canvasW < oh / canvasH ? ow / canvasW : oh / canvasH;
  if (scale < 1)
    scale = 1;
  struct pd_canvas qc;
  pd_canvas_init(&qc, canvasW, canvasH, menuPixels);
  if (rows)
    quick_list_draw(&qc, title, rows, rowCount, rowSel);
  else if (!quick_draw(&qc, g, items, n, sel))
    return;
  quick_finish(&qc);
  SDL_UpdateTexture(menuTexture, &(SDL_Rect){0, 0, canvasW, canvasH}, menuPixels, canvasW * 4);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0x55);
  SDL_RenderFillRect(renderer, NULL);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_Rect at = {(ow - canvasW * scale) / 2, (oh - canvasH * scale) / 2, canvasW * scale, canvasH * scale};
  SDL_RenderCopy(renderer, menuTexture, &(SDL_Rect){0, 0, canvasW, canvasH}, &at);
  timed_present();
}
static void present_menu(struct pd_menu *m, unsigned gw, unsigned gh) {
  if (!render_game(gw, gh))
    return;
  int ow, oh;
  SDL_GetRendererOutputSize(renderer, &ow, &oh);
  int scale = ow / canvasW < oh / canvasH ? ow / canvasW : oh / canvasH;
  if (scale < 1)
    scale = 1;
  int w = canvasW * 3 / 4 < 300 ? (canvasW < 300 ? canvasW : 300) : canvasW * 3 / 4;
  int h = 24 + 4 + m->count * 18 + 2 * 7 + 8 + 2 * MARGIN;
  if (h > canvasH)
    h = canvasH;
  if (!overlay_texture())
    return;
  struct pd_canvas mc;
  pd_canvas_init(&mc, w, h, menuPixels);
  for (int i = 0; i < w * h; i++)
    menuPixels[i] = MENU_KEY;
  m->overlay = true;
  pd_menu_draw(m, &mc);
  for (int i = 0; i < w * h; i++)
    menuPixels[i] = menuPixels[i] == MENU_KEY ? 0 : (menuPixels[i] | 0xFF000000u);
  SDL_UpdateTexture(menuTexture, &(SDL_Rect){0, 0, w, h}, menuPixels, w * 4);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
  SDL_RenderFillRect(renderer, NULL);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_Rect at = {(ow - w * scale) / 2, (oh - h * scale) / 2, w * scale, h * scale};
  SDL_RenderCopy(renderer, menuTexture, &(SDL_Rect){0, 0, w, h}, &at);
  canvas_at(at, w, h);
  timed_present();
}
/* COMPANION VIEW (SETTINGS, settings.ini's view=): where the game, the companion and the side panels sit, from
 * view_layout.c, eased over VIEW_ANIMATION_MS whenever the layout changes (a battle starting, the companion
 * opening, the window resizing). render_game draws the game where viewShown puts it. */
static struct view_layout viewShown, viewFrom, viewTo;
static uint32_t viewSince;
static bool viewReady;
static void view_update(const struct pd_settings *st, bool companionOpen, bool battle, unsigned fw, unsigned fh) {
  int ow = W, oh = H;
  if (SDL_GetRendererOutputSize(renderer, &ow, &oh) < 0 || ow <= 0 || oh <= 0)
    ow = W, oh = H;
  struct view_state vs = {ow, oh, (int)fw, (int)fh, companionOpen, battle, st->panelLocked, st->wingRoom, st->stretch};
  struct view_layout to;
  view_layout(st->companionView, &vs, &to);
  uint32_t now = SDL_GetTicks();
  if (!viewReady) {
    viewShown = viewFrom = viewTo = to;
    viewSince = now - VIEW_ANIMATION_MS;
    viewReady = true;
  } else if (memcmp(&to, &viewTo, sizeof to)) {
    viewFrom = viewShown;
    // A panel or drawer slides in from its edge instead of growing from its middle.
    if (to.companionSlides && to.companion.w && !viewTo.companion.w) {
      viewFrom.companion = to.companion;
      if (to.companion.h >= oh)
        viewFrom.companion.x = ow;
      else
        viewFrom.companion.y = oh;
    }
    viewTo = to;
    viewSince = now;
  }
  int t = (int)((now - viewSince) * 1000 / VIEW_ANIMATION_MS);
  view_layout_lerp(&viewFrom, &viewTo, t > 1000 ? 1000 : t, &viewShown);
}
/* Where the open companion goes: its own rectangle, or STEP BACK's right wing. */
static struct view_rect view_companion(const struct view_layout *v) {
  return v->companion.w ? v->companion : v->right;
}
/* The side panels (party, battle or map, the strips): each a small canvas of its own at the largest whole scale
 * that keeps a panel 160 pixels wide (a strip 24 tall), never past the companion's, redrawn only when the
 * companion's data or the panel's size changed. */
enum { PANEL_LEFT, PANEL_RIGHT, PANEL_TOP, PANEL_BOTTOM, PANELS };
static uint32_t panelPixels[CANVAS_MAX_W * PANEL_MAX_H];
static SDL_Texture *panelTexture[PANELS];
static uint32_t panelKey[PANELS], panelGen;
/* The right wing as last drawn: the map panel (its SAVE STATE / CLOSE GAME buttons live), where its canvas went on the
 * screen and the canvas's size. */
static bool rightWingMap;
static bool rightTextureMap; // the right wing's texture is the map's (with SAVE STATE and CLOSE GAME), not a battle's
static SDL_Rect rightWingAt;
static int rightWingW, rightWingH;
static int panelW[PANELS], panelH[PANELS];
static uint32_t barFacts[2]; // PD_BAR_* the top / bottom bar showed last draw: the right wing leaves them out
static int bezelRepelNow;    // the repel's steps as the bars last read them
static struct wing_toasts wingToasts; // the wings' toasts (a level up, a new area, a save, the spray worn off)
#define BAR_FADE_MS 240u              // a bar's old words fading into its new ones
static void panel_text(const struct pd_game *g, const struct pd_snapshot *s, int which, bool battle, char *a,
                       char *b, char *c, size_t n) {
  a[0] = b[0] = c[0] = 0;
  const struct pd_battle_mon *foe = &s->battlers[PD_POS_OPPONENT_LEFT], *mine = &s->battlers[PD_POS_PLAYER_LEFT];
  (void)n; // the bars' items are lists now (a, b, c stay empty)
  static char foeText[64], matchText[64], catchText[64];
  foeText[0] = matchText[0] = catchText[0] = 0;
  if (battle && s->valid && foe->species > 0) {
    // The battle's items: the foe, what the moves out do to it, a wild one's catch chance.
    char name[40];
    snprintf(foeText, sizeof foeText, "FOE %s Lv%d", pd_species_name(g, foe->species, name, sizeof name), foe->level);
    int best = -1;
    for (int i = 0; i < PD_NUM_MOVES && mine->species > 0; i++) {
      if (!mine->moves[i])
        continue;
      int t = pd_move_type(g, mine->moves[i]);
      if (t < 0)
        continue;
      int pct = pd_type_multiplier_pct(g, t, foe->type1, foe->type2 != foe->type1 ? foe->type2 : -1);
      if (pct > best)
        best = pct;
    }
    snprintf(matchText, sizeof matchText, "%s", bezel_matchup(best));
    if (s->foeCount <= 0) {
      const char *st = pd_status_label(foe->status);
      bool deep = !strcmp(st, "SLP") || !strcmp(st, "FRZ");
      snprintf(catchText, sizeof catchText, "%s", bezel_catch_hint(foe->hp, foe->maxHp, deep, *st && !deep));
    }
  }
  struct bezel_facts f = {.place = NULL, .hour = -1, .battery = -1, .badges = -1, .playHours = -1, .money = -1,
                          .leadToNext = -1, .savedMinutes = -1, .dexCaught = -1, .dexSeen = -1, .nuzDeaths = -1,
                          .nuzArea = -1};
  // The guide's facts (the next boss, this area's wild kinds) are worked out a few times a second, not every frame.
  static char bossName[48];
  static int bossCap, caughtHere, kindsHere;
  static uint32_t guideAt;
  static const struct pd_game *guideGame;
  if (s->valid && (guideGame != g || SDL_GetTicks() - guideAt > 500)) {
    guideGame = g;
    guideAt = SDL_GetTicks();
    bossCap = pd_ui_next_boss(g, s, bossName, sizeof bossName);
    if (!pd_ui_caught_here(g, s, &caughtHere, &kindsHere))
      caughtHere = kindsHere = 0;
  }
  if (s->valid) {
    f.boss = bossCap ? bossName : NULL;
    f.cap = bossCap;
    f.caughtHere = caughtHere;
    f.kindsHere = kindsHere;
    for (int i = 0; i < s->partyCount && i < PD_PARTY_SIZE; i++) {
      const struct pd_mon *m = &s->party[i];
      if (m->egg || m->species <= 0 || m->maxHp <= 0)
        continue;
      static char leadName[40];
      if (m->nickname[0])
        snprintf(leadName, sizeof leadName, "%s", m->nickname);
      else
        pd_species_name(g, m->species, leadName, sizeof leadName);
      f.lead = leadName;
      f.leadHp = m->hp;
      f.leadMaxHp = m->maxHp;
      struct pd_exp exp;
      bool gb = g->cfg && g->cfg->gb;
      if (m->hasExperience && !gb && !g->hackMaps &&
          pd_retail_exp_progress(m->species, m->level, m->experience, 100, &exp))
        f.leadToNext = exp.toNext;
      break;
    }
    f.place = pd_mapsec_name(g, s->mapsec);
    if (g->cfg && g->cfg->badgeFlag > 0) {
      f.badges = 0;
      for (int i = 0; i < 8; i++)
        f.badges += pd_flag(s, g->cfg->badgeFlag + i);
    }
    f.playHours = s->playHours;
    f.playMinutes = s->playMinutes;
    f.money = s->money;
    if (s->dexOk && s->dexCount > 0) {
      f.dexCaught = f.dexSeen = 0;
      for (int i = 0; i < s->dexCount && i / 8 < PD_DEX_BYTES; i++) {
        f.dexSeen += s->dexSeen[i / 8] >> (i % 8) & 1;
        f.dexCaught += s->dexCaught[i / 8] >> (i % 8) & 1;
      }
    }
    if (nuzlocke.on && nuzlocke.supported) {
      f.nuzDeaths = pd_nuz_dead(&nuzlocke.run);
      f.nuzArea = s->posOk ? pd_nuz_encounter_at(&nuzlocke.run, s->mapsec) != NULL : -1;
    }
  }
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  if (tm) {
    f.hour = tm->tm_hour;
    f.minute = tm->tm_min;
  }
  int secs, pct = -1;
  SDL_PowerState power = SDL_GetPowerInfo(&secs, &pct);
  if (power == SDL_POWERSTATE_ON_BATTERY && pct >= 0) { // plugged in: no battery to watch
    f.battery = pct;
    f.charging = power == SDL_POWERSTATE_CHARGING || power == SDL_POWERSTATE_CHARGED;
  }
  uint32_t t = SDL_GetTicks();
  f.hints = lastInputAt && t - lastInputAt < BEZEL_HINT_MS;
  f.companion = f.quickMenu = true;
  f.ff = bezelFf;
  f.zoomPct = wideReady && wideCam.level > 0 ? zoom_percent(wideCam.level) : 0;
  f.saved = bezelSavedAt && t - bezelSavedAt < BEZEL_SAVED_MS;
  f.zoomRecent = zoomChangedAt && t - zoomChangedAt < BEZEL_ZOOM_MS;
  f.pacePct = bezelPacePct;
  f.repel = hudRepelSteps;
  if (gameSavedAt) {
    // Never saved this run: only the warning, once it's been a while.
    int mins = (int)((t - gameSavedAt) / 60000u);
    f.savedMinutes = gameSavedYet || mins >= BEZEL_SAVE_LATE_MIN ? mins : -1;
  }
  // LAYOUT's list: every item with something to say, in order (what the top bar drew left out of the bottom's); how
  // many fit is worked out where the bar is drawn (bar_list_draw).
  int bar = which == PANEL_TOP ? 0 : 1, list = bar + (battle ? 2 : 0);
  f.foe = foeText;
  f.matchup = matchText;
  f.catchHint = catchText;
  barListMode[bar] = true;
  bezelRepelNow = f.repel;
  barN[bar] = bezelCount[list] > 0 ? bezel_list_texts(bezelLists[list], bezelCount[list], bar ? topDrawn : NULL, &f,
                                                      barText[bar], barItem[bar], BEZEL_LIST_MAX)
                                   : 0;
}
/* A bar's items drawn across it: as many as fit (bezel_fit: the place shrinks, then the least kept go), the first at
 * the left, the last at the right and the rest spread evenly between. CARD: on the theme's window, else off-white text
 * on BACK. What it drew goes to barFacts (and topDrawn for the top's). */
static void bar_list_draw(struct pd_canvas *pc, int bar, bool card, uint32_t back) {
  int x0 = 6, r = pc->w - 6;
  uint32_t ink = 0xE8E6DE, shadow = 0x000000;
  if (card) {
    int fr = pd_title_box(pc, 0, 0, pc->w, pc->h, PD_TITLE_FILL);
    x0 = fr + 6;
    r = pc->w - fr - 6;
    ink = PD_LABEL;
    shadow = PD_LABEL_SHADOW;
  } else
    pd_fill(pc, 0, 0, pc->w, pc->h, back);
  int y = pd_text_y(0, pc->h), n = barN[bar], wd[BEZEL_LIST_MAX], place = -1;
  bool keep[BEZEL_LIST_MAX];
  for (int i = 0; i < n; i++) {
    wd[i] = pd_text_width(barText[bar][i]);
    if (barItem[bar][i] == BEZEL_LOCATION)
      place = i;
  }
  enum { GAP = 14 };
  int placeW = bezel_fit(wd, barItem[bar], n, place, pd_text_width("ROUTE 1234"), r - x0, GAP, keep);
  int total = 0, shown = 0;
  for (int i = 0; i < n; i++)
    if (keep[i]) {
      total += i == place ? placeW : wd[i];
      shown++;
    }
  int step = shown > 1 ? (r - x0 - total) / (shown - 1) : 0;
  bool drawn[BEZEL_ITEMS] = {false};
  for (int i = 0, x = x0; i < n; i++) {
    if (!keep[i])
      continue;
    int iw = i == place ? placeW : wd[i];
    if (i == place)
      pd_text_fit(pc, x, y, placeW, barText[bar][i], ink, shadow);
    else
      pd_text(pc, x, y, barText[bar][i], ink, shadow);
    drawn[barItem[bar][i]] = true;
    x += iw + step;
  }
  if (bar == 0)
    memcpy(topDrawn, drawn, sizeof drawn);
  uint32_t facts = 0;
  if (drawn[BEZEL_MONEY] || (drawn[BEZEL_REPEL_MONEY] && bezelRepelNow <= 0))
    facts |= PD_BAR_MONEY;
  if (drawn[BEZEL_BADGES_TIME] || drawn[BEZEL_BADGES])
    facts |= PD_BAR_BADGES;
  if (drawn[BEZEL_BADGES_TIME] || drawn[BEZEL_PLAY_TIME])
    facts |= PD_BAR_TIME;
  if (drawn[BEZEL_NEXT_BOSS] || drawn[BEZEL_BOSS])
    facts |= PD_BAR_BOSS;
  if (drawn[BEZEL_CAUGHT_HERE] || (drawn[BEZEL_AREA] && !(nuzlocke.on && nuzlocke.supported)))
    facts |= PD_BAR_HERE;
  if (drawn[BEZEL_CAUGHT_SEEN])
    facts |= PD_BAR_DEX;
  barFacts[bar] = facts;
}
/* BLACK / GLOW's bar: off-white text on the bar (near-black, or the game's glow), the left and right ends at the
 * bar's edges, the middle only where it clears both. */
static void bezel_strip(struct pd_canvas *pc, const char *a, const char *b, const char *c, uint32_t back) {
  pd_fill(pc, 0, 0, pc->w, pc->h, back);
  const uint32_t ink = 0xE8E6DE, muted = 0xA8A69E, shadow = 0x000000;
  int y = pd_text_y(0, pc->h), x = 6, r = pc->w - 6;
  int lw = *a ? pd_text_width(a) : 0, rw = *c ? pd_text_width(c) : 0;
  if (lw)
    pd_text_fit(pc, x, y, r - x, a, ink, shadow);
  if (rw && x + lw + 12 + rw <= r)
    pd_text_right(pc, r, y, c, ink, shadow);
  else
    rw = 0;
  if (*b) {
    int mw = pd_text_width(b), mx = (pc->w - mw) / 2;
    if (mx >= x + lw + 12 && mx + mw + 12 <= r - rw)
      pd_text(pc, mx, y, b, ink, shadow);
  }
  (void)muted;
}
/* The game's frame squeezed to 16 x 12: a blur of its colours for free (GLOW's bars, BLUR's wings). False with no
 * frame yet. */
static bool game_glow_ready(void) {
  if (!bezelGlowSource)
    return false;
  if (!bezelGlow) {
    bezelGlow = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 16, 12);
    if (!bezelGlow)
      return false;
    SDL_SetTextureScaleMode(bezelGlow, SDL_ScaleModeLinear);
  }
  SDL_Texture *was = SDL_GetRenderTarget(renderer);
  if (SDL_SetRenderTarget(renderer, bezelGlow) != 0) {
    SDL_SetRenderTarget(renderer, was);
    return false;
  }
  SDL_ScaleMode mode;
  SDL_GetTextureScaleMode(bezelGlowSource, &mode);
  SDL_SetTextureScaleMode(bezelGlowSource, SDL_ScaleModeLinear);
  SDL_RenderCopy(renderer, bezelGlowSource, &bezelGlowSourceRect, NULL);
  SDL_SetTextureScaleMode(bezelGlowSource, mode);
  SDL_SetRenderTarget(renderer, was);
  return true;
}
/* BLUR's wing room: the game's side nearest it (side 0 left, 1 right), blurred, dark and faint. */
static void draw_game_blur(SDL_Rect room, int side) {
  if (!game_glow_ready())
    return;
  SDL_Rect from = {side ? 8 : 0, 0, 8, 12};
  SDL_SetTextureBlendMode(bezelGlow, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(bezelGlow, 200);
  SDL_SetTextureColorMod(bezelGlow, 170, 170, 170);
  SDL_RenderCopy(renderer, bezelGlow, &from, &room);
  SDL_SetTextureColorMod(bezelGlow, 255, 255, 255);
  SDL_SetTextureAlphaMod(bezelGlow, 255);
}
/* BATTLE INFO off: the right wing keeps its map in a battle rather than the battle's info. */
static bool wingBattleInfo = true;
static void draw_panel(int which, struct view_rect r, const struct pd_game *g, const struct pd_snapshot *s,
                       bool battle) {
  if (r.w < 8 || r.h < 8)
    return;
  int ow, oh;
  SDL_GetRendererOutputSize(renderer, &ow, &oh);
  int scale = ow / canvasW < oh / canvasH ? ow / canvasW : oh / canvasH;
  int fit = which == PANEL_TOP || which == PANEL_BOTTOM ? r.h / 18 : r.w / 112;
  if (fit < scale)
    scale = fit;
  if (scale < 1)
    scale = 1;
  if (which == PANEL_LEFT || which == PANEL_RIGHT)
    scale = pocket_wing_look_scale(scale, wingLook, r.w, r.h, CANVAS_MAX_W, PANEL_MAX_H);
  // Never a canvas taller (or wider) than its texture: the card would stop short of the wing's height.
  while (r.h / scale > PANEL_MAX_H || r.w / scale > CANVAS_MAX_W)
    scale++;
  // BLACK / DARK: the wing is a card inset in its room, on the background, its content moved a step now and then.
  struct pocket_backdrop look = pocket_backdrop_for(background);
  struct view_rect room = r;
  int dx = 0, dy = 0;
  if (look.cards) {
    int pad = (which == PANEL_TOP || which == PANEL_BOTTOM ? r.h : r.w) / (look.blur ? 12 : 40);
    if (pad < 3)
      pad = 3;
    pocket_wing_shift(background, SDL_GetTicks(), pad / 3, &dx, &dy);
    r = (struct view_rect){r.x + pad, r.y + pad, r.w - 2 * pad - 2, r.h - 2 * pad - 2};
    if (r.w < 8 || r.h < 8)
      return;
  }
  int w = r.w / scale, h = r.h / scale;
  if (w > CANVAS_MAX_W)
    w = CANVAS_MAX_W;
  if (h > PANEL_MAX_H)
    h = PANEL_MAX_H;
  if (!panelTexture[which]) {
    panelTexture[which] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,
                                            CANVAS_MAX_W, PANEL_MAX_H);
    if (!panelTexture[which])
      return;
  }
  char a[64], b[64], c[64];
  bool strip = which == PANEL_TOP || which == PANEL_BOTTOM;
  bool bezel = strip && bezelStyle != BEZEL_STYLE_THEME;
  if (strip)
    panel_text(g, s, which, battle, a, b, c, sizeof a);
  int bar = which == PANEL_TOP ? 0 : 1;
  bool listBar = strip && barListMode[bar];
  uint32_t textKey = 2166136261u;
  for (const char *t[3] = {a, b, c}, **p = t; p < t + 3; p++)
    for (const char *q = *p; *q; q++)
      textKey = (textKey ^ (uint8_t)*q) * 16777619u;
  for (int i = 0; listBar && i < barN[bar]; i++) {
    for (const char *q = barText[bar][i]; *q; q++)
      textKey = (textKey ^ (uint8_t)*q) * 16777619u;
    textKey = (textKey ^ 0x7Cu) * 16777619u;
  }
  uint32_t key = panelGen * 2654435761u ^ (uint32_t)(w * 65599 + h) ^ (battle ? 0x80000000u : 0) ^ (uint32_t)scale << 20 ^
                 (uint32_t)background << 28 ^ (uint32_t)bezelStyle << 24 ^
                 (strip ? textKey : pd_ui_bar_facts * 2246822519u);
  // The bars' see-through texture (GLOW: only the text is drawn over the glow).
  static SDL_Texture *bezelTexture[PANELS];
  if (bezel && !bezelTexture[which]) {
    bezelTexture[which] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                            CANVAS_MAX_W, PANEL_MAX_H);
    if (!bezelTexture[which])
      return;
    SDL_SetTextureBlendMode(bezelTexture[which], SDL_BLENDMODE_BLEND);
  }
  SDL_Texture *shown = bezel ? bezelTexture[which] : panelTexture[which];
  if (bezel) {
    // The bar fills its room edge to edge: no card, its content shifting with the wings'. Text about a third of
    // the bar tall (a font pixel two screen pixels on a 60 px bar), never past the wings' scale.
    if (room.h / 30 < scale)
      scale = room.h / 30 > 0 ? room.h / 30 : 1;
    // A step smaller while the three don't fit side by side: all three at a smaller size beat a dropped one.
    int lw = *a ? pd_text_width(a) : 0, rw = *c ? pd_text_width(c) : 0;
    int need = 2 * (lw > rw ? lw : rw) + (*b ? pd_text_width(b) : 0) + 36; // the middle sits in the centre
    if (listBar) { // a list: one step smaller at most while all of it doesn't fit (then the least kept go)
      need = 12;
      for (int i = 0; i < barN[bar]; i++)
        need += pd_text_width(barText[bar][i]) + 14;
    }
    for (int from = scale; scale > 1 && room.w / scale < need && (!listBar || scale > from - 1);)
      scale--;
    r = room;
    w = r.w / scale;
    h = r.h / scale;
    if (w > CANVAS_MAX_W)
      w = CANVAS_MAX_W;
    if (h > PANEL_MAX_H)
      h = PANEL_MAX_H;
  }
  // A bar whose words changed (not its size): the last picture is kept and faded out over the new one.
  static SDL_Texture *altTexture[2][PANELS];
  static uint32_t fadeAt[PANELS], textSeen[PANELS];
  static int fadeW[PANELS], fadeH[PANELS];
  SDL_Texture **current = bezel ? &bezelTexture[which] : &panelTexture[which];
  bool redraw = key != panelKey[which] || w != panelW[which] || h != panelH[which];
  if (strip && redraw && !reduceMotion && textSeen[which] && textSeen[which] != textKey && w == panelW[which] &&
      h == panelH[which]) {
    SDL_Texture **alt = &altTexture[bezel][which];
    if (!*alt) {
      *alt = SDL_CreateTexture(renderer, bezel ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGB888,
                               SDL_TEXTUREACCESS_STREAMING, CANVAS_MAX_W, PANEL_MAX_H);
      if (*alt)
        SDL_SetTextureBlendMode(*alt, SDL_BLENDMODE_BLEND);
    }
    if (*alt) {
      SDL_Texture *old = *current;
      *current = *alt;
      *alt = old;
      fadeAt[which] = SDL_GetTicks();
      fadeW[which] = panelW[which];
      fadeH[which] = panelH[which];
      shown = *current;
    }
  }
  if (strip)
    textSeen[which] = textKey;
  SDL_Texture *fading = NULL;
  int fadeK = 255;
  if (strip && fadeAt[which] && SDL_GetTicks() - fadeAt[which] < BAR_FADE_MS && altTexture[bezel][which]) {
    fading = altTexture[bezel][which];
    fadeK = (int)(255u * (SDL_GetTicks() - fadeAt[which]) / BAR_FADE_MS);
  }
  if (bezel && (key != panelKey[which] || w != panelW[which] || h != panelH[which])) {
    const uint32_t clear = 0x00FE01FE; // a colour no text uses: see-through
    struct pd_canvas pc;
    pd_canvas_init(&pc, w, h, panelPixels);
    if (listBar)
      bar_list_draw(&pc, bar, false, clear);
    else
      bezel_strip(&pc, a, b, c, clear);
    for (int i = 0; i < w * h; i++)
      panelPixels[i] = (panelPixels[i] & 0xFFFFFF) == clear ? 0 : 0xFF000000u | panelPixels[i];
    SDL_UpdateTexture(shown, &(SDL_Rect){0, 0, w, h}, panelPixels, w * 4);
    if (panelKey[which] != key)
      wingActivity = SDL_GetTicks();
    panelKey[which] = key;
    panelW[which] = w;
    panelH[which] = h;
  }
  if (!bezel && (key != panelKey[which] || w != panelW[which] || h != panelH[which])) {
    struct pd_canvas pc;
    pd_canvas_init(&pc, w, h, panelPixels);
    struct pocket_wing_look wl;
    pocket_wing_look_begin(background, &wl);
    pd_fill(&pc, 0, 0, w, h, panel_back());
    pd_ui_now_ms = SDL_GetTicks();
    pd_ui_reduce_motion = reduceMotion;
    pd_ui_panel_moving = false;
    if (which == PANEL_LEFT) {
      // In a battle, the card SWITCH TO names gets a green rim.
      pd_ui_switch_slot = battle && wingBattleInfo ? pd_ui_switch_pick(g, s) : -1;
      pd_ui_party_panel(&pc, g, s);
    } else if (which == PANEL_RIGHT && battle && wingBattleInfo) {
      pd_ui_battle_list = battleSectionCount >= 0 ? battleSections : NULL;
      pd_ui_battle_count = battleSectionCount;
      pd_ui_battle_panel(&pc, g, s, NULL);
    }
    else if (which == PANEL_RIGHT) {
      pd_ui_wing_buttons_on = true;
      pd_ui_repel_steps = hudRepelSteps;
      pd_ui_wing_list = wingSectionCount >= 0 ? wingSections : NULL;
      pd_ui_wing_count = wingSectionCount;
      pd_ui_wing_nuzlocke = &nuzlocke;
      pd_ui_map_panel(&pc, g, s, false);
    }
    if (which == PANEL_RIGHT)
      rightTextureMap = !(battle && wingBattleInfo);
    else if (strip) // the party's wing is its own: a bar's strip never goes over it
      pd_ui_strip(&pc, a, b, c);
    pocket_wing_look_end(&wl);
    SDL_UpdateTexture(panelTexture[which], &(SDL_Rect){0, 0, w, h}, panelPixels, w * 4);
    if (panelKey[which] != key)
      wingActivity = SDL_GetTicks(); // the wing changed: not a still one
    // Still easing (an HP bar, a card sliding in): drawn again next frame.
    panelKey[which] = pd_ui_panel_moving ? key ^ 0x5A5A5A5Au : key;
    panelW[which] = w;
    panelH[which] = h;
  }
  // Every time it's shown, not only when it's drawn again: draw_panels turns it off while the companion covers the
  // wing, and the same wing comes back from its texture as it was.
  if (which == PANEL_RIGHT && !bezel)
    rightWingMap = rightTextureMap;
  if (bezel) {
    SDL_Rect bar = {room.x, room.y, room.w, room.h};
    set_color(bezelStyle == BEZEL_STYLE_BLACK ? 0x0B0B0D : 0x000000, 255);
    SDL_RenderFillRect(renderer, &bar);
    if (bezelStyle == BEZEL_STYLE_GLOW && game_glow_ready()) {
      // The game's frame squeezed and stretched over the bar: a soft glow of its colours, dark and faint. Above the
      // game its top edge's colours, below it its bottom's: the glow comes out of the picture.
      SDL_Rect from = {0, which == PANEL_TOP ? 0 : 6, 16, 6};
      SDL_SetTextureBlendMode(bezelGlow, SDL_BLENDMODE_BLEND);
      SDL_SetTextureAlphaMod(bezelGlow, 170);
      SDL_SetTextureColorMod(bezelGlow, 170, 170, 170);
      SDL_RenderCopy(renderer, bezelGlow, &from, &bar);
    }
    SDL_Rect to = {r.x + dx + (r.w - w * scale) / 2, r.y + dy + (r.h - h * scale) / 2, w * scale, h * scale};
    if (fading) {
      dim_texture(fading);
      SDL_SetTextureAlphaMod(fading, (uint8_t)(255 - fadeK));
      SDL_RenderCopy(renderer, fading, &(SDL_Rect){0, 0, fadeW[which], fadeH[which]}, &to);
    }
    dim_texture(shown);
    SDL_SetTextureAlphaMod(shown, (uint8_t)fadeK);
    SDL_RenderCopy(renderer, shown, &(SDL_Rect){0, 0, w, h}, &to);
    SDL_SetTextureAlphaMod(shown, 255);
    return;
  }
  if (look.cards) {
    set_color(look.outside, 255);
    SDL_RenderFillRect(renderer, &(SDL_Rect){room.x, room.y, room.w, room.h});
    if (look.blur)
      draw_game_blur((SDL_Rect){room.x, room.y, room.w, room.h}, which == PANEL_LEFT ? 0 : 1);
    SDL_Rect card = {r.x + dx, r.y + dy, r.w, r.h};
    // BLUR: the card a little see-through, so the game's colours show faintly through it too.
    SDL_SetRenderDrawBlendMode(renderer, look.blur ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    set_color(look.card, look.blur ? 170 : 255);
    SDL_RenderFillRect(renderer, &card);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    set_color(look.border, look.borderAlpha);
    SDL_RenderDrawRect(renderer, &card);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  } else {
    set_color(panel_back(), 255);
    SDL_RenderFillRect(renderer, &(SDL_Rect){r.x, r.y, r.w, r.h});
  }
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_Rect to = {r.x + dx + (r.w - w * scale) / 2, r.y + dy + (r.h - h * scale) / 2, w * scale, h * scale};
  if (which == PANEL_RIGHT) {
    rightWingAt = to;
    rightWingW = w;
    rightWingH = h;
  }
  if (fading) { // the old bar under, the new one coming in over it
    dim_texture(fading);
    SDL_SetTextureAlphaMod(fading, 255);
    SDL_RenderCopy(renderer, fading, &(SDL_Rect){0, 0, fadeW[which], fadeH[which]}, &to);
    SDL_SetTextureBlendMode(panelTexture[which], SDL_BLENDMODE_BLEND);
  }
  dim_texture(panelTexture[which]);
  if (look.blur) {
    SDL_SetTextureBlendMode(panelTexture[which], SDL_BLENDMODE_BLEND);
    fadeK = fadeK * 200 / 255;
  }
  SDL_SetTextureAlphaMod(panelTexture[which], (uint8_t)fadeK);
  SDL_RenderCopy(renderer, panelTexture[which], &(SDL_Rect){0, 0, w, h}, &to);
  SDL_SetTextureAlphaMod(panelTexture[which], 255);
}
/* A wing's toast: a small card near the wing's top, faded in and out (wing_toast.h). */
static void draw_toast(struct view_rect r) {
  uint32_t now = SDL_GetTicks();
  int alpha = wing_toast_alpha(&wingToasts, now);
  if (!alpha || r.w < 60 || r.h < 60)
    return;
  int ow, oh;
  SDL_GetRendererOutputSize(renderer, &ow, &oh);
  int scale = r.w / 112;
  int most = ow / canvasW < oh / canvasH ? ow / canvasW : oh / canvasH;
  if (most > 0 && scale > most)
    scale = most;
  if (scale < 1)
    scale = 1;
  int w = r.w / scale - 12, h = 2 * 16 + 10; // two lines: the kind of news over the news
  if (w > CANVAS_MAX_W)
    w = CANVAS_MAX_W;
  if (w < 40)
    return;
  static SDL_Texture *texture;
  static uint32_t drawnKey;
  uint32_t key = wingToasts.at * 2654435761u ^ (uint32_t)w ^ (uint32_t)background << 24;
  if (!texture) {
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, CANVAS_MAX_W, 48);
    if (!texture)
      return;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  }
  if (key != drawnKey) {
    const uint32_t clear = 0x00FE01FE;
    struct pd_canvas pc;
    pd_canvas_init(&pc, w, h, panelPixels);
    struct pocket_wing_look wl;
    pocket_wing_look_begin(background, &wl);
    pd_fill(&pc, 0, 0, w, h, clear);
    pd_title_box(&pc, 0, 0, w, h, PD_TITLE_FILL);
    pd_text_fit(&pc, 7, 4, w - 14, wingToasts.head, PD_VALUE, PD_VALUE_SHADOW);
    pd_text_fit(&pc, 7, 4 + 16 + 1, w - 14, wingToasts.text, PD_LABEL, PD_LABEL_SHADOW);
    pocket_wing_look_end(&wl);
    for (int i = 0; i < w * h; i++)
      panelPixels[i] = (panelPixels[i] & 0xFFFFFF) == clear ? 0 : 0xFF000000u | panelPixels[i];
    SDL_UpdateTexture(texture, &(SDL_Rect){0, 0, w, h}, panelPixels, w * 4);
    drawnKey = key;
  }
  // In: down from a few pixels above its place as it fades in.
  uint32_t age = now - wingToasts.at;
  int drop = !reduceMotion && age < WING_TOAST_IN_MS ? (int)(6 * (WING_TOAST_IN_MS - age) / WING_TOAST_IN_MS) : 0;
  SDL_SetTextureAlphaMod(texture, (uint8_t)alpha);
  SDL_RenderCopy(renderer, texture, &(SDL_Rect){0, 0, w, h},
                 &(SDL_Rect){r.x + (r.w - w * scale) / 2, r.y + (26 - drop) * scale, w * scale, h * scale});
}
/* The wings' button hints, where no bezel bar carries them: a small card at the bottom of the left wing for the first
 * WING_HINT_MS of a game and again once the player has been idle WING_HINT_IDLE_MS. */
#define WING_HINT_MS 8000u
#define WING_HINT_IDLE_MS 20000u
#define WING_HINT_FADE_MS 500u
static uint32_t wingHintsFrom; // the game's first wing frame (0: none yet)
static void draw_hints(struct view_rect r) {
  uint32_t now = SDL_GetTicks();
  if (!wingHintsFrom)
    wingHintsFrom = now ? now : 1;
  uint32_t shown = now - wingHintsFrom, idle = now - lastInputAt;
  int alpha = 0;
  if (shown < WING_HINT_MS)
    alpha = shown > WING_HINT_MS - WING_HINT_FADE_MS ? (int)(255u * (WING_HINT_MS - shown) / WING_HINT_FADE_MS) : 255;
  if (idle > WING_HINT_IDLE_MS) {
    int a = idle - WING_HINT_IDLE_MS >= WING_HINT_FADE_MS ? 255 : (int)(255u * (idle - WING_HINT_IDLE_MS) / WING_HINT_FADE_MS);
    if (a > alpha)
      alpha = a;
  }
  if (reduceMotion && alpha)
    alpha = 255;
  if (!alpha || r.w < 60 || r.h < 60)
    return;
  int ow, oh;
  SDL_GetRendererOutputSize(renderer, &ow, &oh);
  int scale = r.w / 112;
  int most = ow / canvasW < oh / canvasH ? ow / canvasW : oh / canvasH;
  if (most > 0 && scale > most)
    scale = most;
  if (scale < 1)
    scale = 1;
  int w = r.w / scale - 12, h = 2 * 16 + 10;
  if (w > CANVAS_MAX_W)
    w = CANVAS_MAX_W;
  if (w < 40)
    return;
  static SDL_Texture *texture;
  static uint32_t drawnKey;
  uint32_t key = (uint32_t)w ^ (uint32_t)background << 24 ^ 0x9E3779B9u;
  if (!texture) {
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, CANVAS_MAX_W, 48);
    if (!texture)
      return;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  }
  if (key != drawnKey) {
    const uint32_t clear = 0x00FE01FE;
    struct pd_canvas pc;
    pd_canvas_init(&pc, w, h, panelPixels);
    struct pocket_wing_look wl;
    pocket_wing_look_begin(background, &wl);
    pd_fill(&pc, 0, 0, w, h, clear);
    pd_title_box(&pc, 0, 0, w, h, PD_TITLE_FILL);
    pd_text_fit(&pc, 7, 4, w - 14, "X COMPANION  Y QUICK MENU", PD_LABEL, PD_LABEL_SHADOW);
    pd_text_fit(&pc, 7, 4 + 16 + 1, w - 14, "R3 VIEW  CLICK A WING: ITS PAGE", PD_MUTED, PD_MUTED_SHADOW);
    pocket_wing_look_end(&wl);
    for (int i = 0; i < w * h; i++)
      panelPixels[i] = (panelPixels[i] & 0xFFFFFF) == clear ? 0 : 0xFF000000u | panelPixels[i];
    SDL_UpdateTexture(texture, &(SDL_Rect){0, 0, w, h}, panelPixels, w * 4);
    drawnKey = key;
  }
  SDL_SetTextureAlphaMod(texture, (uint8_t)alpha);
  SDL_RenderCopy(renderer, texture, &(SDL_Rect){0, 0, w, h},
                 &(SDL_Rect){r.x + (r.w - w * scale) / 2, r.y + r.h - (h + 10) * scale, w * scale, h * scale});
}
/* BLACK / DARK: a thin dark grey line round the game beside the wings, so its edge doesn't meet pure black. */
static void draw_gutter(void) {
  struct pocket_backdrop look = pocket_backdrop_for(background);
  int ow, oh;
  if (!look.cards || SDL_GetRendererOutputSize(renderer, &ow, &oh) < 0)
    return;
  SDL_Rect g = lastGameRect;
  if (g.x <= 0 && g.y <= 0 && g.w >= ow && g.h >= oh)
    return;
  int t = g.w >= 960 ? 2 : 1;
  set_color(look.gutter, 255);
  SDL_Rect sides[4] = {{g.x - t, g.y - t, g.w + 2 * t, t}, {g.x - t, g.y + g.h, g.w + 2 * t, t}, {g.x - t, g.y, t, g.h},
                       {g.x + g.w, g.y, t, g.h}};
  SDL_RenderFillRects(renderer, sides, 4);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}
static void draw_panels(const struct view_layout *v, const struct pd_game *g, const struct pd_snapshot *s,
                        bool battle, bool companionInRight) {
  if (v->left.w || v->right.w || v->top.w || v->bottom.w)
    draw_gutter();
  // The bars as last drawn: what they show, the right wing doesn't repeat.
  pd_ui_bar_facts = (v->top.w >= 8 && v->top.h >= 8 ? barFacts[0] : 0) |
                    (v->bottom.w >= 8 && v->bottom.h >= 8 ? barFacts[1] : 0);
  draw_panel(PANEL_LEFT, v->left, g, s, battle);
  if (v->right.w < 8 || companionInRight)
    rightWingMap = false;
  if (!companionInRight)
    draw_panel(PANEL_RIGHT, v->right, g, s, battle);
  draw_panel(PANEL_TOP, v->top, g, s, battle);
  draw_panel(PANEL_BOTTOM, v->bottom, g, s, battle);
  // The toast over the right wing (the left one while the companion takes the right).
  if (!companionInRight && v->right.w >= 60)
    draw_toast(v->right);
  else if (v->left.w >= 60)
    draw_toast(v->left);
  // The button hints at the bottom of the left wing, where no bar shows them.
  if (!companionInRight && v->left.w >= 60 && !(v->top.h >= 8 || v->bottom.h >= 8))
    draw_hints(v->left);
}
/* The right wing's button (PD_WING_*) a click at (x, y) (the renderer's pixels) is on, -1 for none. */
static int wing_button_at(float x, float y) {
  if (!rightWingMap || rightWingAt.w <= 0 || rightWingAt.h <= 0 || x < rightWingAt.x || y < rightWingAt.y)
    return -1;
  int cx = (int)((x - rightWingAt.x) * rightWingW / rightWingAt.w), cy = (int)((y - rightWingAt.y) * rightWingH / rightWingAt.h);
  for (int i = 0; i < PD_WING_BUTTONS; i++) {
    const struct pd_ui_rect *b = &pd_ui_wing_buttons[i];
    if (b->w > 0 && cx >= b->x && cy >= b->y && cx < b->x + b->w && cy < b->y + b->h)
      return i;
  }
  return -1;
}
/* The companion page a click at (x, y) on a wing opens, -1 off the wings. */
static int wing_tab_at(const struct view_layout *v, float x, float y, bool battle) {
  const struct view_rect *r[2] = {&v->left, &v->right};
  for (int i = 0; i < 2; i++)
    if (r[i]->w >= 8 && x >= r[i]->x && y >= r[i]->y && x < r[i]->x + r[i]->w && y < r[i]->y + r[i]->h)
      return i == 0 ? PD_TAB_PARTY : battle ? PD_TAB_BATTLE : PD_TAB_MAP;
  return -1;
}
static bool view_has_panels(const struct view_layout *v) {
  return v->left.w || v->right.w || v->top.w || v->bottom.w;
}
/* The game with its side panels, no companion open. */
static void present_view_game(const struct pd_game *g, const struct pd_snapshot *s, bool battle, unsigned gw,
                              unsigned gh) {
  if (!render_game(gw, gh))
    return;
  draw_panels(&viewShown, g, s, battle, false);
  timed_present();
}
/* The game alone (test_runtime.c draws with it). */
__attribute__((unused)) static void present_game(unsigned gw, unsigned gh) {
  update_announce();
  if (render_game(gw, gh))
    timed_present();
}
/* The game alone with room above and below it (a taller screen than the game's shape): the bezel bars there. */
#define BEZEL_MIN_ROOM 20
static void present_bars_game(const struct pd_game *g, const struct pd_snapshot *s, bool battle, unsigned gw,
                              unsigned gh) {
  update_announce();
  if (!render_game(gw, gh))
    return;
  int ow, oh;
  SDL_GetRendererOutputSize(renderer, &ow, &oh);
  int above = lastGameRect.y, below = oh - lastGameRect.y - lastGameRect.h;
  if (above >= BEZEL_MIN_ROOM && below >= BEZEL_MIN_ROOM && lastGameRect.x <= 0 + ow / 8) {
    draw_panel(PANEL_TOP, (struct view_rect){0, 0, ow, above}, g, s, battle);
    draw_panel(PANEL_BOTTOM, (struct view_rect){0, oh - below, ow, below}, g, s, battle);
  }
  timed_present();
}
/* The open companion where the view puts it: over the dimmed game (WINDOW), beside it (SIDE PANEL, STEP BACK)
 * or as a see-through drawer (GLASS DRAWER); its canvas is sized for where it lands, then drawn into the
 * rectangle the animation has reached. */
static void present_view_companion(const struct pd_game *g, const struct pd_snapshot *s, bool battle, unsigned gw,
                                   unsigned gh) {
  if (SDL_GetTicks() < messageUntil) {
    pd_fill(&canvas, 4, canvasH - 50, canvasW - 8, 24, 0x181818);
    pd_text_fit(&canvas, 10, canvasH - 46, canvasW - 20, message, 0xffffff, PD_NO_SHADOW);
  }
  if (!render_game(gw, gh))
    return;
  struct view_rect target = view_companion(&viewTo), shown = view_companion(&viewShown);
  draw_panels(&viewShown, g, s, battle, !viewShown.companion.w);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  if (viewShown.dim) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
    SDL_RenderFillRect(renderer, NULL);
  }
  if (target.w <= 0 || target.h <= 0 || shown.w <= 0 || shown.h <= 0) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    timed_present();
    return;
  }
  if (!viewShown.dim) {
    // Beside the game the companion sits on a backdrop of its own (see-through as a drawer).
    uint32_t back = panel_back();
    SDL_SetRenderDrawColor(renderer, back >> 16, back >> 8 & 0xFF, back & 0xFF, viewShown.companionGlass ? 170 : 255);
    SDL_RenderFillRect(renderer, &(SDL_Rect){shown.x, shown.y, shown.w, shown.h});
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  if (SDL_UpdateTexture(texture, NULL, pixels, canvasW * 4) < 0) {
    notice(SDL_GetError());
    return;
  }
  int scale = target.w / canvasW < target.h / canvasH ? target.w / canvasW : target.h / canvasH;
  if (scale < 1)
    scale = 1;
  long fw = (long)canvasW * scale, fh = (long)canvasH * scale;
  long fx = target.x + (target.w - fw) / 2, fy = target.y + (target.h - fh) / 2;
  SDL_Rect at = {shown.x + (int)((fx - target.x) * shown.w / target.w), shown.y + (int)((fy - target.y) * shown.h / target.h),
                 (int)(fw * shown.w / target.w), (int)(fh * shown.h / target.h)};
  if (viewShown.companionGlass) {
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(texture, 215);
  }
  dim_texture(texture);
  SDL_RenderCopy(renderer, texture, NULL, &at);
  canvas_at(at, canvasW, canvasH);
  SDL_SetTextureColorMod(texture, 255, 255, 255);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
  SDL_SetTextureAlphaMod(texture, 255);
  timed_present();
}
static bool capture_frame(const char *path, unsigned *lit) {
  int ow, oh;
  if (SDL_GetRendererOutputSize(renderer, &ow, &oh) < 0 || ow <= 0 || oh <= 0 ||
      ow > 8192 || oh > 8192)
    return false;
  uint32_t *shot = malloc((size_t)ow * oh * 4);
  if (!shot)
    return false;
  bool ok = SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGB888, shot,
                                 ow * 4) == 0;
  if (ok && lit) {
    *lit = 0;
    for (int i = 0; i < ow * oh; i++)
      if (shot[i] & 0xffffff)
        (*lit)++;
  }
  if (ok && path)
    ok = pd_png_write(path, ow, oh, shot);
  free(shot);
  return ok;
}
/* STATES: ten slots per game, slot 0 the one state file there always was (states/<rom>.ss), the others
 * states/<rom>.<n>.ss; each save leaves a 120x80 picture of the game beside it (<state>.thumb, 0xRRGGBB). */
#include <sys/stat.h>
static uint32_t slotThumbs[PD_STATE_SLOTS][PD_STATE_THUMB_W * PD_STATE_THUMB_H];
static bool slotsStale = true; // a save changed a slot: read them again before the next draw
static void slot_path(char *out, size_t n, const char *root, const char *name, int slot) {
  if (slot <= 0)
    snprintf(out, n, "%s/states/%s.ss", root, name);
  else
    snprintf(out, n, "%s/states/%s.%d.ss", root, name, slot);
}
static void slots_read(struct pd_host_info *host, const char *root, const char *name) {
  for (int i = 0; i < PD_STATE_SLOTS; i++) {
    struct pd_state_slot *out = &host->slots[i];
    char path[1100], thumb[1120];
    slot_path(path, sizeof path, root, name, i);
    struct stat st;
    out->used = !stat(path, &st);
    out->thumb = NULL;
    out->when[0] = 0;
    if (!out->used)
      continue;
    time_t when = st.st_mtime;
    struct tm *t = localtime(&when);
    if (t)
      strftime(out->when, sizeof out->when, "%m-%d %H:%M", t);
    snprintf(thumb, sizeof thumb, "%s.thumb", path);
    FILE *f = fopen(thumb, "rb");
    if (!f)
      continue;
    if (fread(slotThumbs[i], 1, sizeof slotThumbs[i], f) == sizeof slotThumbs[i])
      out->thumb = slotThumbs[i];
    fclose(f);
  }
  slotsStale = false;
}
// The game's picture at the moment of a save, shrunk to the slot's thumbnail (nearest pixel; a Game Boy's 10:9 is
// stretched to 3:2 like the card's well).
static void slot_thumb_write(struct mCore *c, const char *path) {
  unsigned w, h;
  c->desiredVideoDimensions(c, &w, &h);
  if (!w || !h || w > 256 || h > 256)
    return;
  static uint32_t px[PD_STATE_THUMB_W * PD_STATE_THUMB_H];
  for (int y = 0; y < PD_STATE_THUMB_H; y++)
    for (int x = 0; x < PD_STATE_THUMB_W; x++) {
      uint32_t v = (uint32_t)video[(y * h / PD_STATE_THUMB_H) * 256 + x * w / PD_STATE_THUMB_W]; // mGBA's 0xBBGGRR
      px[y * PD_STATE_THUMB_W + x] = (v & 0xFFu) << 16 | (v & 0xFF00u) | (v >> 16 & 0xFFu);
    }
  char thumb[1120];
  snprintf(thumb, sizeof thumb, "%s.thumb", path);
  FILE *f = fopen(thumb, "wb");
  if (!f)
    return;
  fwrite(px, 1, sizeof px, f);
  fclose(f);
}
static bool state(struct mCore *c, const char *path, bool save) {
  char tmp[1200];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  struct VFile *f = VFileOpen(save ? tmp : path,
                              save ? O_CREAT | O_TRUNC | O_RDWR : O_RDONLY);
  if (!f)
    return false;
  bool ok = save ? mCoreSaveStateNamed(c, f, SAVESTATE_RTC)
                 : mCoreLoadStateNamed(c, f, SAVESTATE_RTC);
  if(!f->close(f))ok=false;
  if (ok && save)
    bezelSavedAt = SDL_GetTicks();
  if (ok && !save) {
    ra_progress_load(path); // RetroAchievements: its progress as it was when the state was made
    pk_heal_rtc(c);
    pk_wide_reset(); // EXPAND's RAM history is from before the jump
    wideReady = false;
  }
  if (save) {
    if (ok)
      ok = pocket_replace(tmp, path);
    else
      pocket_remove(tmp);
    if (ok) {
      slot_thumb_write(c, path);
      ra_progress_save(path);
    }
    slotsStale = true;
  }
  return ok;
}
/* STATES' UNDO SAVE / UNDO LOAD, as on the 3DS and Android: a save over a slot first copies what the slot held
 * (<state>.undo and its thumbnail), and a load first keeps the game as it is (<state>.undoload) to go back to. Only the
 * last one of each can be undone; a state never touches the save file. */
static char undoSaveSlot[1200];  // the slot whose last save can be put back, "" for none
static char undoLoadState[1220]; // the game from just before the last load, "" for none
static void undo_forget(void) {
  undoSaveSlot[0] = undoLoadState[0] = 0;
}
static bool user_state(struct mCore *c, const char *path, bool save) {
  char undo[1220], thumb[1220], undoThumb[1240];
  if (save) {
    snprintf(undo, sizeof undo, "%s.undo", path);
    snprintf(thumb, sizeof thumb, "%s.thumb", path);
    snprintf(undoThumb, sizeof undoThumb, "%s.undo.thumb", path);
    bool had = pocket_exists(path) && rom_copy_file(path, undo);
    if (had && pocket_exists(thumb) && !rom_copy_file(thumb, undoThumb))
      pocket_remove(undoThumb);
    bool ok = state(c, path, true);
    if (ok)
      snprintf(undoSaveSlot, sizeof undoSaveSlot, "%s", had ? path : "");
    return ok;
  }
  snprintf(undo, sizeof undo, "%s.undoload", path);
  struct VFile *f = VFileOpen(undo, O_CREAT | O_TRUNC | O_RDWR);
  bool kept = f && mCoreSaveStateNamed(c, f, SAVESTATE_RTC);
  if (f && !f->close(f))
    kept = false;
  if (kept)
    ra_progress_save(undo);
  bool ok = state(c, path, false);
  if (ok)
    snprintf(undoLoadState, sizeof undoLoadState, "%s", kept ? undo : "");
  return ok;
}
static bool undo_save(void) {
  if (!undoSaveSlot[0])
    return false;
  char undo[1220], thumb[1220], undoThumb[1240];
  snprintf(undo, sizeof undo, "%s.undo", undoSaveSlot);
  snprintf(thumb, sizeof thumb, "%s.thumb", undoSaveSlot);
  snprintf(undoThumb, sizeof undoThumb, "%s.undo.thumb", undoSaveSlot);
  bool ok = rom_copy_file(undo, undoSaveSlot);
  if (ok) {
    if (pocket_exists(undoThumb))
      rom_copy_file(undoThumb, thumb);
    pocket_remove(undo);
    pocket_remove(undoThumb);
    slotsStale = true;
  }
  undoSaveSlot[0] = 0;
  return ok;
}
static bool undo_load(struct mCore *c) {
  if (!undoLoadState[0])
    return false;
  bool ok = state(c, undoLoadState, false);
  if (ok)
    pocket_remove(undoLoadState);
  undoLoadState[0] = 0;
  return ok;
}
/* The D-pad's cursor over the companion. Kept by hit id, since every draw makes
 * the hit list anew: a press that leaves the screen as it was keeps it where it
 * is (the nearest item if that one went), and a new screen (a tab, a summary, a
 * pick-list, a dex entry) puts it on that screen's start (pd_ui.startId: a
 * pick-list's current choice, a confirm's CANCEL), else its first item. */
static int focusId = -1;
static struct pd_hit focusAt;
struct screen {
  int tab, overlay, summarySlot, dexOpen;
};
static struct screen lastScreen = {-1, -1, -1, -1};
static struct screen screen_of(const struct pd_ui *u) {
  return (struct screen){u->tab, u->overlay, u->summarySlot, u->dexOpen};
}
static void focus_new_screen(void) { lastScreen.tab = -1; }
// What the cursor can land on: not the tab bar, nor a scrim or a list's drag area.
static bool focusable(int id) {
  return !(id >= HIT_TAB && id < HIT_TAB + PD_TAB_COUNT) && id != HIT_FF && id != HIT_SCRIM &&
         id != HIT_BAG_LIST && id != HIT_DEX_LIST && id != HIT_GUIDE_LIST && id != HIT_SETTINGS_LIST;
}
// A window over the screen (the guide's notice, a picker, PLACES) starts with its scrim: the cursor stays on what
// comes after it - the screen under the window can't be reached (or pressed) while it's open.
static int modal_floor(const struct pd_ui *u) {
  for (int i = 0; i < u->hitCount; i++)
    if (u->hits[i].id == HIT_SCRIM)
      return i;
  return 0;
}
static int focus_index(const struct pd_ui *u, int id) {
  for (int i = modal_floor(u); i < u->hitCount; i++)
    if (u->hits[i].id == id)
      return i;
  return -1;
}
// The shared map is a touch target. Linux keeps a tile cursor over that same
// target, using the shared player/section layouts rather than another grid.
static struct {
  bool ready;
  unsigned rom;
  int page, requestedPage, tx, ty, sel, dungeon;
  int minX, minY, maxX, maxY;
} mapCursor;
static void map_cursor_sync(const struct pd_ui *u, const struct pd_game *g,
                            const struct pd_snapshot *s) {
  if (u->tab != PD_TAB_MAP) {
    mapCursor.ready = false;
    return;
  }
  const struct pd_map_art *art = pd_map_art(g);
  if (!art) {
    mapCursor.ready = false;
    return;
  }
  if (focus_index(u, HIT_MAP) < 0)
    return;
  int page = 0, tx = PD_MAP_W / 16, ty = PD_MAP_H / 16;
  bool player = pd_map_player_tile(g, s, &page, &tx, &ty);
  int shown = u->mapPage >= 0 ? u->mapPage : player ? page : 0;
  if (shown >= art->pages) shown = 0;
  const struct pd_region_layout *layouts = g->kind == PD_GAME_EMERALD ?
      pd_layouts_emerald : pd_layouts_firered;
  int pages = g->kind == PD_GAME_EMERALD ? pd_layouts_emerald_count : pd_layouts_firered_count;
  if (shown < 0 || shown >= pages) {
    mapCursor.ready = false;
    return;
  }
  const struct pd_region_layout *layout = &layouts[shown];
  mapCursor.minX = layout->offX;
  mapCursor.minY = layout->offY;
  mapCursor.maxX = layout->offX + layout->w - 1;
  mapCursor.maxY = layout->offY + layout->h - 1;
  if (mapCursor.ready && mapCursor.rom == g->loadId && mapCursor.page == shown &&
      mapCursor.requestedPage == u->mapPage &&
      mapCursor.sel == u->mapSel && mapCursor.dungeon == u->mapDungeon)
    return;
  if (!player || page != shown) {
    tx = PD_MAP_W / 16;
    ty = PD_MAP_H / 16;
  }
  if (u->mapDungeon >= 0) {
    tx = u->mapSelTx;
    ty = u->mapSelTy;
  } else if (u->mapSel >= 0) {
    struct pd_tile_rect r;
    if (pd_map_tiles(g, u->mapSel, &page, &r, 1) && page == shown) {
      tx = r.x + (r.w - 1) / 2;
      ty = r.y + (r.h - 1) / 2;
    }
  }
  mapCursor.ready = true;
  mapCursor.rom = g->loadId;
  mapCursor.page = shown;
  mapCursor.requestedPage = u->mapPage;
  mapCursor.tx = tx < mapCursor.minX ? mapCursor.minX : tx > mapCursor.maxX ? mapCursor.maxX : tx;
  mapCursor.ty = ty < mapCursor.minY ? mapCursor.minY : ty > mapCursor.maxY ? mapCursor.maxY : ty;
  mapCursor.sel = u->mapSel;
  mapCursor.dungeon = u->mapDungeon;
}
static bool map_cursor_active(const struct pd_ui *u) {
  return mapCursor.ready && u->tab == PD_TAB_MAP &&
         u->overlay == PD_OVERLAY_NONE && focusId == HIT_MAP;
}
// B leaves the grid for PLACES / ME / region buttons; another B is normal Back.
static bool map_cursor_back(struct pd_ui *u) {
  int i = focus_index(u, HIT_MAP_PLACES);
  if (!map_cursor_active(u) || i < 0)
    return false;
  focusId = HIT_MAP_PLACES;
  focusAt = u->hits[i];
  return true;
}
static void focus_activate(struct pd_ui *u) {
  int at = focus_index(u, focusId);
  if (at < 0)
    return;
  const struct pd_hit *h = &u->hits[at];
  if (map_cursor_active(u)) {
    // Call the map's existing selection directly: overlaid map buttons must
    // not intercept a tile in the top-right corner of the grid.
    u->touchX0 = h->x + (mapCursor.tx * 8 + 4) * h->w / PD_MAP_W;
    u->touchY0 = h->y + (mapCursor.ty * 8 + 4) * h->h / PD_MAP_H;
    ui_map_act(u, HIT_MAP);
    mapCursor.sel = u->mapSel;
    mapCursor.dungeon = u->mapDungeon;
    mapCursor.requestedPage = u->mapPage;
  } else {
    pd_ui_touch(u, h->x + h->w / 2, h->y + h->h / 2, true);
    pd_ui_touch(u, h->x + h->w / 2, h->y + h->h / 2, false);
  }
}
// After a draw: where the cursor is now (an index into u->hits, -1 = none).
static int focus_resolve(struct pd_ui *u) {
  struct screen now = screen_of(u);
  bool changed = memcmp(&now, &lastScreen, sizeof now) != 0;
  lastScreen = now;
  int floor = modal_floor(u);
  int i = changed ? -1 : focus_index(u, focusId);
  if (i >= 0 && i < floor)
    i = -1;
  if (i < 0 && !changed && focusId >= 0) { // gone: the nearest item to where it was
    int best = 1 << 30;
    for (int j = floor; j < u->hitCount; j++) {
      const struct pd_hit *h = &u->hits[j];
      int dx = h->x + h->w / 2 - (focusAt.x + focusAt.w / 2),
          dy = h->y + h->h / 2 - (focusAt.y + focusAt.h / 2);
      if (focusable(h->id) && dx * dx + dy * dy < best)
        best = dx * dx + dy * dy, i = j;
    }
  }
  if (i < 0 && u->startId >= 0)
    i = focus_index(u, u->startId);
  if (i >= 0 && i < floor)
    i = -1;
  for (int j = floor; i < 0 && j < u->hitCount; j++)
    if (focusable(u->hits[j].id))
      i = j;
  focusId = i >= 0 ? u->hits[i].id : -1;
  if (i >= 0)
    focusAt = u->hits[i];
  return i;
}
static void focus_move(struct pd_ui *u, int direction) {
  if (map_cursor_active(u)) {
    if (direction == LEFT && mapCursor.tx > mapCursor.minX) mapCursor.tx--;
    if (direction == RIGHT && mapCursor.tx < mapCursor.maxX) mapCursor.tx++;
    if (direction == UP && mapCursor.ty > mapCursor.minY) mapCursor.ty--;
    if (direction == DOWN && mapCursor.ty < mapCursor.maxY) mapCursor.ty++;
    focus_activate(u); // Name the location immediately, like the game's map.
    return;
  }
  int cur = focus_index(u, focusId);
  if (cur < 0) {
    focus_resolve(u);
    return;
  }
  const struct pd_hit *h = &u->hits[cur];
  int best = -1, score = 1000000;
  for (int i = modal_floor(u); i < u->hitCount; i++) {
    if (i == cur)
      continue;
    struct pd_hit *t = &u->hits[i];
    // Never onto what focus_resolve can't land on either (the tab chips: L / R own the bar).
    if (!focusable(t->id) || t->id == HIT_SCRIM || t->id == HIT_BAG_LIST || t->id == HIT_DEX_LIST ||
        t->id == HIT_GUIDE_LIST || t->id == HIT_SETTINGS_LIST)
      continue;
    int dx = (t->x + t->w / 2) - (h->x + h->w / 2),
        dy = (t->y + t->h / 2) - (h->y + h->h / 2);
    int along = direction == LEFT    ? -dx
                : direction == RIGHT ? dx
                : direction == UP    ? -dy
                                     : dy;
    // Across: the gap between the two along the other axis, 0 where they overlap (a full-width row right
    // under a button beats a chip far off to the side).
    int across = direction == LEFT || direction == RIGHT
                     ? (t->y + t->h <= h->y ? h->y - (t->y + t->h) : t->y >= h->y + h->h ? t->y - (h->y + h->h) : 0)
                     : (t->x + t->w <= h->x ? h->x - (t->x + t->w) : t->x >= h->x + h->w ? t->x - (h->x + h->w) : 0);
    // What lines up with the cursor on the other axis comes first (the next chip in a tab bar, not the slot
    // just above it); an item off to the side is only reached where nothing lines up.
    int cost = along + across * 4 + (across > 0 ? 100000 : 0);
    if (along > 0 && cost < score) {
      best = i;
      score = cost;
    }
  }
  if (best >= 0) {
    focusId = u->hits[best].id;
    focusAt = u->hits[best];
  } else if (direction == UP || direction == DOWN) {
    int d = direction == DOWN ? 36 : -36;
    if (u->overlay == PD_OVERLAY_PLACES)
      u->placesScroll += d;
    else if (u->tab == PD_TAB_BAG)
      u->bagScroll += d;
    else if (u->tab == PD_TAB_DEX)
      u->dexScroll += d;
    else if (u->tab == PD_TAB_GUIDE)
      u->guideScroll += d;
    else if (u->tab == PD_TAB_SETTINGS && u->overlay == PD_OVERLAY_NONE)
      u->settingsScroll += d;
  }
}
static void focus_draw(struct pd_ui *u, int focus) {
  if (focus < 0 || focus >= u->hitCount)
    return;
  struct pd_hit *h = &u->hits[focus];
  struct pd_hit tile;
  if (map_cursor_active(u)) {
    int x = mapCursor.tx * 8, y = mapCursor.ty * 8;
    tile = (struct pd_hit){h->x + x * h->w / PD_MAP_W,
                          h->y + y * h->h / PD_MAP_H,
                          (x + 8) * h->w / PD_MAP_W - x * h->w / PD_MAP_W,
                          (y + 8) * h->h / PD_MAP_H - y * h->h / PD_MAP_H,
                          HIT_MAP, false};
    h = &tile;
    pd_fill(&canvas, h->x - 1, h->y - 1, h->w + 2, 1, 0x202828);
    pd_fill(&canvas, h->x - 1, h->y + h->h, h->w + 2, 1, 0x202828);
    pd_fill(&canvas, h->x - 1, h->y, 1, h->h, 0x202828);
    pd_fill(&canvas, h->x + h->w, h->y, 1, h->h, 0x202828);
  }
  if (h->id >= HIT_SLOT && h->id < HIT_SLOT + PD_PARTY_SIZE)
    return; // a party slot shows its own selected look (pd_ui.focusId)
  pd_fill(&canvas, h->x, h->y, h->w, 2, 0xff7331);
  pd_fill(&canvas, h->x, h->y + h->h - 2, h->w, 2, 0xff7331);
  pd_fill(&canvas, h->x, h->y, 2, h->h, 0xff7331);
  pd_fill(&canvas, h->x + h->w - 2, h->y, 2, h->h, 0xff7331);
}
/* Pacing. The screen's refresh paces the game where it's close to the game's
 * 59.73 fps (a 60 Hz panel): a game frame per refresh, the present waiting for
 * it, no timer - the timer's frames drifted against the refresh and every few
 * seconds one missed it (the Smart Pro lost ~13 frames in 5 s). The audio then
 * plays a hair faster, so its sample rate follows the queue (dynamic rate
 * control, as RetroArch and the Android app do): +-0.5% around the ratio of the
 * two rates, keeping ~60 ms queued. Elsewhere, or with POCKET_NO_VSYNC, the
 * timer paces as before. */
static bool vsyncPaced;
static double screenHz;
static void pacing_start(double gameHz) {
  SDL_DisplayMode mode = {0};
  screenHz = SDL_GetCurrentDisplayMode(0, &mode) == 0 ? mode.refresh_rate : 0;
  SDL_RendererInfo info;
  vsyncPaced = !getenv("POCKET_NO_VSYNC") && screenHz >= 58 && screenHz <= 62 &&
               SDL_GetRendererInfo(renderer, &info) == 0 && (info.flags & SDL_RENDERER_PRESENTVSYNC) &&
               gameHz > 0;
}
// A refresh-paced loop whose presents don't wait (a driver ignoring vsync) goes back to the timer.
static void pacing_check(double frameMs) {
  static double avg;
  static int count;
  if (!vsyncPaced)
    return;
  avg += frameMs;
  if (++count < 120)
    return;
  if (avg / count < 12.0) {
    vsyncPaced = false;
    fprintf(stderr, "Presents don't wait for the refresh (%.1f ms apart): pacing by the timer\n", avg / count);
  }
  avg = 0;
  count = 0;
}
/* plorpOS: speed > 1 is fast-forward with its sound: as many samples as one frame's, so it plays sped up. */
// INSTANT TEXT / REUSABLE TMS onto the ROM, or the file's bytes back (pd_rom_tweaks.c). mGBA copies the ROM before
// its first write, so the file and the bytes the game was named and hashed by stay as they were.
static void tweak_apply(struct mCore *c, const uint8_t *rom, const struct pd_tweak_patch *p, bool on) {
  for (int i = 0; i < p->count; i++)
    for (int b = 0; b < p->writes[i].len; b++)
      c->rawWrite8(c, 0x08000000u + p->writes[i].at + (uint32_t)b, -1, on ? p->writes[i].value[b] : rom[p->writes[i].at + (uint32_t)b]);
}

static void audio_rate(struct mCore *c, double gameHz, double speed) {
  double rate = 32768;
  if (vsyncPaced && audio && screenHz > 0) {
    double queued = SDL_GetQueuedAudioSize(audio) / 4.0, target = 32768 * 0.06;
    double d = (target - queued) / target;
    d = d > 1 ? 1 : d < -1 ? -1 : d;
    rate = 32768 * gameHz / screenHz * (1 + 0.005 * d);
  }
  rate /= speed;
  for (int i = 0; i < 2; i++)
    blip_set_rates(c->getAudioChannel(c, i), c->frequency(c), rate);
}
/* POCKET_PERF=1: every 5 s, how the frames went: loop iterations, the work in
 * one (emulation, EXPAND, drawing, present) on average and at worst, how many
 * took longer than a game frame, how far apart presents landed, the screen's
 * refresh rate. */
static void perf_frame(uint64_t start, double hz) {
  static int on = -1, n, late;
  static double sum, worst, gapSum, gapWorst, presents, emus, wides;
  static uint64_t lastEnd, windowStart;
  if (on < 0)
    on = getenv("POCKET_PERF") != NULL;
  if (!on)
    return;
  uint64_t end = SDL_GetPerformanceCounter(), f = SDL_GetPerformanceFrequency();
  double work = (double)(end - start) * 1000.0 / f, budget = 1000.0 / hz;
  if (!windowStart)
    windowStart = end;
  n++;
  sum += work;
  presents += presentMs;
  emus += emuMs;
  wides += wideMs;
  presentMs = emuMs = wideMs = 0;
  if (work > worst)
    worst = work;
  if (work > budget)
    late++;
  if (lastEnd) {
    double gap = (double)(end - lastEnd) * 1000.0 / f;
    gapSum += gap;
    if (gap > gapWorst)
      gapWorst = gap;
  }
  lastEnd = end;
  if (end - windowStart >= 5 * f) {
    SDL_DisplayMode mode = {0};
    SDL_GetCurrentDisplayMode(0, &mode);
    fprintf(stderr, "perf: emulation %.2f ms, EXPAND %.2f ms a frame\n", emus / n, wides / n);
    fprintf(stderr, "perf: %d frames in %.1f s, work %.2f ms avg (%.2f of it waiting in present) / %.2f worst, "
            "%d over %.2f ms, frame to frame %.2f ms avg / %.2f worst, screen %d Hz, %s, audio queued %u B\n",
            n, (double)(end - windowStart) / f, sum / n, presents / n, worst, late, budget,
            gapSum / (n > 1 ? n - 1 : 1), gapWorst, mode.refresh_rate,
            vsyncPaced ? "paced by the screen" : "paced by the timer", audio ? SDL_GetQueuedAudioSize(audio) : 0);
    n = late = 0;
    sum = worst = gapSum = gapWorst = presents = emus = wides = 0;
    windowStart = end;
  }
}
#ifdef POCKET_TEST
static void test_frame(struct mCore *, bool, int, struct pd_ui *,
                       struct pd_snapshot *);
#endif
/* plorpOS: REWIND, the app's ring - a state every 2 frames, 600 of them (~20 s) - while settings.rewind. */
static struct mCoreRewindContext rewindRing;
static bool rewindOn;
static int rewindTick;
static void rewind_follow(bool on) {
  if (on == rewindOn)
    return;
  if (rewindOn)
    pk_rewind_deinit(&rewindRing);
  else
    pk_rewind_init(&rewindRing, 600);
  rewindOn = on;
  rewindTick = 0;
}
/* SCENE SPEEDS: what's on screen (PD_SCENE_*, -1 = the plain field or a scene not told apart), as the app has it:
 * a menu screen (a battle's own bag and party screens too), a battle, a text box on the field, riding or surfing with
 * the controls free. Menus come from EXPAND's screen read, the rest from the field read (fieldFlags). */
static bool sceneFieldSeen; // the map has been on screen since the game started: the title screens aren't menus
static int scene_now(int flags, bool fieldKnown, const struct pd_snapshot *snap) {
  int mode = wideSupported ? pk_wide_last_mode() : -1;
  if (mode == PK_WIDE_MODE_MAP)
    sceneFieldSeen = true;
  if (mode == PK_WIDE_MODE_MENU && sceneFieldSeen && pk_wide_scene() != PK_WIDE_SCENE_FADE)
    return PD_SCENE_MENU;
  if ((flags & PK_FIELD_BATTLE) || (!fieldKnown && snap->inBattle) || mode == PK_WIDE_MODE_BATTLE)
    return PD_SCENE_BATTLE;
  if (flags & PK_FIELD_TEXT)
    return PD_SCENE_TEXT;
  if ((flags & (PK_FIELD_LOCKED | PK_FIELD_AVATAR)) == PK_FIELD_AVATAR)
    return flags & PK_FIELD_SURF ? PD_SCENE_SURF : flags & PK_FIELD_BIKE ? PD_SCENE_BIKE : -1;
  return -1;
}
/* The speed with fast-forward off: the scene's SCENE SPEEDS pick, else GAME SPEED's. */
static double scene_pace(const struct pd_settings *s, const struct pd_host_info *h, int scene) {
  static const double SCENE_X[PD_SCENE_SPEED_COUNT] = {0, 1, 1.5, 2, 3, 4};
  if (scene >= 0 && h->sceneKnown[scene] && s->sceneSpeed[scene] > 0)
    return SCENE_X[s->sceneSpeed[scene]];
  return PD_GAME_SPEED_PCT[s->gameSpeed] / 100.0;
}
/* FF MODE SMART: 1x while the game waits for the player (the app's SmartFf.kt, the 3DS's smart_slows): a GBA battle's
 * menus (FIGHT / a move / a target / the bag / the party), and a menu screen where EXPAND reads the scene. Never its
 * animations or text. */
static bool smart_slows(struct mCore *c, const struct pd_game *g, bool inBattle) {
  if (inBattle && g->cfg && c->platform(c) == mPLATFORM_GBA) {
    switch (pd_battle_state_read(g->cfg, read_memory, c, NULL)) {
    case PD_BATTLE_ACTION:
    case PD_BATTLE_MOVE:
    case PD_BATTLE_TARGET:
    case PD_BATTLE_PARTY_OPEN:
    case PD_BATTLE_BAG_OPEN:
      return true;
    default:
      return false;
    }
  }
  return wideSupported && pk_wide_scene() == PK_WIDE_SCENE_MENU;
}
/* plorpOS: fast-forward's frames this refresh - its speed, or for MAX as many as ~80% of a frame's time fits
 * (frameMs: what one cost lately). */
static int ff_frames(const struct pd_settings *s, double hz, double frameMs) {
  static double owed; // 1.5x: one frame, then two, the fraction carried
  if (s->ffSpeed) {
    owed += s->ffSpeed / 100.0;
    int n = (int)owed;
    owed -= n;
    return n;
  }
  int n = frameMs > 0 ? (int)(800.0 / hz / frameMs) : 2;
  return n < 2 ? 2 : n > 30 ? 30 : n;
}
#ifdef POCKET_DS
extern int pd_ds_run(const char*, const char*, int, const char*, SDL_Renderer*, bool (*)(void));
extern void (*pd_ds_device_event)(const SDL_Event*);
extern bool (*pd_ds_host_event)(const SDL_Event*);
extern int pd_ds_xbox_layout;
extern unsigned (*pd_ds_host_actions)(void);
extern void (*pd_ds_present)(void);
extern void (*pd_ds_point)(float, float, float *, float *);
extern int pd_ds_game_touch_off;
extern bool (*pd_ds_rewind_held)(void);
extern int pd_ds_rewind_on;
extern int pd_ds_ui_size, pd_ds_wing_room, pd_ds_theme;
extern bool pd_ds_dark, pd_ds_expand, pd_ds_auto_zoom;
static bool ds_rewind_held(void) { return rewindHeld || rewindKey; }
static bool ds_running(void) { return running != 0; }
#endif
#ifdef POCKET_DS
/* The DS loop's share of the HOTKEYS: its bits, cleared once read (pd_ds_host_actions). */
/* A DS game's controller events, before the DS loop's own use of them: FULLSCREEN's keys, then HOTKEYS as a GBA game
 * has them (stick_hotkeys, the extra buttons): LT / RT ZOOM OUT / IN, L3 FF TOGGLE and R3 USUAL USE (DS LAYOUT, held
 * the picker) by default. A button left on USUAL USE goes on to the DS loop: L2 its quick menu, R2 SWAP SCREENS. */
static bool ds_host_event(const SDL_Event *e) {
  if (fullscreen_event(e))
    return true;
  static bool trigger[2];
  if (e->type == SDL_CONTROLLERAXISMOTION &&
      (e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) {
    int i = e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT, action = hotkeyMap[i ? PD_HOTKEY_R2 : PD_HOTKEY_L2];
    bool down = e->caxis.value > 16000;
    if (!action)
      return false;
    if (action == HA_REWIND)
      rewindHeld = down;
    else if (down && !trigger[i])
      hotkey_fire(action);
    trigger[i] = down;
    return true;
  }
  if (e->type == SDL_CONTROLLERBUTTONDOWN || e->type == SDL_CONTROLLERBUTTONUP) {
    bool down = e->type == SDL_CONTROLLERBUTTONDOWN;
    int slot = e->cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK    ? PD_HOTKEY_L3
               : e->cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK ? PD_HOTKEY_R3
                                                                       : -1;
    for (int i = 0; slot < 0 && i < PD_HOTKEY_BUTTONS; i++)
      if (HOTKEY_BUTTONS[i] >= 0 && HOTKEY_BUTTONS[i] == e->cbutton.button)
        slot = i;
    bool stick = slot == PD_HOTKEY_L3 || slot == PD_HOTKEY_R3;
    bool chord = stickChord;
    if (stick && !down) { // both sticks up: the next release is an action again
      SDL_GameController *pad = SDL_GameControllerFromInstanceID(e->cbutton.which);
      if (!pad || (!SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSTICK) &&
                   !SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK)))
        stickChord = false;
    }
    if (slot < 0 || !hotkeyMap[slot])
      return false;
    int action = hotkeyMap[slot];
    if (action == HA_REWIND)
      rewindHeld = down;
    else if (stick ? !down && !chord : down) // a stick's action on its release, so L3 + R3 stays FULLSCREEN
      hotkey_fire(action);
    return true;
  }
  return false;
}
static unsigned ds_host_actions(void) {
  unsigned bits = hostActions;
  hostActions = 0;
  return bits;
}
#endif
/* RUMBLE (settings.rumble): every open pad buzzes as a battle starts, harder for a shiny foe, and along with a rumble
 * cartridge (mGBA's mPERIPH_RUMBLE). POCKET_RUMBLE_LOG prints each one (headless checks: no pad there). */
static bool rumbleOn;
static void rumble(uint16_t low, uint16_t high, uint32_t ms) {
  if (!rumbleOn && (low || high))
    return;
  if (getenv("POCKET_RUMBLE_LOG"))
    fprintf(stderr, "rumble %u %u %u\n", low, high, ms);
  for (int i = 0; i < padCount; i++)
    SDL_GameControllerRumble(pads[i], low, high, ms);
}
static void cart_rumble(struct mRumble *r, int enable) {
  static int was;
  (void)r;
  if (!!enable == was)
    return;
  was = !!enable;
  rumble(was ? 0xA000 : 0, was ? 0xA000 : 0, was ? 2000 : 0);
}
static struct mRumble cartRumble = {.setRumble = cart_rumble};
static void rumble_events(const struct pd_snapshot *s, bool fieldBattle) {
  static bool was, shinySeen;
  bool now = (s->valid && s->inBattle) || fieldBattle;
  if (now && !was)
    rumble(0x5000, 0x3000, 180);
  if (!now)
    shinySeen = false;
  else if (!shinySeen && s->valid && s->inBattle)
    for (int i = 1; i < 4; i += 2) // the foes' places
      if (s->battlers[i].species && s->battlers[i].shiny) {
        shinySeen = true;
        rumble(0xFFFF, 0xC000, 700);
        break;
      }
  was = now;
}
/* THIS GAME ONLY: a game can keep its own settings in DATA/game-settings/<rom>.ini. The program-wide ones (buttons,
 * hotkeys, the window, updates, rumble ...) stay in settings.ini whatever the game: they're read from there and written
 * back there, so a change to them while a game keeps its own still holds everywhere. */
static void settings_host_wide(struct pd_settings *dst, const struct pd_settings *src) {
  dst->guideNotice = src->guideNotice;
  dst->buttonLayout = src->buttonLayout;
  memcpy(dst->hotkeys, src->hotkeys, sizeof dst->hotkeys);
  dst->orientation = src->orientation;
  dst->gameTouch = src->gameTouch;
  dst->updateCheck = src->updateCheck;
  dst->resumeGames = src->resumeGames;
  dst->rumble = src->rumble;
  dst->batterySaver = src->batterySaver;
  dst->fullscreen = src->fullscreen;
  dst->savesBeside = src->savesBeside;
  dst->boxartAuto = src->boxartAuto;
  dst->quickBtn = src->quickBtn;
  dst->companionBtn = src->companionBtn;
  dst->hudBtn = src->hudBtn;
  dst->uiSize = src->uiSize; // UI SIZE is the screen's, not a game's
}
static char gameConfig[1100]; // the running game's own settings file, "" while it uses settings.ini
static void settings_put(const struct pd_settings *s, const char *config) {
  if (!*gameConfig) {
    pd_settings_save(s, config);
    return;
  }
  pd_settings_save(s, gameConfig);
  struct pd_settings g;
  pd_settings_load(&g, config);
  settings_host_wide(&g, s);
  pd_settings_save(&g, config);
}
/* BATTERY SAVER (settings.batterySaver): AUTO saves while the machine runs on its battery (checked every few
 * seconds), ON always. Saving: fast forward at most 3x, xBRZ drawn as SHARP, the companion and the game list redrawn
 * half as often. */
static bool saver_active(void) {
  static uint32_t checked;
  static bool onBattery;
  if (saverMode == PD_SAVER_ON)
    return true;
  if (saverMode != PD_SAVER_AUTO)
    return false;
  uint32_t t = SDL_GetTicks();
  if (!checked || t - checked > 3000) {
    checked = t ? t : 1;
    int secs, pct;
    onBattery = SDL_GetPowerInfo(&secs, &pct) == SDL_POWERSTATE_ON_BATTERY || getenv("POCKET_ON_BATTERY");
  }
  return onBattery;
}
/* NUZLOCKE: each game's run in DATA/nuzlocke/<crc>.txt, "on" then the app's own text for the run (NuzlockeRun). */
static void nuz_path(char *out, size_t n, uint32_t crc) { snprintf(out, n, "%s/nuzlocke/%08x.txt", root, (unsigned)crc); }
static void nuz_load(uint32_t crc) {
  pd_nuz_init(&nuzlocke);
  char path[1024];
  nuz_path(path, sizeof path, crc);
  FILE *f = fopen(path, "rb");
  if (!f)
    return;
  static char text[(PD_NUZ_MAX_ENCOUNTERS + PD_NUZ_MAX_MONS) * 160 + 64];
  size_t got = fread(text, 1, sizeof text - 1, f);
  fclose(f);
  text[got] = 0;
  nuzlocke.on = !strncmp(text, "on\t1", 4);
  const char *run = strchr(text, '\n');
  pd_nuz_decode(&nuzlocke.run, run ? run + 1 : "");
}
static void nuz_store(uint32_t crc) {
  if (!nuzlocke.dirty)
    return;
  nuzlocke.dirty = false;
  char path[1024], tmp[1040];
  snprintf(path, sizeof path, "%s/nuzlocke", root);
  pocket_mkdir(path);
  nuz_path(path, sizeof path, crc);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  static char text[(PD_NUZ_MAX_ENCOUNTERS + PD_NUZ_MAX_MONS) * 160 + 64];
  int head = snprintf(text, sizeof text, "on\t%d\n", nuzlocke.on ? 1 : 0);
  size_t n = (size_t)head + pd_nuz_encode(&nuzlocke.run, text + head, sizeof text - (size_t)head);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return;
  bool ok = fwrite(text, 1, n, f) == n;
  ok = fclose(f) == 0 && ok;
  if (!ok) {
    remove(tmp);
    return;
  }
  remove(path);
  rename(tmp, path);
}

static void run_game(const char *path, int maxFrames, const char *shot) {
  {
    const char *file = path + strlen(path);
    while (file > path && file[-1] != '/' && file[-1] != '\\')
      file--;
    snprintf(shotName, sizeof shotName, "%s", file);
    char *dot = strrchr(shotName, '.');
    if (dot && dot != shotName)
      *dot = 0;
  }
  hostActions = 0;
#ifdef POCKET_DS
  const char *ext = strrchr(path, '.');
  if (ext && !strcasecmp(ext, ".nds")) {
    if (audio) { SDL_CloseAudioDevice(audio); audio = 0; }
    pd_ds_device_event = controller_device_event;
    pd_ds_host_event = ds_host_event;
    pd_ds_xbox_layout = buttonLayout == PD_LAYOUT_XBOX;
    pd_ds_host_actions = ds_host_actions;
    pd_ds_present = timed_present;
    pd_ds_point = pd_host_point;
    pd_ds_game_touch_off = gameTouch == PD_GAME_TOUCH_OFF;
    rewindHeld = rewindKey = false;
    pd_ds_rewind_held = ds_rewind_held;
    {
      char cfg[1100];
      struct pd_settings st;
      snprintf(cfg, sizeof cfg, "%s/settings.ini", root);
      pd_settings_load(&st, cfg);
      pd_ds_rewind_on = st.rewind || rewind_hotkey();
      pd_ds_ui_size = st.uiSize;     // UI SIZE and WING ROOM: the DS layouts follow the screen's settings
      pd_ds_theme = st.theme;        // THEME and DARK MODE: the DS companion wears the app's look
      pd_ds_dark = st.darkMode;
      pd_ds_wing_room = st.wingRoom;
      pd_ds_expand = st.aspect == PD_ASPECT_EXPAND; // ASPECT and AUTO ZOOM IN: DS EXPAND and its camera
      pd_ds_auto_zoom = st.autoZoom;
    }
    appError = pd_ds_run(path, root, maxFrames, shot, renderer, ds_running);
    // The DS backend owns its device while running. Restore the normal host's
    // device for the next GB/GBA game selected without restarting the app.
    audio_open();
    audioLost = !audio;
    held = pressed = 0;
    return;
  }
#endif
  FILE *f = fopen(path, "rb");
  if (!f) {
    appError = 1;
    notice("Cannot open ROM");
    return;
  }
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  rewind(f);
  if (len < 0x150 || len > 40 * 1024 * 1024) {
    fclose(f);
    appError = 1;
    notice("Invalid ROM size");
    return;
  }
  unsigned char *romdata = malloc((size_t)len);
  if (!romdata || fread(romdata, 1, len, f) != (size_t)len) {
    free(romdata);
    fclose(f);
    appError = 1;
    notice("Cannot read ROM");
    return;
  }
  fclose(f);
  struct VFile *vf = VFileOpen(path, O_RDONLY);
  struct mCore *c = vf ? mCoreFindVF(vf) : NULL;
  if (!c || !c->init(c)) {
    if (vf)
      vf->close(vf);
    free(romdata);
    appError = 1;
    notice("Unsupported ROM");
    return;
  }
  mCoreInitConfig(c, NULL);
  if (c->platform(c) == mPLATFORM_GB) {
    // Match upstream's Android GB setup, including SGB-enhanced cartridges.
    mCoreConfigSetIntValue(&c->config, "sgb.borders", 0);
    c->reloadConfigOption(c, "sgb.borders", &c->config);
  }
  c->setVideoBuffer(c, video, 256);
  c->setAudioBufferSize(c, 512);
  if (!c->loadROM(c, vf)) {
    vf->close(vf);
    mCoreConfigDeinit(&c->config);
    c->deinit(c);
    free(romdata);
    appError = 1;
    notice("ROM load failed");
    return;
  }
  char sav[1024], ss[1024];
  struct pocket_save_status saveStatus = {0};
  struct ffm *ffMusic = NULL; // FF MUSIC's STEADY recorder (GBA games)
  struct ffm_clip ffClip = {0};
  const char *name = strrchr(path, '/');
#ifdef _WIN32
  // A Windows path may use either separator: the save / state is named after the file alone, as on Linux.
  const char *back = strrchr(path, '\\');
  if (back && (!name || back > name))
    name = back;
#endif
  name = name ? name + 1 : path;
  if (saveFile)
    snprintf(sav, sizeof sav, "%s", saveFile);
  else {
    snprintf(sav, sizeof sav, "%s/saves/%s.sav", root, name);
    char ini[1024];
    snprintf(ini, sizeof ini, "%s/settings.ini", root);
    struct pd_settings saveSettings;
    pd_settings_load(&saveSettings, ini);
    if (saveSettings.savesBeside && rom_in_added_folder(root, path)) {
      // SAVES BESIDE GAMES (on by default) for a game in an added folder: "Game.sav" next to "Game.gba", as
      // RetroArch and the Android app keep it; DATA/roms' games keep DATA/saves. A save already beside the game
      // wins; else one played before in DATA/saves is copied there the first time (the old one is left alone).
      char beside[1024];
      rom_save_beside(beside, sizeof beside, path);
      if (!pocket_exists(beside) && pocket_exists(sav) && !rom_copy_file(sav, beside)) {
        appError = 1;
        notice("Save copy failed; game not opened");
        goto cleanup;
      }
      snprintf(sav, sizeof sav, "%s", beside);
    }
  }
  snprintf(ss, sizeof ss, "%s/states/%s.ss", root, name);
  if (!pocket_save_backup(sav, time(NULL))) {
    appError = 1;
    notice("Save backup failed; game not opened");
    goto cleanup;
  }
  struct VFile *sv = pocket_save_wrap(VFileOpen(sav, O_CREAT | O_RDWR), &saveStatus);
  if (!sv || !c->loadSave(c, sv)) {
    if (sv)
      sv->close(sv);
    appError = 1;
    notice("Save file unavailable; game not opened");
    goto cleanup;
  }
  struct mAVStream stream = {.postAudioBuffer = post_audio};
  c->setAVStream(c, &stream);
  c->setPeripheral(c, mPERIPH_RUMBLE, &cartRumble);
  for (int i = 0; i < 2; i++)
    blip_set_rates(c->getAudioChannel(c, i), c->frequency(c), 32768);
  c->reset(c);
  pk_heal_rtc(c);
  ra_load_game(c, romdata, (size_t)len); // RetroAchievements: the file's hash, when you're signed in
  uint32_t crc = 0;
  c->checksum(c, &crc, mCHECKSUM_CRC32);
  wideSupported = pk_wide_configure(crc) != 0;
  wideReady = false;
  wide_camera_reset(&wideCam);
  wideCamAspect = -1;
  // WILD ENCOUNTERS: the game's own switch (wild.c finds it), held by pk_wide's field read every frame.
  uint32_t wildSwitch = 0;
  bool fieldKnown = c->platform(c) == mPLATFORM_GBA && pk_field_configure(crc);
  if (fieldKnown)
    wildSwitch = pk_wild_switch_find(romdata, (size_t)len);
  pk_rest_reset(); // pk_call (FLY) learns this game's main loop depth
  unsigned gw, gh;
  c->desiredVideoDimensions(c, &gw, &gh);
  struct pd_game game;
  pd_game_detect(&game, romdata, (size_t)len);
  game.rom = romdata;
  game.romSize = (size_t)len;
  pd_game_identify(&game);
  fprintf(stderr, "pokedaisy: %s, sha1 %s%s%s\n", game.title ? game.title : "?", game.sha1[0] ? game.sha1 : "-",
          game.unsupported ? " - " : "", game.unsupported ? game.unsupported : "");
  if (c->platform(c) == mPLATFORM_GBA)
    pd_romart_scan(&game);
  char config[1024];
  snprintf(config, sizeof config, "%s/settings.ini", root);
  struct pd_settings settings;
  pd_settings_load(&settings, config);
  snprintf(gameConfig, sizeof gameConfig, "%s/game-settings/%s.ini", root, name);
  if (pocket_exists(gameConfig)) {
    struct pd_settings global = settings;
    pd_settings_load(&settings, gameConfig);
    settings_host_wide(&settings, &global);
  } else
    gameConfig[0] = 0;
  if (wildSwitch)
    pk_field_wild(wildSwitch, settings.wildOff);
  bool quickOpen = false;
  int quickSel = 0;
  // FLY (fly.c): the config's FLY where the ROM's code matches. flyAsk = its destinations wanted (asked for between
  // two frames the game's code can be called in), flyList = the menu shows them; flyStage 1 = start the flight,
  // 2 = the fade, then finish it.
  const struct pd_config *flyCfg = game.cfg && c->platform(c) == mPLATFORM_GBA && fly_supported(game.cfg, romdata, (size_t)len) ? game.cfg : NULL;
  struct fly_targets flyT = {0};
  const char *flyNames[FLY_MAX_TOWNS];
  int flyAsk = 0, flySel = 0, flyStage = 0, flyMapsec = 0, flyWait = 0, fieldFlags = 0;
  bool flyList = false;
  uint32_t flyCallback = 0;
  // REPEL, BIKE and PC anywhere (field_items.c): offered where the ROM's code is the config's. A job waits for
  // two frames the game's code can be called between; BIKE then presses SELECT for the game (gameKeys), watches
  // the riding state flip (or the field lock) and puts the player's own registered item back 20 frames later.
  bool itemsOk = game.cfg && c->platform(c) == mPLATFORM_GBA && field_items_supported(game.cfg, romdata, (size_t)len);
  bool pcOk = itemsOk && pc_supported(game.cfg, romdata, (size_t)len);
  bool bikeOk = itemsOk && bike_supported(game.cfg);
  enum { JOB_NONE, JOB_PC, JOB_REPEL, JOB_BIKE, JOB_BIKE_WAIT, JOB_BIKE_RESTORE, JOB_EVOLVE } job = JOB_NONE;
  int jobItem = 0, jobFrames = 0, jobTries = 0, repelSel = 0, repelKinds[REPEL_KINDS], repelKindCount = 0;
  bool repelList = false, bikeWasRiding = false, bikeTook = false;
  uint16_t bikeOld = 0;
  unsigned gameKeys = 0;
  int gameKeyFrames = 0;
  // The HUD: on after 20 frames of the plain field (FieldHud's rule), off at once.
  struct hud_state hudState = { .wild = -1, .focus = -1 };
  int hudFieldFrames = 0;
  bool hudFocus = false; // the HUD hotkey's cursor is on (the game gets no keys meanwhile)
  hudDrawnKey = 0;
  char repelRows[REPEL_KINDS][48];
  const char *repelNames[REPEL_KINDS];
  struct pd_ui ui;
  pd_ui_init(&ui, &settings);
  // NUZLOCKE: this game's run, followed sample by sample while it's on (GBA games, not Emerald Rogue).
  nuz_load(crc);
  nuzlocke.supported = c->platform(c) == mPLATFORM_GBA && pd_nuz_supports(&game);
  ui.nuzlocke = &nuzlocke; // N/A where it doesn't work, as on Android
  hotkeys_load();
  rewind_follow(settings.rewind || rewindBound || rewind_hotkey());
  double frameCost = 0;
  uint64_t lastStart = 0;
  struct pd_snapshot snap = {0};
  struct pd_host_info host = {.hideScreenMode = true, .hasExpand = true, .hasLookAround = true, .hasFfChip = true, .hasQuickMenu = true, .hasDisplayFilter = true, .hasHud = c->platform(c) == mPLATFORM_GBA,
                              .expandGame = wideSupported, .hasCompanionView = true, .hasUpdateCheck = true, .hasShaders = true, .hasSavesBeside = !saveFile, .hasFullscreen = true, .hasButtonLayout = true, .hasOrientation = true, .hasGameTouch = true, .count = 1, .labels = {"HOST"},
                              .fullSettings = true, .androidMenus = true, .hasRewind = true, .hasFfMusic = true,
                              .batteryPct = -1, .layouts = companion_layouts(), .layoutCount = POCKET_VIEWS};
  snprintf(host.values[0], 40, POCKET_HOST_NAME);
  host.hasStates = true; // STATES: slot 0 (the old one state file) is picked to begin with
  // INSTANT TEXT and REUSABLE TMS: found in the ROM's own code, GBA games only (as on Android).
  struct pd_tweak_patch textPatch = {0}, tmPatch = {0};
  bool textOn = false, tmsOn = false;
  if (c->platform(c) == mPLATFORM_GBA) {
    pd_instant_text_find(romdata, (size_t)len, &textPatch);
    pd_reusable_tms_find(romdata, (size_t)len, &tmPatch);
    host.hasRomTweaks = true;
    host.instantTextFits = textPatch.count > 0;
    host.reusableTmsFits = tmPatch.count > 0;
    // Radical Red's, Unbound's and SoulGold's own TMs never run out.
    host.reusableTmsBuiltIn = game.guide == PD_GUIDE_RADICAL_RED || game.guide == PD_GUIDE_UNBOUND || game.guide == PD_GUIDE_SOULGOLD;
  }
  // TRADE EVOLUTIONS: retail English FireRed rev 1, LeafGreen and Emerald, where the config has the game's evolution
  // functions and their code is where it says (TradeEvolution.kt). Who gets EVOLVE is asked of the game between frames.
  bool tradeOk = c->platform(c) == mPLATFORM_GBA && game.cfg && trade_evo_supported(game.cfg, romdata, (size_t)len);
  host.hasTradeEvos = c->platform(c) == mPLATFORM_GBA;
  // SCENE SPEEDS: BATTLE everywhere, TEXT where the field read works, MENU where EXPAND reads the screen, BIKE and
  // SURF once the field read has found the player's own state (below). GAME SPEED's pace in between.
  // FF MUSIC's STEADY: each song a GBA game plays is recorded on its own, on a second core, as the game starts it
  // (ff_music.c); while the game races, that recording plays at its normal speed instead of the game's sound.
  if (c->platform(c) == mPLATFORM_GBA)
    ffMusic = ffm_start(romdata, (size_t)len, crc, root);
  host.hasSteady = ffMusic != NULL;
  uint32_t songKey = 0, songAsked = 0;
  Uint32 songAt = 0;
  bool clipOn = false;
  host.hasSceneSpeeds = true;
  host.sceneKnown[PD_SCENE_BATTLE] = true;
  host.sceneKnown[PD_SCENE_TEXT] = fieldKnown;
  host.sceneKnown[PD_SCENE_MENU] = wideSupported;
  double paceAcc = 0;
  sceneFieldSeen = false;
  host.tradeEvoFits = tradeOk;
  // SHINY ODDS: retail and the hacks the app checked, by hash; each new wild foe looked at as the game makes it.
  struct shiny_target shinyTarget;
  static struct shiny_roller shinyRoller;
  bool shinyOk = c->platform(c) == mPLATFORM_GBA && shiny_target_for(&game, &shinyTarget);
  if (shinyOk)
    shiny_roller_init(&shinyRoller, &shinyTarget, (uint64_t)SDL_GetPerformanceCounter() ^ (uint64_t)time(NULL) << 32);
  host.hasShinyOdds = c->platform(c) == mPLATFORM_GBA;
  host.shinyOddsFits = shinyOk;
  // DAY AND NIGHT: any GBA game but those with a day and night of their own (DayNight.OWN_DAY_NIGHT).
  host.hasDayNight = c->platform(c) == mPLATFORM_GBA;
  host.dayNightFits = host.hasDayNight && game.guide != PD_GUIDE_UNBOUND && game.guide != PD_GUIDE_HEART_AND_SOUL &&
                      game.guide != PD_GUIDE_ELITE_REDUX;
  static struct day_night dayNight;
  day_night_reset(&dayNight);
  int dayNightTarget[3] = {256, 256, 256}, dayNightClockTint[3] = {256, 256, 256};
  uint32_t dayNightClock = 0;
  uint32_t evolvePers[6] = {0}, evolveSign = 0;
  int evolveSlot = -1, evolveTick = 0;
  host.hasResume = true;
  host.hasRumble = true;
  host.hasPerGame = true;
  host.hasSaver = true;
  host.perGame = *gameConfig != 0;
  slotsStale = true;
  /* RESUME GAMES: leaving a game leaves states/<rom>.resume.ss; the next start asks CONTINUE or START FROM SAVE.
   * States never carry the save, so the save file is never touched; a save newer than the state (played elsewhere
   * since, beyond the few seconds the save's own last write can trail the state) skips the question. */
  char resumePath[1100];
  snprintf(resumePath, sizeof resumePath, "%s/states/%s.resume.ss", root, name);
  bool resumeOffer = false;
  {
    struct stat rs, vs;
    if (settings.resumeGames && !stat(resumePath, &rs) && rs.st_size > 0)
      resumeOffer = stat(sav, &vs) || vs.st_mtime <= rs.st_mtime + 10;
  }
  const char *opts[] = {"Resume game",
                        "Party",
                        "Save state",
                        "Load state",
                        "Toggle fast-forward",
                        "Toggle fullscreen",
                        "Quit to game list",
                        "Achievements",
                        "About"};
  const char *confirmOpts[] = {"Cancel", "Confirm"};
  const char *resumeOpts[] = {"Continue", "Start from save"};
  struct pd_menu gameMenu, confirmMenu, resumeMenu, raMenu;
  // ACHIEVEMENTS (menuOpen 7): the game's RetroAchievements, made when it opens.
  static char raRows[400][96], raDesc[400][160];
  static const char *raItems[400];
  static char raTitle[64];
  bool wingClose = false; // the CLOSE GAME question came from the right wing's button
  pd_menu_init(&gameMenu, "GAME", opts, 9);
  pd_menu_init(&raMenu, "ACHIEVEMENTS", raItems, 0);
  pd_menu_init(&confirmMenu, "CONFIRM", confirmOpts, 2);
  pd_menu_init(&resumeMenu, "Continue where you left off?", resumeOpts, 2);
  int menuOpen = 0; // 1 the game menu, 2 / 3 / 5 its confirms, 4 About, 6 RESUME GAMES' question, 7 ACHIEVEMENTS
  bool companion = false, ff = false, leave = false;
  undo_forget(); // a new game: the last one's undos aren't this one's
  if (resumeOffer) {
    menuOpen = 6;
    companion = true;
  }
  int frames = 0;
  bool companionWas = false;
  focusId = -1;
  focus_new_screen();
  uint64_t due = SDL_GetPerformanceCounter(), lastFrame = due;
  double hz = (double)c->frequency(c) / c->frameCycles(c);
  pacing_start(hz);
  while (running && !leave) {
    settings.fullscreen = fullscreenOn ? 1 : 0; // F11 / L3 + R3 changed it behind SETTINGS' back
    uint64_t frameStart = SDL_GetPerformanceCounter();
    if(saveStatus.failed) { appError=1; notice("Save I/O failed; stopping game"); break; }
    // plorpOS: back from a pause (its in-game menu stops this process, as does sleep): what its OPTIONS
    // changed in settings.ini, taken at once.
    if (lastStart && (frameStart - lastStart) * 4 > SDL_GetPerformanceFrequency()) {
      struct pd_settings fresh;
      if (pd_settings_load(&fresh, config)) {
        settings = fresh;
        if (wildSwitch)
          pk_field_wild(wildSwitch, settings.wildOff);
        hudDrawnKey = 0;
      }
      hotkeys_load();
    }
    lastStart = frameStart;
    rewind_follow(settings.rewind || rewindBound || rewind_hotkey());
    lcdEnabled = settings.filter == PD_FILTER_SUBPIXEL;
    gameFilter = settings.filter;
    gameGrid = settings.grid;
    pocket_shader_desaturate(settings.filter == PD_FILTER_LCD_PAPER ? 15 : 0);
    saverMode = settings.batterySaver;
    bool saving = saver_active();
    host.saving = saving;
    pocket_shader_set(saving && settings.shader == POCKET_SHADER_XBRZ ? POCKET_SHADER_SHARP : settings.shader,
                      settings.colors, false);
    oledBlack = settings.colors == POCKET_COLORS_OLED;
    dimCompanion = settings.dimCompanion;
    reduceMotion = settings.reduceMotion;
    background = pocket_background_resolve(settings.background, oledBlack, settings.darkMode);
    wingLook = settings.wingLook;
    uiSize = settings.uiSize;
    pocket_shader_brightness(settings.brightness);
    // DARK MODE and THEME: the core's look follows them (the wings, the strips, the HUD, the companion).
    static int lookKey = -1;
    int lookNow = (settings.darkMode ? 1 : 0) | settings.theme << 1;
    if (lookNow != lookKey) {
      lookKey = lookNow;
      pd_look_apply(&game, &settings);
      hudDrawnKey = 0;
      panelGen++;
    }
    gameScaling = settings.aspect == PD_ASPECT_ORIGINAL || !wideSupported ? settings.scaling : PD_SCALE_INTEGER;
    events();
    audio_check();
    repeat_directions();
    // plorpOS's hotkeys, over the game only.
    unsigned hkFired = 0, hkOn = 0;
    hotkeyMask = 0;
    if (!companion && !quickOpen) {
      hkOn = hotkeys_held(&hkFired);
      stick_hotkeys();
    }
    int zoomStep = host_action(HA_ZOOM_OUT) ? 1 : host_action(HA_ZOOM_IN) ? -1 : 0;
    if (zoomStep) {
      if (!wideSupported)
        notice("This game doesn't zoom");
      else {
        // ASPECT ORIGINAL has nothing to zoom: the first press turns EXPAND on (and keeps it).
        if (settings.aspect == PD_ASPECT_ORIGINAL) {
          settings.aspect = PD_ASPECT_EXPAND;
          settings.aspectPicked = true;
          settings_put(&settings, config);
        }
        int to = wideCam.level + zoomStep;
        to = to < 0 ? 0 : to >= wideLevelCount ? wideLevelCount - 1 : to;
        static char zoomText[32];
        snprintf(zoomText, sizeof zoomText, "Zoom %d%%", zoom_percent(to));
        if (wideCam.level != to)
          zoomChangedAt = SDL_GetTicks();
        wideCam.level = to;
        notice(zoomText);
      }
    }
    if (host_action(HA_SAVE_STATE))
      hkFired |= 1u << HK_SAVE;
    if (host_action(HA_LOAD_STATE))
      hkFired |= 1u << HK_LOAD;
    if (host_action(HA_FF)) {
      ff = !ff;
      notice(ff ? "Fast-forward on" : "Fast-forward off");
    }
    hostActions &= ~((1u << HA_DS_LAYOUT_NEXT) | (1u << HA_DS_LAYOUT_BACK) | (1u << HA_SWAP_SCREENS)); // DS only
    if (hkFired & (1u << HK_SAVE))
      notice(user_state(c, ss, true) ? "State saved" : "State save failed");
    if (hkFired & (1u << HK_LOAD))
      notice(!pocket_exists(ss) ? "No state saved yet" : user_state(c, ss, false) ? "State loaded" : "State load failed");
    bool ffHeld = hkOn & (1u << HK_FF), rewinding = ((hkOn & (1u << HK_REWIND)) || rewindHeld || rewindKey) && rewindOn;
    // R3 (its usual use): a tap switches between WINGS and BATTLE WINGS (from any other view: WINGS), a hold opens
    // every view to pick from (A or a tap picks, B leaves it as it was). The view's name shows either way.
    static uint32_t r3Since;
    static bool r3Picked;
    if (!quickOpen && !companion && !hotkeyMap[PD_HOTKEY_R3]) {
      bool r3 = held & (1u << R3);
      if (down(R3)) {
        r3Since = SDL_GetTicks();
        r3Picked = false;
      }
      if (r3 && r3Since && !r3Picked && !stickChord && SDL_GetTicks() - r3Since >= R3_HOLD_MS) {
        r3Picked = true;
        c->setKeys(c, 0);
        int v = layout_pick("PICK A VIEW", companion_layouts(), POCKET_VIEWS, settings.companionView);
        if (v >= 0) {
          settings.companionView = v;
          settings_put(&settings, config);
          notice(pd_companion_view_name(v));
        }
        if (audio)
          SDL_ClearQueuedAudio(audio);
      } else if (!r3 && r3Since) {
        if (!r3Picked && !stickChord) {
          settings.companionView =
              settings.companionView == POCKET_VIEW_WINGS ? POCKET_VIEW_BATTLE_WINGS : POCKET_VIEW_WINGS;
          settings_put(&settings, config);
          notice(pd_companion_view_name(settings.companionView));
        }
        r3Since = 0;
      }
    }
    if (down(X) || host_action(HA_COMPANION)) {
      menuOpen = 0;
      companion = !companion;
      c->setKeys(c, 0);
      if (audio)
        SDL_ClearQueuedAudio(audio);
    }
    // A click on a wing opens the companion on that wing's page: the party's on PARTY, the map's on MAP (BATTLE in
    // a battle).
    // A tap on a HUD target (a party slot, the minimap, the balls, PC, BAG, WILD, COMPANION) presses it, as the HUD
    // hotkey's A does.
    int hudTapped = -1;
    if (wingClicked && !companion && !menuOpen && !quickOpen && hudShown) {
      hudTapped = hud_target_at(wingClickX, wingClickY);
      if (hudTapped >= 0)
        wingClicked = false;
    }
    // SAVE STATE and CLOSE GAME under the right wing's map: a state at once, or the CLOSE GAME question (NO picked
    // first; B, Esc or NO go back to the game).
    if (wingClicked && !companion && !menuOpen && !quickOpen) {
      int button = wing_button_at(wingClickX, wingClickY);
      if (button == PD_WING_SAVE_STATE)
        notice(user_state(c, ss, true) ? "State saved" : "State save failed");
      else if (button == PD_WING_CLOSE_GAME) {
        companion = true;
        menuOpen = 3;
        wingClose = true;
        confirmMenu.title = "Close game? Unsaved progress since your last save is lost.";
        confirmMenu.selected = 0;
        c->setKeys(c, 0);
        if (audio)
          SDL_ClearQueuedAudio(audio);
      }
      if (button >= 0)
        wingClicked = false;
    }
    if (wingClicked && !companion && !menuOpen && !quickOpen) {
      int tab = wing_tab_at(&viewShown, wingClickX, wingClickY, snap.inBattle);
      if (tab >= 0) {
        pd_ui_update(&ui, &snap); // a battle's start puts BATTLE first: before the page picked, not over it
        ui.tab = tab;
        if (tab == PD_TAB_MAP)
          ui.mapPage = -1;
        companion = true;
        c->setKeys(c, 0);
        if (audio)
          SDL_ClearQueuedAudio(audio);
      }
    }
    wingClicked = false;
    if ((!hostMenu && down(MENU)) || host_action(HA_MENU) || ((down(START) || down(SELECT)) &&
                       (held & (1u << START)) && (held & (1u << SELECT)))) {
      // A second press closes the menu back to the game, like B and RESUME GAME do (it used to leave the
      // companion open behind it).
      companion = !menuOpen;
      menuOpen = menuOpen ? 0 : 1;
      c->setKeys(c, 0);
      if (audio)
        SDL_ClearQueuedAudio(audio);
    }
    // The HUD hotkey (plorpos-l22.4): the cursor goes onto the HUD, the D-pad moves it, A presses what it is on
    // (a slot -> its summary, the minimap / MAP -> the map, the balls / REPEL STEPS / BAG -> the bag, PC -> the
    // game's PC, WILD -> GUIDE's HERE, COMPANION), B / MENU / the hotkey again leave.
    if (hudFocus && (companion || quickOpen || !hudShown)) {
      hudFocus = false;
      hudState.focus = -1;
    }
    if ((hkFired & (1u << HK_HUD)) && !companion && !quickOpen) {
      if (hudFocus || !hudShown || !hudHits.n) {
        if (!hudFocus)
          notice("No HUD on screen");
        hudFocus = false;
        hudState.focus = -1;
      } else {
        hudFocus = true;
        hudState.focus = hud_first(&hudHits);
        c->setKeys(c, 0);
      }
    } else if (hudFocus || hudTapped >= 0) {
      if (hudTapped < 0 && (down(B) || down(MENU))) {
        hudFocus = false;
        hudState.focus = -1;
      } else if (hudTapped >= 0 || down(A)) {
        int id = hudTapped >= 0 ? hudTapped : hudState.focus;
        pd_ui_update(&ui, &snap);
        hudFocus = false;
        hudState.focus = -1;
        if (id == HUD_PC) {
          job = JOB_PC;
          jobFrames = 0;
        } else {
          if (id >= HUD_SLOT && id < HUD_SLOT + PD_PARTY_SIZE) {
            ui.tab = PD_TAB_PARTY;
            ui.summarySlot = id - HUD_SLOT;
          } else if (id == HUD_MAP || id == HUD_MAP_BUTTON) {
            ui.tab = PD_TAB_MAP;
            ui.mapPage = -1;
          } else if (id == HUD_BALLS || id == HUD_REPEL || id == HUD_BAG) {
            ui.tab = PD_TAB_BAG;
          } else if (id == HUD_WILD) {
            ui.tab = PD_TAB_GUIDE;
            ui.guidePage = 0;
            ui.guideOpen = -1;
          }
          companion = true;
          c->setKeys(c, 0);
          if (audio)
            SDL_ClearQueuedAudio(audio);
        }
      } else {
        int dir = down(UP) ? UP : down(DOWN) ? DOWN : down(LEFT) ? LEFT : down(RIGHT) ? RIGHT : -1;
        if (dir >= 0)
          hudState.focus = hud_step(&hudHits, hudState.focus, (dir == RIGHT) - (dir == LEFT), (dir == DOWN) - (dir == UP));
      }
    }
    // Y: the QUICK MENU over the running game (its keys withheld meanwhile).
    // In the app's order (QuickAction): PC, FLY, REPEL, BIKE, WILD, SAVE / LOAD STATE, FF, COMPANION; each only
    // where the game can, dimmed with why when not now.
    struct quick_item quick[QUICK_COUNT];
    int quickN = 0;
    char repelWhy[32] = "";
    if (pcOk)
      quick[quickN++] = (struct quick_item){QUICK_PC, false, false, NULL, 0, 0, NULL};
    if (flyCfg) {
      int flyer = fly_flyer(&snap);
      quick[quickN++] = (struct quick_item){QUICK_FLY, false, false,
                                            flyer < 0 ? "No one in your party knows FLY" : NULL,
                                            flyer < 0 ? 0 : snap.party[flyer].species, 0, NULL};
    }
    if (itemsOk) {
      // The strongest kind held for the icon; dimmed while one works (the steps left).
      repelKindCount = 0;
      for (int k = 0; k < REPEL_KINDS; k++)
        if (bag_count(&snap, repel_kinds[k][0]) > 0)
          repelKinds[repelKindCount++] = repel_kinds[k][0];
      int left = repel_steps(c, game.cfg);
      if (left)
        snprintf(repelWhy, sizeof repelWhy, "%d steps left", left);
      quick[quickN++] = (struct quick_item){QUICK_REPEL, false, false,
                                            left ? repelWhy : repelKindCount ? NULL : "No REPEL in your bag", 0,
                                            repelKindCount ? repelKinds[repelKindCount - 1] : repel_kinds[0][0], NULL};
    }
    if (bikeOk) {
      int bike = bag_bike(&snap);
      bool riding = (fieldFlags & PK_FIELD_AVATAR) && (fieldFlags & PK_FIELD_BIKE);
      quick[quickN++] = (struct quick_item){QUICK_BIKE, false, false, bike ? NULL : "No bike in your bag", 0,
                                            bike ? bike : 360, riding ? "GET OFF" : NULL};
    }
    quick[quickN++] = (struct quick_item){QUICK_WILD, settings.wildOff, false,
                                          wildSwitch ? NULL : "Not in this game", 0, 0, NULL};
    quick[quickN++] = (struct quick_item){QUICK_SAVE_STATE, false, false, NULL, 0, 0, NULL};
    quick[quickN++] = (struct quick_item){QUICK_LOAD_STATE, false, false,
                                          pocket_exists(ss) ? NULL : "No state saved yet", 0, 0, NULL};
    quick[quickN++] = (struct quick_item){QUICK_FF, false, ff, NULL, 0, 0, NULL};
    quick[quickN++] = (struct quick_item){QUICK_COMPANION, false, false, NULL, 0, 0, NULL};
    if (quickSel >= quickN)
      quickSel = 0;
    quick_set_grid(settings.quickGrid);
    if (down(Y) && !companion) {
      quickOpen = !quickOpen;
      flyList = repelList = false;
      flyAsk = 0;
      c->setKeys(c, 0);
    } else if (quickOpen && repelList) {
      // REPEL's kinds in the bag: UP / DOWN, A uses one, B back to the ring.
      if (down(UP))
        repelSel = (repelSel + repelKindCount - 1) % repelKindCount;
      if (down(DOWN))
        repelSel = (repelSel + 1) % repelKindCount;
      if (down(B))
        repelList = false;
      else if (down(MENU))
        quickOpen = repelList = false;
      else if (down(A)) {
        quickOpen = repelList = false;
        job = JOB_REPEL;
        jobItem = repelKinds[repelSel];
        jobFrames = 0;
      }
    } else if (quickOpen && flyList) {
      // FLY's places: UP / DOWN, A flies, B back to the ring.
      if (down(UP) && flyT.count)
        flySel = (flySel + flyT.count - 1) % flyT.count;
      if (down(DOWN) && flyT.count)
        flySel = (flySel + 1) % flyT.count;
      if (down(B))
        flyList = false;
      else if (down(MENU))
        quickOpen = flyList = false;
      else if (down(A) && flyT.count) {
        quickOpen = flyList = false;
        flyMapsec = flyT.towns[flySel];
        flyStage = 1;
        flyWait = 0;
      }
    } else if (quickOpen && (down(B) || down(MENU))) {
      quickOpen = false;
    } else if (quickOpen) {
      int dx = down(RIGHT) - down(LEFT), dy = down(DOWN) - down(UP);
      if (dx || dy)
        quickSel = quick_step(quickN, quickSel, dx, dy, canvasW, canvasH);
      if (down(A) && !quick[quickSel].why && quick[quickSel].action == QUICK_FLY) {
        flyAsk = 1; // answered below, between two frames
      } else if (down(A) && !quick[quickSel].why && quick[quickSel].action == QUICK_REPEL && repelKindCount > 1) {
        // More than one kind: their list (QuickMenu.kt's RepelKinds), each with how many and its steps.
        for (int k = 0; k < repelKindCount; k++) {
          int steps = 0;
          for (int r = 0; r < REPEL_KINDS; r++)
            if (repel_kinds[r][0] == repelKinds[k])
              steps = repel_kinds[r][1];
          snprintf(repelRows[k], sizeof repelRows[k], "%s x%d  %d steps", pd_item_name(&game, repelKinds[k]),
                   bag_count(&snap, repelKinds[k]), steps);
          repelNames[k] = repelRows[k];
        }
        repelList = true;
        repelSel = repelKindCount - 1;
      } else if (down(A) && !quick[quickSel].why) {
        quickOpen = false;
        switch (quick[quickSel].action) {
        case QUICK_PC:
          job = JOB_PC;
          jobFrames = 0;
          break;
        case QUICK_REPEL:
          job = JOB_REPEL;
          jobItem = repelKinds[0];
          jobFrames = 0;
          break;
        case QUICK_BIKE:
          job = JOB_BIKE;
          jobItem = bag_bike(&snap);
          jobFrames = 0;
          jobTries = 15; // PokeDaisyActivity's SELECT_TRIES
          break;
        case QUICK_WILD:
          settings.wildOff = !settings.wildOff;
          settings_put(&settings, config);
          notice(settings.wildOff ? "Wild encounters off" : "Wild encounters on");
          break;
        case QUICK_SAVE_STATE:
          notice(user_state(c, ss, true) ? "State saved" : "State save failed");
          break;
        case QUICK_LOAD_STATE:
          notice(user_state(c, ss, false) ? "State loaded" : "State load failed");
          break;
        case QUICK_FF:
          ff = !ff;
          break;
        case QUICK_COMPANION:
          companion = true;
          if (audio)
            SDL_ClearQueuedAudio(audio);
          break;
        default:
          break;
        }
      }
    }
    if (companion) {
      companion_canvas();
      if (menuOpen) {
        if (menuOpen == 4) {
          if (down(B) || down(A))
            menuOpen = 1;
        } else {
          struct pd_menu *pm = menuOpen == 1 ? &gameMenu : menuOpen == 6 ? &resumeMenu : menuOpen == 7 ? &raMenu : &confirmMenu;
          int tapped = menu_taps(pm);
          if (tapped >= 0 && tapped < pm->count) {
            pm->selected = tapped;
            pressed |= 1u << A;
          }
          if (down(UP))
            pd_menu_move(pm, -1);
          if (down(DOWN))
            pd_menu_move(pm, 1);
          if (menuOpen == 7 && (down(A) || down(B))) {
            // A: the achievement's description; B: back to the GAME menu.
            if (down(A) && pd_menu_pickable(&raMenu, raMenu.selected) && raDesc[raMenu.selected][0])
              notice(raDesc[raMenu.selected]);
            else if (down(B))
              menuOpen = 1;
          } else if (menuOpen == 6 && (down(A) || down(B))) {
            // CONTINUE loads the state left last time; B or START FROM SAVE plays on from the save's boot.
            if (down(A) && resumeMenu.selected == 0)
              notice(state(c, resumePath, false) ? "Continued where you left off" : "Couldn't continue; started from the save");
            menuOpen = 0;
            companion = false;
          } else if (down(B)) {
            if (menuOpen == 1 || wingClose) {
              menuOpen = 0;
              companion = false;
              wingClose = false;
            } else
              menuOpen = 1;
          } else if (down(A)) {
            if (menuOpen == 1) {
              switch (gameMenu.selected) {
              case 0:
                menuOpen = 0;
                companion = false;
                break;
              case 1:
                menuOpen = 0;
                ui.tab = PD_TAB_PARTY;
                break;
              case 2:
                menuOpen = 5;
                confirmMenu.title = "Overwrite save state?";
                confirmMenu.selected = 0;
                break;
              case 3:
                menuOpen = 2;
                confirmMenu.title = "Load state? Current progress will rewind.";
                confirmMenu.selected = 0;
                break;
              case 4:
                ff = !ff;
                notice(ff ? "Fast-forward on" : "Fast-forward off");
                break;
              case 5:
                fullscreen_toggle();
                break;
              case 6:
                menuOpen = 3;
                confirmMenu.title = "Quit? Save in-game first.";
                confirmMenu.selected = 0;
                break;
              case 7: {
                int rn = ra_rows(raRows, raDesc, 400);
                if (!rn) {
                  notice(ra_login_state() != RA_LOGGED_IN ? "Sign in to RetroAchievements in SETTINGS first"
                                                          : "No achievements for this game");
                  break;
                }
                for (int r = 0; r < rn; r++)
                  raItems[r] = raRows[r];
                ra_summary(raTitle, sizeof raTitle);
                pd_menu_init(&raMenu, raTitle, raItems, rn);
                raMenu.selected = 0;
                pd_menu_move(&raMenu, 1); // row 0 is a group title
                menuOpen = 7;
                break;
              }
              case 8:
                menuOpen = 4;
                break;
              }
            } else {
              if (confirmMenu.selected == 1) {
                if (menuOpen == 3)
                  leave = true;
                else if (menuOpen == 5)
                  notice(user_state(c, ss, true) ? "State saved"
                                            : "State save failed");
                else
                  notice(user_state(c, ss, false) ? "State loaded"
                                             : "State load failed");
              }
              menuOpen = 1;
              if (wingClose) { // asked from the wing: NO goes back to the game
                menuOpen = 0;
                companion = false;
                wingClose = false;
              }
            }
          }
        }
      } else {
        if (down(B) && !map_cursor_back(&ui) && !pd_ui_back(&ui))
          companion = false;
        if (down(L3) && settings.companionView == PD_VIEW_SIDE_PANEL) {
          settings.panelLocked = !settings.panelLocked;
          notice(settings.panelLocked ? "Side panel locked" : "Side panel unlocked");
          settings_put(&settings, config);
        }
        if (down(L) || down(R)) {
          pd_ui_next_tab(&ui, down(R) ? 1 : -1);
        }
        for (int k = RIGHT; k <= DOWN; k++)
          if (down(k))
            focus_move(&ui, k);
        if (down(A))
          focus_activate(&ui);
        for (int i = 0; i < tapCount; i++)
          pd_ui_touch(&ui, taps[i].x, taps[i].y, taps[i].down);
        tapCount = 0;
        switch (pd_ui_take_action(&ui)) {
        case PD_ACTION_LEAVE_GAME:
          leave = true;
          break;
        case PD_ACTION_TRADE_EVOLVE:
          // The game's evolution plays on the field: the companion closes and the job waits for a safe frame.
          if (ui.summarySlot >= 0 && ui.summarySlot < 6 && host.evolveTarget[ui.summarySlot] > 0) {
            evolveSlot = ui.summarySlot;
            job = JOB_EVOLVE;
            jobFrames = 0;
            pd_ui_back(&ui);
            companion = false;
          }
          break;
        case PD_ACTION_TOGGLE_FF:
          ff = !ff;
          break;
        case PD_ACTION_SAVE_STATE:
          notice(user_state(c, ss, true) ? "State saved" : "State save failed");
          break;
        case PD_ACTION_LOAD_STATE:
          notice(user_state(c, ss, false) ? "State loaded" : "State load failed");
          break;
        case PD_ACTION_UNDO_SAVE:
          notice(undo_save() ? "Save undone: the slot is as it was" : "Nothing to undo");
          break;
        case PD_ACTION_UNDO_LOAD:
          notice(undo_load(c) ? "Load undone" : "Nothing to undo");
          break;
        case PD_ACTION_STATE_SLOT:
          // The picked slot is where SAVE / LOAD and their hotkeys go from now on.
          host.slotSelected = ui.actionSlot;
          slot_path(ss, sizeof ss, root, name, host.slotSelected);
          break;
        case PD_ACTION_RESTART_GAME:
          c->reset(c);
          ra_reset();
          notice("Game restarted");
          break;
        case PD_ACTION_PER_GAME:
          if (!host.perGame) { // from now on this game's changes stay its own, starting from what it has now
            char dir[1100];
            snprintf(dir, sizeof dir, "%s/game-settings", root);
            pocket_mkdir(dir);
            snprintf(gameConfig, sizeof gameConfig, "%s/game-settings/%s.ini", root, name);
            settings_put(&settings, config);
            host.perGame = pocket_exists(gameConfig);
            if (!host.perGame)
              gameConfig[0] = 0;
            notice(host.perGame ? "This game keeps its own settings now" : "Couldn't keep this game's settings");
          } else { // back to the usual settings, as they are
            pocket_remove(gameConfig);
            gameConfig[0] = 0;
            host.perGame = false;
            struct pd_settings global;
            pd_settings_load(&global, config);
            settings = global;
            if (wildSwitch)
              pk_field_wild(wildSwitch, settings.wildOff);
            rewind_follow(settings.rewind || rewindBound || rewind_hotkey());
            notice("This game uses your usual settings again");
          }
          break;
        case PD_ACTION_SETTINGS_CHANGED:
          rewind_follow(settings.rewind || rewindBound || rewind_hotkey());
          if ((settings.fullscreen == 1) != fullscreenOn)
            fullscreen_set(settings.fullscreen == 1);
          settings.fullscreen = fullscreenOn ? 1 : 0;
          buttonLayout = settings.buttonLayout;
          orientation_set(settings.orientation);
          gameTouch = settings.gameTouch;
          settings_put(&settings, config);
          break;
        default:
          break;
        }
      }
      if (down(L2)) {
        menuOpen = 5;
        confirmMenu.title = "Overwrite save state?";
        confirmMenu.selected = 0;
      }
      if (down(R2)) {
        menuOpen = 2;
        confirmMenu.title = "Load state? Current progress will rewind.";
        confirmMenu.selected = 0;
      }
    } else {
      bool fast = (ff || ffHeld) && !rewinding &&
                  (settings.ffNormal || !smart_slows(c, &game, snap.inBattle || (fieldFlags & PK_FIELD_BATTLE)));
      if (fieldFlags & PK_FIELD_AVATAR)
        host.sceneKnown[PD_SCENE_BIKE] = host.sceneKnown[PD_SCENE_SURF] = true;
      double pace = fast || rewinding ? 1 : scene_pace(&settings, &host, scene_now(fieldFlags, fieldKnown, &snap));
      // FF MUSIC: STEADY plays the song's recording while the game races (silent until it's made); SPED-UP the game's
      // own sound; OFF nothing. A song STEADY can't record plays sped up.
      bool racing = (fast || pace > 1) && !rewinding;
      bool steady = ffMusic && settings.ffAudio && settings.ffSteady;
      uint32_t key = ffMusic ? ffm_key(c) : 0;
      if (key != songKey) {
        songKey = key;
        songAt = SDL_GetTicks();
      }
      enum ffm_state songState = steady && key ? ffm_state(ffMusic, key) : FFM_NEVER;
      if (!steady)
        songAsked = 0;
      else if (key && (key != songAsked || songState == FFM_COMING)) {
        songAsked = key;
        ffm_request(ffMusic, key);
      }
      bool useClip = racing && songState == FFM_READY;
      if (useClip && ffClip.key != key) {
        ffm_clip_free(&ffClip);
        clipOn = false;
        useClip = ffm_load(ffMusic, key, &ffClip);
      }
      if (useClip != clipOn) {
        // Where the song is by now: from the top for one just started, further on otherwise.
        ffClip.at = (uint64_t)(SDL_GetTicks() - songAt) * FFM_RATE / 1000;
        clipOn = useClip;
        if (audio)
          SDL_ClearQueuedAudio(audio);
      }
      muted = rewinding || clipOn || (racing && songState == FFM_COMING) || (racing && !settings.ffAudio);
      uint32_t playerKeys = (quickOpen || hudFocus) ? 0 : held & ~hotkeyMask & 1023;
      // AUTO-RUN: B flipped while walking on the player's own field (holding it walks), as on the 3DS and Android -
      // never in a script, a battle, a menu or on a bike (the Acro Bike's wheelie is B).
      if (settings.autoRun && gameKeyFrames <= 0 && (playerKeys & 0xF0) && !(playerKeys & 1) &&
          (fieldFlags & PK_FIELD_KNOWN) && !(fieldFlags & (PK_FIELD_LOCKED | PK_FIELD_BATTLE | PK_FIELD_BIKE)) &&
          !snap.inBattle && (!wideSupported || pk_wide_scene() == PK_WIDE_SCENE_FIELD))
        playerKeys ^= 2;
      c->setKeys(c, playerKeys | (gameKeyFrames > 0 ? gameKeys : 0));
      int count = 1;
      if (fast) {
        count = ff_frames(&settings, hz, frameCost);
      } else {
        // GAME SPEED / SCENE SPEEDS: whole frames this refresh, the fraction carried (1.5x: 1, 2, 1, 2 ...).
        paceAcc += pace;
        count = (int)paceAcc;
        paceAcc -= count;
      }
      if (saving && count > 3)
        count = 3;
      // GAME SPEED's sound is stretched to its pitch; fast-forward's plays sped up, as FF MUSIC says.
      stretchSpeed = fast || rewinding ? 1 : pace;
      audio_rate(c, hz, fast ? (settings.ffSpeed ? settings.ffSpeed / 100.0 : count) : stretchSpeed != 1 ? 1 : pace);
      if ((settings.instantText && textPatch.count > 0) != textOn) {
        textOn = !textOn;
        tweak_apply(c, romdata, &textPatch, textOn);
      }
      if ((settings.reusableTms && tmPatch.count > 0 && !host.reusableTmsBuiltIn) != tmsOn) {
        tmsOn = !tmsOn;
        tweak_apply(c, romdata, &tmPatch, tmsOn);
      }
      ra_set_paused(shinyOk && settings.shinyOdds > 0);
      uint64_t runStart = SDL_GetPerformanceCounter();
      for (int i = 0; i < count; i++) {
        uint64_t t = SDL_GetPerformanceCounter();
        // REWIND: back one entry, then that frame runs to draw it; at the oldest, stand still.
        if (rewinding && !pk_rewind_restore(&rewindRing, c))
          break;
        if (rewindOn && !rewinding && ++rewindTick >= 2) {
          rewindTick = 0;
          pk_rewind_append(&rewindRing, c);
        }
        c->runFrame(c);
        ra_frame();
        if (gameKeyFrames > 0 && --gameKeyFrames == 0)
          c->setKeys(c, (quickOpen || hudFocus) ? 0 : held & ~hotkeyMask & 1023);
        if (c->platform(c) == mPLATFORM_GBA)
          pk_rest_note(c);
        if (shinyOk && settings.shinyOdds > 0 && shiny_tick(&shinyRoller, SHINY_ODDS[settings.shinyOdds], c))
          fprintf(stderr, "shiny odds: foe made shiny (1 in %d)\n", SHINY_ODDS[settings.shinyOdds]);
        if (fieldKnown)
          fieldFlags = pk_field_state(c, settings.runIndoors, wildSwitch && settings.wildOff ? 0 : 100);
        if (host.dayNightFits) {
          // The clock's tint outdoors on the field (text boxes keep their own colours); none in a battle or a menu,
          // eased in and out. The clock is read every ten seconds.
          int scene = wideSupported ? pk_wide_scene() : PK_WIDE_SCENE_FIELD;
          bool onField = !snap.inBattle && !(fieldFlags & PK_FIELD_BATTLE) &&
                         day_night_outdoors(snap.valid ? snap.mapType : -1) &&
                         (scene == PK_WIDE_SCENE_FIELD || scene == PK_WIDE_SCENE_TEXT ||
                          scene == PK_WIDE_SCENE_OVERLAY || scene == PK_WIDE_SCENE_FADE);
          if (dayNightClock == 0 || (uint32_t)frames - dayNightClock >= 600) {
            time_t now = time(NULL);
            struct tm lt = *localtime(&now);
            const char *fakeHour = getenv("POCKET_HOUR"); // headless checks: the hour to draw
            day_night_at(fakeHour ? (float)atof(fakeHour) : (float)lt.tm_hour + (float)lt.tm_min / 60.0f,
                         dayNightClockTint);
            dayNightClock = (uint32_t)frames | 1;
          }
          for (int k = 0; k < 3; k++)
            dayNightTarget[k] = settings.dayNight && onField ? dayNightClockTint[k] : 256;
          if (day_night_step(&dayNight, dayNightTarget))
            pk_tint_set(c, dayNight.cur[0], dayNight.cur[1], dayNight.cur[2]);
        }
        emuMs += ms_since(t);
        t = SDL_GetPerformanceCounter();
        wide_tick(c, i < count - 1, &settings);
        wideMs += ms_since(t);
      }
      if (fast)
        frameCost = frameCost * 0.8 + 0.2 * ms_since(runStart) / count;
      if (clipOn && audio) {
        int16_t clipBuf[1024 * 2];
        while (SDL_GetQueuedAudioSize(audio) < FFM_RATE * 4 / 10) {
          ffm_clip_fill(&ffClip, clipBuf, 1024);
          game_gain(clipBuf, 1024 * 2);
          SDL_QueueAudio(audio, clipBuf, sizeof clipBuf);
        }
      }
    }
    // The companion's data: fresh when it opens (it pauses the game), else twice a
    // second - read through the bus byte by byte, every frame cost frame time.
    if (!snap.valid || (companion && !companionWas) || (!companion && frames % 30 == 0)) {
      pd_snapshot_read(&snap, &game, read_memory, c);
      panelGen++;
      if (nuzlocke.on && nuzlocke.supported && snap.valid) {
        struct pd_nuz_sample sample;
        pd_nuz_sample_of(&sample, &game, &snap);
        pd_nuz_feed(&nuzlocke, &sample);
        for (int i = 0; i < nuzlocke.eventCount; i++)
          notice(nuzlocke.events[i]);
      }
    }
    if (nuzlocke.dirty)
      nuz_store(crc);
    companionWas = companion;
    rumbleOn = settings.rumble;
    rumble_events(&snap, (fieldFlags & PK_FIELD_BATTLE) != 0);
    // FLY's steps, each between two frames the game's own code can be called in (pk_call_safe), as the app's
    // field-item jobs wait for one.
    bool flyBattle = snap.inBattle || (fieldFlags & PK_FIELD_BATTLE);
    bool flyBusy = companion || (fieldFlags & PK_FIELD_LOCKED) ||
                   (wideSupported && pk_wide_scene() != PK_WIDE_SCENE_FIELD);
    if (flyCfg && flyAsk && quickOpen && !companion) {
      if (pk_call_safe(c)) {
        pd_snapshot_read(&snap, &game, read_memory, c);
        fly_targets(c, flyCfg, romdata, (size_t)len, &snap, flyBusy, flyBattle, &flyT);
        flyAsk = 0;
        if (flyT.result == FLY_OK && flyT.count) {
          for (int i = 0; i < flyT.count; i++) {
            const char *name = pd_mapsec_name(&game, flyT.towns[i]);
            flyNames[i] = name && *name ? name : "?";
          }
          flyList = true;
          flySel = 0;
        } else {
          quickOpen = false;
          notice(flyT.result == FLY_OK ? "You haven't been anywhere to FLY to yet." : fly_reason(flyT.result));
        }
      } else if (++flyAsk > 120) {
        flyAsk = 0;
        quickOpen = false;
        notice(fly_reason(FLY_FAILED));
      }
    }
    if (flyCfg && flyStage == 1 && !companion && pk_call_safe(c)) {
      pd_snapshot_read(&snap, &game, read_memory, c);
      enum fly_result r = fly_start(c, flyCfg, romdata, (size_t)len, &snap, flyMapsec, &flyT, flyBusy, flyBattle);
      if (r == FLY_OK) {
        flyStage = 2;
        flyWait = 0;
        flyCallback = fly_main_callback(c, flyCfg);
      } else {
        flyStage = 0;
        notice(fly_reason(r));
      }
    } else if (flyCfg && flyStage == 2 && !companion && ++flyWait >= FLY_FADE_FRAMES && pk_call_safe(c)) {
      flyStage = 0;
      if (!fly_finish(c, flyCfg, romdata, (size_t)len, flyMapsec, &flyT, fly_main_callback(c, flyCfg) != flyCallback))
        notice(fly_reason(FLY_FAILED));
    }
    bool safe = job != JOB_NONE && !companion && pk_call_safe(c);
    if (job == JOB_EVOLVE && safe) {
      job = JOB_NONE;
      enum item_result r = trade_evolve(c, game.cfg, romdata, (size_t)len, evolveSlot, evolvePers[evolveSlot], flyBusy, flyBattle);
      if (r != ITEM_USED)
        notice(item_reason(r));
      for (int i = 0; i < 6; i++)
        host.evolveTarget[i] = 0;
      evolveSign = 0;
    } else if (job == JOB_PC && safe) {
      job = JOB_NONE;
      enum item_result r = pc_open(c, game.cfg, romdata, (size_t)len, flyBusy, flyBattle);
      if (r != ITEM_USED)
        notice(item_reason(r));
    } else if (job == JOB_REPEL && safe) {
      job = JOB_NONE;
      int steps = 0;
      enum item_result r = repel_use(c, game.cfg, romdata, (size_t)len, jobItem, flyBusy, flyBattle, &steps);
      if (r == ITEM_USED) {
        char msg[48];
        snprintf(msg, sizeof msg, "Used! It lasts %d steps.", steps);
        notice(msg);
      } else
        notice(item_reason(r));
    } else if (job == JOB_BIKE && safe) {
      enum item_result r = bike_register(c, game.cfg, romdata, (size_t)len, jobItem, flyBusy, flyBattle, &bikeOld);
      if (r == ITEM_USED) {
        gameKeys = 1u << SELECT;
        gameKeyFrames = 2;
        bikeWasRiding = fieldFlags & PK_FIELD_BIKE;
        bikeTook = false;
        job = JOB_BIKE_WAIT;
        jobFrames = 0;
      } else {
        job = JOB_NONE;
        notice(item_reason(r));
      }
    } else if (job == JOB_BIKE_WAIT && !companion) {
      // SELECT again if the game dropped it (a bike rolling at full speed ignores it), but only where taking it
      // shows for sure: the riding state flips, or the field's controls stay held (PokeDaisyActivity.pressSelect).
      if (((fieldFlags & PK_FIELD_BIKE) != 0) != bikeWasRiding || (fieldFlags & PK_FIELD_LOCKED))
        bikeTook = true;
      if (++jobFrames >= 10) {
        if (bikeTook || !(fieldFlags & PK_FIELD_AVATAR) || --jobTries <= 0) {
          job = JOB_BIKE_RESTORE;
        } else {
          gameKeys = 1u << SELECT;
          gameKeyFrames = 2;
        }
        jobFrames = 0;
      }
    } else if (job == JOB_BIKE_RESTORE && ++jobFrames >= 20) {
      job = JOB_NONE;
      bike_restore(c, game.cfg, jobItem, bikeOld);
    } else if ((job == JOB_PC || job == JOB_REPEL || job == JOB_BIKE || job == JOB_EVOLVE) && !companion && ++jobFrames > 120) {
      job = JOB_NONE;
      notice(item_reason(ITEM_FAILED));
    }
    // TRADE EVOLUTIONS: who'd evolve by trade, asked again when the party changes (not every frame: each slot is a
    // call into the game).
    if (tradeOk && settings.tradeEvolutions && job == JOB_NONE && !companion && !flyBattle && ++evolveTick >= 30) {
      evolveTick = 0;
      uint32_t sign = 2166136261u ^ (uint32_t)snap.partyCount;
      for (int i = 0; i < snap.partyCount && i < 6; i++)
        sign = (sign ^ snap.party[i].personality ^ (uint32_t)snap.party[i].species << 8 ^ (uint32_t)snap.party[i].heldItem << 20) * 16777619u;
      if (sign != evolveSign && pk_call_safe(c) && trade_evo_targets(c, game.cfg, romdata, (size_t)len, host.evolveTarget, evolvePers))
        evolveSign = sign;
    }
    bool viewBattle = snap.inBattle || (fieldFlags & PK_FIELD_BATTLE);
    wingBattleInfo = settings.battleInfo;
    // With the companion open the game stands still, so the view keeps a companion open only while it is drawn.
    view_update(&settings, companion && (!menuOpen || menuOpen == 4), viewBattle, gw, gh);
    gameView = &viewShown;
    if (quickOpen && !companion) {
      companion_canvas();
      const char *const *rows = flyList ? flyNames : repelList ? repelNames : NULL;
      present_quick(&game, quick, quickN, quickSel, flyList ? "FLY" : "REPEL", rows,
                    flyList ? flyT.count : repelKindCount,
                    flyList ? flySel : repelSel, gw, gh);
    } else if (companion && menuOpen && menuOpen != 4) {
      present_menu(menuOpen == 1 ? &gameMenu : menuOpen == 6 ? &resumeMenu : menuOpen == 7 ? &raMenu : &confirmMenu, gw, gh);
    } else if (companion) {
      {
        struct view_rect cr = view_companion(&viewTo);
        if (cr.w > 0 && cr.h > 0)
          companion_canvas_for(cr.w, cr.h);
      }
      if (menuOpen) {
        if (menuOpen == 4) {
          pd_splash_draw(&canvas, "Based on PokeDaisy by Lidor Itzhari",
                         "https://github.com/lidor30/pokedaisy",
                         "GPL-3.0 | mGBA MPL-2.0 | B: Back");
        } else
          pd_menu_draw(menuOpen == 1 ? &gameMenu : menuOpen == 6 ? &resumeMenu : menuOpen == 7 ? &raMenu : &confirmMenu, &canvas);
      } else {
        pd_ui_update(&ui, &snap);
        // The companion draws the focused hit as its own cursor where it has one.
        ui.focusId = focusId;
        host.ffOn = ff;
      gameGain = settings.gameMuted ? 0 : settings.gameVolume * 256 / 100;
      host.undoSave = undoSaveSlot[0] != 0;
      host.undoLoad = undoLoadState[0] != 0;
        host.hasState = pocket_exists(ss);
        if (slotsStale)
          slots_read(&host, root, name);
        pd_ui_draw(&ui, &canvas, &game, &snap, &host);
        map_cursor_sync(&ui, &game, &snap);
        focus_draw(&ui, focus_resolve(&ui));
      }
      present_view_companion(&game, &snap, viewBattle, gw, gh);
    } else {
      bool field = (fieldFlags & PK_FIELD_KNOWN) && !(fieldFlags & (PK_FIELD_LOCKED | PK_FIELD_BATTLE)) &&
                   (!wideSupported || pk_wide_scene() == PK_WIDE_SCENE_FIELD) && flyStage == 0;
      hudFieldFrames = field ? hudFieldFrames + 1 : 0;
      // The bezel bars' facts this frame.
      static unsigned savesSeen;
      if (!gameSavedAt)
        gameSavedAt = SDL_GetTicks();
      if (saveStatus.syncs != savesSeen) {
        savesSeen = saveStatus.syncs;
        bezelSavedAt = gameSavedAt = SDL_GetTicks();
        gameSavedYet = true;
      }
      bezelFf = ff ? settings.ffSpeed : 0;
      bezelPacePct = (int)(scene_pace(&settings, &host, scene_now(fieldFlags, fieldKnown, &snap)) * 100 + 0.5);
      {
        static uint32_t savedSeen;
        struct wing_toast_in in = {.valid = snap.valid && snap.partyCount > 0, .battle = viewBattle,
                                   .saved = bezelSavedAt != savedSeen && wingToasts.primed, .mapsec = snap.mapsec,
                                   .place = snap.valid ? pd_mapsec_name(&game, snap.mapsec) : NULL,
                                   .repel = hudRepelSteps, .partyCount = snap.partyCount};
        savedSeen = bezelSavedAt; // a save while the game loads (before a party is read) is no news
        for (int i = 0; i < snap.partyCount && i < WING_TOAST_PARTY; i++) {
          const struct pd_mon *m = &snap.party[i];
          in.party[i].key = m->personality ^ (uint32_t)m->species;
          in.party[i].level = m->species == PD_SPECIES_EGG ? 0 : m->level;
          if (m->nickname[0])
            snprintf(in.party[i].name, sizeof in.party[i].name, "%s", m->nickname);
          else
            pd_species_name(&game, m->species, in.party[i].name, sizeof in.party[i].name);
        }
        wing_toast_watch(&wingToasts, &in, SDL_GetTicks());
      }
      bezelStyle = bezel_style_resolve(settings.bezelStyle, pocket_backdrop_for(background).cards);
      memcpy(bezelSlots, settings.bezelSlots, sizeof bezelSlots);
      for (int b = 0; b < 4; b++)
        bezelCount[b] = bar_list_of(&settings, b, bezelLists[b]);
      wingSectionCount = wing_list_of(&settings, wingSections);
      battleSectionCount = battle_list_of(&settings, battleSections);
      pd_ui_foe_ivs = settings.foeIvs;
      pd_ui_party_types = settings.partyTypes;
      if (game.cfg && c->platform(c) == mPLATFORM_GBA && (frames & 15) == 0) {
        int steps = repel_steps(c, game.cfg);
        hudRepelSteps = steps > 0 ? steps : 0;
      }
      if (view_has_panels(&viewShown)) {
        companion_canvas();
        present_view_game(&game, &snap, viewBattle, gw, gh);
      } else if (settings.hud && viewShown.hud && game.cfg && c->platform(c) == mPLATFORM_GBA && hudFieldFrames >= 20) {
        int steps = repel_steps(c, game.cfg);
        if (steps <= 0)
          hudState.repelFull = 0;
        else if (!hudState.repelFull || steps > hudState.repelFull)
          hudState.repelFull = hud_repel_full(steps);
        hudState.repel = steps > 0 ? steps : 0;
        hudRepelSteps = hudState.repel;
        hudState.pc = pcOk;
        hudState.wild = hud_wild_uncaught(&game, &snap);
        companion_canvas();
        present_hud_game(&game, &snap, &hudState, &settings, gw, gh);
      } else {
        present_bars_game(&game, &snap, viewBattle, gw, gh);
      }
    }
#ifdef POCKET_TEST
    test_frame(c, companion, frames, &ui, &snap);
#endif
    frames++;
    if (maxFrames && frames >= maxFrames) {
      if (shot) {
        unsigned lit = 0;
        if (!capture_frame(shot, &lit))
          notice("Capture failed");
        fprintf(stderr,
                "Capture frame %u: %ux%u game, rect %d,%d %dx%d, lit=%u\n",
                c->frameCounter(c), gw, gh, lastGameRect.x, lastGameRect.y,
                lastGameRect.w, lastGameRect.h, lit);
      }
      break;
    }
    perf_frame(frameStart, hz);
    uint64_t now = SDL_GetPerformanceCounter();
    pacing_check((double)(now - lastFrame) * 1000.0 / SDL_GetPerformanceFrequency());
    lastFrame = now;
    if (saving && companion && !maxFrames)
      SDL_Delay(16); // the game waits under the companion: half the redraws
    if (vsyncPaced) {
      due = now; // the present waited for the refresh: that's the pace
      continue;
    }
    due += (uint64_t)(SDL_GetPerformanceFrequency() / hz);
    if (now < due)
      SDL_Delay((Uint32)((due - now) * 1000 / SDL_GetPerformanceFrequency()));
    else if (now - due > SDL_GetPerformanceFrequency() / 5)
      due = now;
  }
  pk_finish_save(c, PK_FINISH_SAVE_FRAMES, NULL); // a save the game is writing finishes before the core stops
  // RESUME GAMES: where the game was left, for CONTINUE next time (with RESUME GAMES off, a stale one goes).
  if (settings.resumeGames && menuOpen != 6) {
    if (!state(c, resumePath, true))
      fprintf(stderr, "pokedaisy: resume state not written\n");
  } else if (!settings.resumeGames)
    pocket_remove(resumePath);
  if (audio)
    SDL_ClearQueuedAudio(audio);
cleanup:
  ffm_clip_free(&ffClip);
  nuz_store(crc);
  ffm_stop(ffMusic);
  rewind_follow(false);
  rumble(0, 0, 0); // a rumble cart left buzzing stops with the game
  gameConfig[0] = 0; // the game list writes settings.ini again
  pk_wide_configure(0);
  pk_wide_extras_reset(); // DAY AND NIGHT's tint goes with the game
  wideSupported = wideReady = false;
  ra_unload_game();
  c->unloadROM(c);
  if(saveStatus.failed)appError=1;
  mCoreConfigDeinit(&c->config);
  c->deinit(c);
  free(romdata);
  held = pressed = 0;
}
#ifndef POCKET_VERSION
#define POCKET_VERSION "dev"
#endif
/* The update prompt over the game list (updater.h), driven by the pad: UPDATE NOW / LATER / SKIP THIS VERSION, then
 * the download's progress and its outcome. True while it's up: it takes the keys. */
static int updateSel;
static bool updateDone; // READY / FAILED read and closed
/* The game list's own screens (GAME FOLDERS, the folder browser) run their frames here. A headless run (--frames)
 * ends inside them too and takes its shot there; POCKET_MENU_KEYS ("D D A B": A, B, U(p), D(own), L(eft), R(ight), one every 12
 * frames, a space skips a turn) presses keys for it, so the screens can be pictured without a person. */
static int menuFrames, menuTicks;
static const char *menuShot, *menuKeys;
static bool menu_frame(void) {
  if (!running)
    return true; // a screen inside already ended the run (and took its shot)
  bool last = menuFrames && menuTicks + 1 >= menuFrames;
  if (last && menuShot && gridOverlay)
    presentShot = menuShot; // the box art is drawn over the canvas: the window, read before it's shown
  present();
  if (menuFrames && ++menuTicks >= menuFrames) {
    // The canvas itself: a read-back after the present can be the frame before.
    if (menuShot && !gridOverlay && !pd_png_write(menuShot, canvasW, canvasH, pixels))
      notice("Capture failed");
    running = 0;
    return true;
  }
  SDL_Delay(saver_active() ? 33 : 16);
  return false;
}
static void menu_script(void) {
  static int last = -1;
  if (!menuKeys || !*menuKeys || menuTicks % 12 || menuTicks == last)
    return;
  last = menuTicks;
  char c = *menuKeys++;
  int k = c == 'A' ? A : c == 'B' ? B : c == 'U' ? UP : c == 'D' ? DOWN : c == 'L' ? LEFT : c == 'R' ? RIGHT
          : c == 'X' ? X : c == 'Y' ? Y : -1;
  if (k >= 0)
    pressed |= 1u << k;
}
/* A title that fits the window: a long path keeps its end ("...\Emulation\roms\gba"). */
static const char *menu_title(const char *title, char *buf, size_t size) {
  const char *t = title;
  while (*t && pd_text_width(t) > canvas.w - 40) {
    t++;
    snprintf(buf, size, "...%s", t);
    if (pd_text_width(buf) <= canvas.w - 40)
      return buf;
  }
  return title;
}
/* Where the games go, shown while a list has none. */
#ifdef _WIN32
#define ROMS_PATH "DATA\\roms\\"
#else
#define ROMS_PATH "DATA/roms/"
#endif
/* A pick-list: the chosen row, or -1 for B (or the app closing). HINT, when set, is a line under the list. */
static int menu_pick_hint(const char *title, const char *hint, const char *const *items, int count, int selected) {
  char shortTitle[1100];
  struct pd_menu m;
  pd_menu_init(&m, menu_title(title, shortTitle, sizeof shortTitle), items, count);
  m.emptyPath = ROMS_PATH;
  m.hint = hint;
  m.selected = selected >= 0 && selected < count ? selected : 0;
  while (running) {
    events();
    repeat_directions();
    menu_script();
    if (down(B) || down(MENU))
      return -1;
    if (down(UP))
      pd_menu_move(&m, -1);
    if (down(DOWN))
      pd_menu_move(&m, 1);
    if (down(A) && count)
      return m.selected;
    int tapped = menu_taps(&m);
    if (tapped >= 0 && tapped < count)
      return tapped;
    companion_canvas();
    pd_menu_draw(&m, &canvas);
    if (menu_frame())
      return -1;
  }
  return -1;
}
static int menu_pick(const char *title, const char *const *items, int count, int selected) {
  return menu_pick_hint(title, NULL, items, count, selected);
}
/* A pick-list that also ends on any key in KEYS (LEFT, RIGHT and the shoulders repeat while held): the row, with
 * *KEY the key that ended it (A for a pick or a tap), or -1 for B. */
static int menu_pick_keys(const char *title, const char *hint, const char *const *items, int count, int selected,
                          unsigned keys, int *key) {
  char shortTitle[1100];
  struct pd_menu m;
  pd_menu_init(&m, menu_title(title, shortTitle, sizeof shortTitle), items, count);
  m.hint = hint;
  m.selected = selected >= 0 && selected < count ? selected : 0;
  *key = A;
  while (running) {
    events();
    repeat_directions();
    static const int shoulders[] = {L, R, L2, R2};
    for (int i = 0; i < 4; i++) {
      int k = shoulders[i];
      if ((held & (1u << k)) && SDL_GetTicks() >= repeatAt[k]) {
        pressed |= 1u << k;
        repeatAt[k] = SDL_GetTicks() + 120;
      }
    }
    menu_script();
    if (down(B) || down(MENU))
      return -1;
    if (down(UP))
      pd_menu_move(&m, -1);
    if (down(DOWN))
      pd_menu_move(&m, 1);
    for (int k = 0; k < NKEYS; k++)
      if ((keys & (1u << k)) && down(k) && count) {
        *key = k;
        return m.selected;
      }
    if (down(A) && count)
      return m.selected;
    int tapped = menu_taps(&m);
    if (tapped >= 0 && tapped < count)
      return tapped;
    companion_canvas();
    pd_menu_draw(&m, &canvas);
    if (menu_frame())
      return -1;
  }
  return -1;
}
#ifdef POCKET_DS
extern const struct pd_layout_option *pd_ds_layouts(int *count);
extern int pd_ds_layout_load(const char *root);
extern bool pd_ds_layout_save(const char *root, int layout);
extern bool pd_ds_wing_left_load(const char *root);
extern bool pd_ds_wing_left_save(const char *root, bool left);
extern bool pd_ds_popout_load(const char *root);
extern bool pd_ds_popout_save(const char *root, bool on);
#endif
/* LAYOUT's picker from the game list's Settings: the layouts as drawn tiles (pd_layout_pick.h). The D-pad walks
 * them, A or a tap picks one, B leaves. The picked index, or -1. */
static int layout_pick(const char *title, const struct pd_layout_option *options, int count, int current) {
  int selected = current >= 0 && current < count ? current : 0, scroll = 0, pressedTile = -1;
  int tiles[PD_LAYOUT_OPTIONS][4] = {{0}};
  int cols = pd_layout_picker_columns(canvas.w);
  while (running) {
    events();
    repeat_directions();
    menu_script();
    if (down(B) || down(MENU))
      return -1;
    if (down(LEFT) && selected > 0)
      selected--;
    if (down(RIGHT) && selected < count - 1)
      selected++;
    if (down(UP) && selected >= cols)
      selected -= cols;
    if (down(DOWN) && selected + cols < count)
      selected += cols;
    if (down(A))
      return selected;
    for (int i = 0; i < tapCount; i++) {
      int hit = pd_layout_picker_hit((const int (*)[4])tiles, count, taps[i].x, taps[i].y);
      if (taps[i].down)
        pressedTile = hit;
      else if (hit >= 0 && hit == pressedTile)
        return hit;
      else
        pressedTile = -1;
    }
    tapCount = 0;
    companion_canvas();
    pd_layout_picker_draw(&canvas, title, options, count, current, selected, pressedTile, &scroll, tiles);
    if (menu_frame())
      return -1;
  }
  return -1;
}
/* ADD A FOLDER's browser: the drives, then folders; A opens one, USE THIS FOLDER picks the one you're in, B goes up
 * (at the drives, back out). True with the folder in OUT. With EXT (".bps") it picks a file instead: the folders and
 * the files ending in it, A on a file picks it. */
static bool browse_path(char *out, size_t size, const char *ext, const char *title) {
  static char dir[POCKET_FOLDER_MAX]; // a second look starts where the last one was
  char from[POCKET_FOLDER_MAX] = "";
  if (!ext || (*dir && !pocket_is_dir(dir)))
    dir[0] = 0;
  while (running) {
    char **names;
    int n = rom_dir_entries_with(dir, ext, &names), extra = *dir && !ext ? 1 : 0, selected = 0;
    const char **items = malloc(sizeof *items * (size_t)(n + 1));
    if (!items) {
      rom_dir_entries_free(names, n);
      return false;
    }
    if (extra)
      items[0] = "USE THIS FOLDER";
    for (int i = 0; i < n; i++) {
      items[i + extra] = names[i];
      if (*from && !strcmp(names[i], from))
        selected = i + extra; // back up: the folder you came from
    }
    int pick = menu_pick(*dir ? dir : title, items, n + extra, selected);
    bool done = false, ok = false;
    from[0] = 0;
    if (pick < 0) {
      if (!*dir || !running)
        done = true;
      else {
        // The name of the folder left, to land on it.
        const char *last = dir + strlen(dir);
        while (last > dir && last[-1] != '/' && last[-1] != '\\')
          last--;
        snprintf(from, sizeof from, "%s", *last ? last : dir);
        rom_dir_up(dir);
      }
    } else if (extra && pick == 0) {
      snprintf(out, size, "%s", dir);
      done = ok = true;
    } else {
      char next[POCKET_FOLDER_MAX];
      if (*dir)
        rom_dir_join(next, sizeof next, dir, names[pick - extra]);
      else
        snprintf(next, sizeof next, "%s", names[pick]);
      if (ext && *dir && !pocket_is_dir(next)) {
        snprintf(out, size, "%s", next);
        done = ok = true;
      } else
        snprintf(dir, sizeof dir, "%s", next);
    }
    free(items);
    rom_dir_entries_free(names, n);
    if (done)
      return ok;
  }
  return false;
}
static bool browse_folder(char *out, size_t size) { return browse_path(out, size, NULL, "PICK A GAME FOLDER"); }
/* A whole file, up to MAX bytes (NULL past it or when it can't be read). */
static uint8_t *read_all(const char *path, size_t max, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  uint8_t *data = NULL;
  long n = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
  if (n >= 0 && (size_t)n <= max && fseek(f, 0, SEEK_SET) == 0 && (data = malloc((size_t)n + 1)) &&
      fread(data, 1, (size_t)n, f) != (size_t)n) {
    free(data);
    data = NULL;
  }
  fclose(f);
  if (data)
    *len = (size_t)n;
  return data;
}
static long file_length(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;
  long n = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
  fclose(f);
  return n;
}
/* The first N characters of two paths alike, '/' and '\' as one and case aside (the game list joins with '/'). */
static bool path_prefix(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char x = a[i] == '\\' ? '/' : (char)tolower((unsigned char)a[i]);
    char y = b[i] == '\\' ? '/' : (char)tolower((unsigned char)b[i]);
    if (x != y || !x)
      return false;
  }
  return true;
}
static bool path_under(const char *path, const char *dir) {
  size_t n = strlen(dir);
  while (n && (dir[n - 1] == '/' || dir[n - 1] == '\\'))
    n--;
  return path_prefix(path, dir, n) && (path[n] == 0 || path[n] == '/' || path[n] == '\\');
}
static bool same_path(const char *a, const char *b) { return strlen(a) == strlen(b) && path_prefix(a, b, strlen(a)); }
/* Where a patched game of kind PLAT goes, as the app's RomPatcher.outputDir: a game folder named for its kind (gba,
 * gb / gbc, nds), or such a folder right inside one, the base game's own first; else the base game's folder when it's
 * in a game folder; else DATA/roms. */
static void patch_output_dir(char *out, size_t size, enum bps_platform plat, const char *base) {
  static char roots[POCKET_FOLDERS_MAX + 1][POCKET_FOLDER_MAX];
  snprintf(roots[0], sizeof roots[0], "%s/roms", root);
  int n = 1 + rom_folders_read(root, roots + 1, POCKET_FOLDERS_MAX);
  static const char *const KINDS[][2] = {{"gba", NULL}, {"gb", "gbc"}, {"gbc", "gb"}, {"nds", NULL}};
  char baseDir[POCKET_FOLDER_MAX];
  snprintf(baseDir, sizeof baseDir, "%s", base);
  char *cut = NULL;
  for (char *p = baseDir; *p; p++)
    if (*p == '/' || *p == '\\')
      cut = p;
  if (cut)
    *cut = 0;
  char first[POCKET_FOLDER_MAX] = "";
  for (int k = 0; k < 2; k++) {
    const char *kind = KINDS[plat][k];
    for (int r = 0; kind && r < n; r++) {
      // The game folder itself, then the folders right inside it.
      char **names;
      int m = rom_dir_entries(roots[r], &names);
      for (int i = -1; i < m; i++) {
        char cand[POCKET_FOLDER_MAX];
        if (i < 0) {
          const char *leaf = roots[r];
          for (const char *p = roots[r]; *p; p++)
            if ((*p == '/' || *p == '\\') && p[1])
              leaf = p + 1;
          if (strncasecmp(leaf, kind, strlen(kind)) || (leaf[strlen(kind)] && leaf[strlen(kind)] != '/' &&
                                                         leaf[strlen(kind)] != '\\'))
            continue;
          snprintf(cand, sizeof cand, "%.*s", (int)sizeof cand - 1, roots[r]);
        } else if (!strcasecmp(names[i], kind)) {
          rom_dir_join(cand, sizeof cand, roots[r], names[i]);
        } else
          continue;
        if (path_under(baseDir, cand)) {
          snprintf(out, size, "%s", cand);
          rom_dir_entries_free(names, m);
          return;
        }
        if (!*first)
          snprintf(first, sizeof first, "%s", cand);
      }
      rom_dir_entries_free(names, m);
    }
  }
  if (*first) {
    snprintf(out, size, "%s", first);
    return;
  }
  for (int r = 0; r < n; r++)
    if (cut && path_under(baseDir, roots[r])) {
      snprintf(out, size, "%s", baseDir);
      return;
    }
  pocket_mkdir(roots[0]);
  snprintf(out, size, "%s", roots[0]);
}
static const char *file_leaf(const char *path) {
  const char *leaf = path;
  for (const char *p = path; *p; p++)
    if (*p == '/' || *p == '\\')
      leaf = p + 1;
  return leaf;
}
/* PATCH A ROM, as the app's: the .bps at PATCH read, the game it was made for found in the game list by its size and
 * CRC32 (no picking), a look at what will happen, then the patched game written beside the player's other games of its
 * kind, named after the patch. The base game is never touched; nothing is left behind on a failure. True with the new
 * game's path in MADE. */
static bool patch_rom(const char *patch, char *made, size_t madeSize) {
  size_t plen = 0;
  uint8_t *pb = read_all(patch, BPS_MAX_BYTES, &plen);
  if (!pb) {
    notice("Couldn't read the patch");
    return false;
  }
  struct bps p;
  enum bps_result r = bps_parse(&p, pb, plen);
  if (r == BPS_OK && p.sourceSize + p.targetSize > BPS_MAX_BYTES)
    r = BPS_TOO_BIG;
  if (r != BPS_OK) {
    notice(bps_reason(r));
    free(pb);
    return false;
  }
  // The game it's for: only same-sized files are read.
  struct rom_list l = {0};
  rom_list_load(&l, root, true);
  char base[POCKET_FOLDER_MAX] = "";
  uint8_t *src = NULL;
  size_t slen = 0;
  for (int i = 0; i < l.count && !src; i++) {
    if (file_length(l.paths[i]) != (long)p.sourceSize)
      continue;
    size_t len = 0;
    uint8_t *d = read_all(l.paths[i], (size_t)p.sourceSize, &len);
    if (d && len == p.sourceSize && bps_crc32(0, d, len) == p.sourceCrc) {
      src = d;
      slen = len;
      snprintf(base, sizeof base, "%s", l.paths[i]);
    } else
      free(d);
  }
  rom_list_free(&l);
  if (!src) {
    const char *known = bps_known_base(p.sourceCrc);
    char msg[128];
    snprintf(msg, sizeof msg, "BASE ROM NOT FOUND: the patch needs %.80s", known ? known : "a game your list doesn't have");
    notice(msg);
    free(pb);
    return false;
  }
  enum bps_platform plat = bps_platform_of(src, slen);
  static const char *const EXT[] = {"gba", "gb", "gbc", "nds"};
  const char *ext = EXT[plat], *dot = strrchr(file_leaf(base), '.');
  if ((plat == BPS_GB || plat == BPS_GBC) && dot && (!strcasecmp(dot, ".gb") || !strcasecmp(dot, ".gbc")))
    ext = dot + 1; // Yellow is a .gb that's also a Color cart
  char dir[POCKET_FOLDER_MAX], name[300];
  patch_output_dir(dir, sizeof dir, plat, base);
  bps_output_name(name, sizeof name, file_leaf(patch), ext);
  char rows[5][POCKET_FOLDER_MAX + 40];
  snprintf(rows[0], sizeof rows[0], "PATCH\t" PD_MENU_MUTED "%s", file_leaf(patch));
  snprintf(rows[1], sizeof rows[1], "GAME\t" PD_MENU_MUTED "%s", file_leaf(base));
  snprintf(rows[2], sizeof rows[2], "SAVED IN\t" PD_MENU_MUTED "%s", dir);
  snprintf(rows[3], sizeof rows[3], "APPLY PATCH");
  snprintf(rows[4], sizeof rows[4], "CANCEL");
  const char *items[5] = {rows[0], rows[1], rows[2], rows[3], rows[4]};
  char hint[400];
  snprintf(hint, sizeof hint, "Makes %s. Your game stays as it is.", name);
  if (menu_pick_hint("PATCH A ROM", hint, items, 5, 3) != 3) {
    free(src);
    free(pb);
    return false;
  }
  uint8_t *out = NULL;
  size_t olen = 0;
  r = bps_apply(&p, src, slen, &out, &olen);
  free(src);
  free(pb);
  if (r != BPS_OK) {
    notice(bps_reason(r));
    return false;
  }
  // A free name (" (2)" and on); the same game made before is kept as it is.
  char path[POCKET_FOLDER_MAX + 320], stem[300];
  snprintf(stem, sizeof stem, "%s", name);
  char *sd = strrchr(stem, '.');
  if (sd)
    *sd = 0;
  rom_dir_join(path, sizeof path, dir, name);
  for (int k = 2; pocket_exists(path); k++) {
    size_t have = 0;
    uint8_t *there = file_length(path) == (long)olen ? read_all(path, olen, &have) : NULL;
    bool same = there && have == olen && !memcmp(there, out, olen);
    free(there);
    if (same) {
      free(out);
      snprintf(made, madeSize, "%s", path);
      char msg[128];
      snprintf(msg, sizeof msg, "%.100s is there already", file_leaf(path));
      notice(msg);
      return true;
    }
    char numbered[340];
    snprintf(numbered, sizeof numbered, "%s (%d).%s", stem, k, ext);
    rom_dir_join(path, sizeof path, dir, numbered);
  }
  char tmp[sizeof path + 8];
  {
    char part[360];
    snprintf(part, sizeof part, ".%.300s.part", file_leaf(path));
    rom_dir_join(tmp, sizeof tmp, dir, part);
  }
  FILE *f = fopen(tmp, "wb");
  bool ok = f && fwrite(out, 1, olen, f) == olen;
  if (f)
    ok = fclose(f) == 0 && ok;
  ok = ok && pocket_replace(tmp, path);
  free(out);
  if (!ok) {
    pocket_remove(tmp);
    notice("Couldn't save the patched game there");
    return false;
  }
  snprintf(made, madeSize, "%s", path);
  char msg[128];
  snprintf(msg, sizeof msg, "Saved %.110s", file_leaf(path));
  notice(msg);
  return true;
}
/* PATCH A ROM's start: a .bps picked in the browser. */
static bool patch_rom_pick(char *made, size_t madeSize) {
  char patch[POCKET_FOLDER_MAX];
  return browse_path(patch, sizeof patch, ".bps", "PICK A .BPS PATCH") && patch_rom(patch, made, madeSize);
}
/* GAME FOLDERS: the folders the game list reads besides DATA/roms (DATA/roms-folders.txt). True when it changed. */
static bool folders_screen(void) {
  static char folders[POCKET_FOLDERS_MAX][POCKET_FOLDER_MAX];
  bool changed = false;
  int selected = 0;
  while (running) {
    int n = rom_folders_read(root, folders, POCKET_FOLDERS_MAX);
    const char *items[POCKET_FOLDERS_MAX + 1];
    items[0] = "ADD A FOLDER";
    for (int i = 0; i < n; i++)
      items[i + 1] = folders[i];
    int pick = menu_pick("GAME FOLDERS", items, n + 1, selected);
    if (pick < 0)
      return changed;
    selected = pick;
    if (pick == 0) {
      char folder[POCKET_FOLDER_MAX];
      if (browse_folder(folder, sizeof folder)) {
        if (rom_folders_add(root, folder)) {
          changed = true;
          selected = n + 1;
        } else
          notice(n >= POCKET_FOLDERS_MAX ? "The list is full" : "That folder is on the list already");
      }
    } else {
      // Off the list only: the games and saves in it are left where they are.
      static const char *const ask[] = {"REMOVE FROM THE LIST", "KEEP IT"};
      if (menu_pick(folders[pick - 1], ask, 2, 1) == 0 && rom_folders_remove(root, pick - 1)) {
        changed = true;
        selected = pick - 1;
      }
    }
  }
  return changed;
}
/* The library's rail (the app's LibraryNav: GAMES / SETTINGS down the left; the app's TOOLS has nothing this host
 * runs) and the game list's header button (the app's +, here ADD GAMES: GAME FOLDERS). */
static const char *const RAIL[] = {"GAMES", "SETTINGS"};
enum { RAIL_GAMES, RAIL_SETTINGS };
static void rail_setup(struct pd_menu *m, int open) {
  m->nav = RAIL;
  m->navCount = 2;
  m->navSelected = open;
}
static int first_pickable(const struct pd_menu *m) {
  for (int i = 0; i < m->count; i++)
    if (pd_menu_pickable(m, i))
      return i;
  return -1;
}
/* The D-pad on the rail and the header: LEFT from the list goes to the rail, UP / DOWN there open the next page,
 * RIGHT (or A) goes back to the list; UP from the list's first row (or from the rail's top) goes to the header's
 * buttons. Returns the rail item to open (-1: none) and sets *act to a header button picked (-1: none); *listKeys
 * says whether the list still takes this frame's keys. */
static int rail_keys(struct pd_menu *m, int *act, bool *listKeys) {
  int open = -1, before = m->focus;
  *act = -1;
  if (before == PD_MENU_FOCUS_NAV) {
    if (down(UP) && m->navSelected > 0)
      open = m->navSelected - 1;
    else if (down(UP) && m->actionCount)
      m->focus = PD_MENU_FOCUS_ACTIONS;
    if (down(DOWN) && m->navSelected < m->navCount - 1)
      open = m->navSelected + 1;
    if (down(RIGHT) || down(A))
      m->focus = PD_MENU_FOCUS_LIST;
  } else if (before == PD_MENU_FOCUS_ACTIONS) {
    if (down(LEFT)) {
      if (m->actionSelected > 0)
        m->actionSelected--;
      else if (m->navCount)
        m->focus = PD_MENU_FOCUS_NAV;
    }
    if (down(RIGHT) && m->actionSelected < m->actionCount - 1)
      m->actionSelected++;
    if (down(DOWN))
      m->focus = PD_MENU_FOCUS_LIST;
    if (down(A))
      *act = m->actionSelected;
  } else {
    if (down(LEFT) && m->navCount)
      m->focus = PD_MENU_FOCUS_NAV;
    else if (down(UP) && m->actionCount && (m->count == 0 || m->selected == first_pickable(m)))
      m->focus = PD_MENU_FOCUS_ACTIONS;
  }
  *listKeys = before == PD_MENU_FOCUS_LIST && m->focus == PD_MENU_FOCUS_LIST;
  return open;
}
/* A rail / header tap from the last menu_taps. */
static int rail_taps(struct pd_menu *m, int *act) {
  int open = m->navPicked;
  if (m->actionPicked >= 0)
    *act = m->actionPicked;
  m->navPicked = m->actionPicked = -1;
  return open;
}
/* GET STARTED (the app's first-time setup): a welcome, then GAME FOLDERS, BUTTON LAYOUT, FULLSCREEN, the
 * COMPANION VIEW and DS LAYOUT pickers and COLORS, one page each, then the game list. Shown on a first start with no
 * games and no DATA/setup-done; SETTINGS > APP > RUN SETUP brings it back. A on a row picks it and goes on, B goes
 * back a page (on the welcome: skips setup). Every choice is saved as it is made. True when the game list needs
 * reading again (a folder was added). */
static bool setup_done(void) {
  char path[1100];
  snprintf(path, sizeof path, "%s/setup-done", root);
  return pocket_readable(path);
}
static void setup_mark_done(void) {
  char path[1100];
  snprintf(path, sizeof path, "%s/setup-done", root);
  FILE *f = fopen(path, "wb");
  if (f) {
    fputs("1\n", f);
    fclose(f);
  }
}
enum { SETUP_WELCOME, SETUP_FOLDERS, SETUP_BUTTONS, SETUP_FULL, SETUP_VIEW, SETUP_DS, SETUP_COLORS, SETUP_DONE };
static bool get_started(void) {
  static const char *const COLOR_ROWS[PD_COLORS] = {"ORIGINAL\tTHE GAME AS MADE", "HANDHELD\tA HANDHELD SCREEN'S TINT",
                                                    "OLED\tTRUE BLACK AROUND IT"};
  char path[1100];
  snprintf(path, sizeof path, "%s/settings.ini", root);
  bool changed = false;
  int step = SETUP_WELCOME;
#ifdef POCKET_DS
  const int steps = 6; // the numbered pages: folders, buttons, fullscreen, view, DS layout, colours
#else
  const int steps = 5;
#endif
  static char hints[SETUP_DONE + 1][160]; // a buffer per page
  while (running) {
    char *hint = hints[step];
    const size_t hintSize = sizeof hints[0];
    struct pd_settings st;
    pd_settings_load(&st, path);
    int pick = -1, number = step - SETUP_WELCOME;
#ifndef POCKET_DS
    if (step > SETUP_DS)
      number--;
#endif
    switch (step) {
    case SETUP_WELCOME: {
      static const char *const rows[] = {"GET STARTED", "SKIP SETUP"};
      pick = menu_pick_hint("WELCOME TO POKEDAISY", "A few choices to start. SETTINGS can change any of them later.",
                            rows, 2, 0);
      if (pick == 1 || (pick < 0 && running)) {
        setup_mark_done();
        return changed;
      }
      if (pick == 0)
        step = SETUP_FOLDERS;
      break;
    }
    case SETUP_FOLDERS: {
      static char folders[POCKET_FOLDERS_MAX][POCKET_FOLDER_MAX];
      int n = rom_folders_read(root, folders, POCKET_FOLDERS_MAX);
      char count[64];
      snprintf(count, sizeof count, "YOUR FOLDERS\t%d", n);
      const char *rows[] = {"NEXT", "ADD A FOLDER", count};
      snprintf(hint, hintSize, "STEP %d OF %d. Add the folders that hold your games.", number,
               steps);
      pick = menu_pick_hint("GAME FOLDERS", hint, rows, n ? 3 : 2, 0);
      if (pick < 0)
        step = SETUP_WELCOME;
      else if (pick == 0)
        step = SETUP_BUTTONS;
      else if (pick == 1) {
        char folder[POCKET_FOLDER_MAX];
        if (browse_folder(folder, sizeof folder)) {
          if (rom_folders_add(root, folder))
            changed = true;
          else
            notice(n >= POCKET_FOLDERS_MAX ? "The list is full" : "That folder is on the list already");
        }
      } else if (folders_screen())
        changed = true;
      break;
    }
    case SETUP_BUTTONS: {
      static const char *const rows[] = {"NINTENDO\tA ON THE RIGHT", "XBOX\tA AT THE BOTTOM"};
      snprintf(hint, hintSize, "STEP %d OF %d. Which button is A? LT / RT zoom, L3 is fast-forward and R3 "
                               "switches the view (hold it to pick one): "
                               "SETTINGS > HOTKEYS changes them.",
               number, steps);
      pick = menu_pick_hint("BUTTON LAYOUT", hint, rows, 2, st.buttonLayout == PD_LAYOUT_XBOX ? 1 : 0);
      if (pick < 0)
        step = SETUP_FOLDERS;
      else {
        st.buttonLayout = pick == 1 ? PD_LAYOUT_XBOX : PD_LAYOUT_NINTENDO;
        pd_settings_save(&st, path);
        controls_load();
        step = SETUP_FULL;
      }
      break;
    }
    case SETUP_FULL: {
      static const char *const rows[] = {"WINDOW", "FULLSCREEN"};
      snprintf(hint, hintSize, "STEP %d OF %d. F11 (or L3 + R3 on a pad) switches it any time.", number, steps);
      pick = menu_pick_hint("FULLSCREEN", hint, rows, 2, fullscreenOn ? 1 : 0);
      if (pick < 0)
        step = SETUP_BUTTONS;
      else {
        if ((pick == 1) != fullscreenOn)
          fullscreen_set(pick == 1);
        step = SETUP_VIEW;
      }
      break;
    }
    case SETUP_VIEW: {
      snprintf(hint, hintSize, "COMPANION VIEW (STEP %d OF %d)", number, steps);
      int v = layout_pick(hint, companion_layouts(), POCKET_VIEWS, st.companionView);
      if (v < 0)
        step = SETUP_FULL;
      else {
        st.companionView = v;
        pd_settings_save(&st, path);
        step = SETUP_DS;
      }
      break;
    }
    case SETUP_DS: {
#ifdef POCKET_DS
      int dsCount = 0;
      const struct pd_layout_option *dsLayouts = pd_ds_layouts(&dsCount);
      snprintf(hint, hintSize, "DS LAYOUT (STEP %d OF %d)", number, steps);
      int l = layout_pick(hint, dsLayouts, dsCount, pd_ds_layout_load(root));
      if (l < 0) {
        step = SETUP_VIEW;
        break;
      }
      if (!pd_ds_layout_save(root, l))
        notice("Couldn't save DS LAYOUT");
#endif
      step = SETUP_COLORS;
      break;
    }
    case SETUP_COLORS: {
      snprintf(hint, hintSize, "STEP %d OF %d. OLED keeps everything around the game black.", number, steps);
      pick = menu_pick_hint("COLORS", hint, COLOR_ROWS, PD_COLORS, st.colors >= 0 && st.colors < PD_COLORS ? st.colors : 0);
      if (pick < 0) {
#ifdef POCKET_DS
        step = SETUP_DS;
#else
        step = SETUP_VIEW;
#endif
      } else {
        st.colors = pick;
        pd_settings_save(&st, path);
        pocket_shader_set(st.shader, st.colors, false);
        oledBlack = st.colors == POCKET_COLORS_OLED;
        step = SETUP_DONE;
      }
      break;
    }
    default: {
      static const char *const rows[] = {"GO TO MY GAMES"};
      pick = menu_pick_hint("YOU'RE ALL SET", "SETTINGS > APP > RUN SETUP brings this back.", rows, 1, 0);
      if (pick < 0 && running)
        step = SETUP_COLORS;
      else {
        setup_mark_done();
        return changed;
      }
    }
    }
  }
  return changed;
}
/* The game list's SETTINGS: the app's top-screen Settings in its groups and order (SCREEN, LIBRARY, CONTROLS, APP;
 * its FF / SPEED and ONLINE rows have nothing to set here), with its sub-pages DISPLAY OPTIONS, GAME BUTTONS and
 * HOTKEYS. COMPANION VIEW and DS LAYOUT open the LAYOUT pickers. Saved to DATA/settings.ini (DS LAYOUT:
 * DATA/ds-settings.ini) at once. Returns the rail item to open next (RAIL_GAMES), or -1 when the app closes;
 * *foldersChanged says the game list needs reading again. */
static void trade_saves(const char *firstRom, const char *firstName);
enum { PAGE_SET_HOME, PAGE_SET_DISPLAY, PAGE_SET_BUTTONS, PAGE_SET_HOTKEYS, PAGE_SET_BEZEL, PAGE_SET_LIST };
/* LAYOUT's lists (SETTINGS > SCREEN > LAYOUT): the top bar, the bottom bar and the right wing. */
enum { LIST_TOP, LIST_BOTTOM, LIST_WING, LIST_BATTLE_TOP, LIST_BATTLE_BOTTOM, LIST_BATTLE_WING, LISTS };
static const char *const LIST_TITLES[LISTS] = {"TOP BAR",           "BOTTOM BAR",        "RIGHT WING",
                                               "BATTLE TOP BAR",    "BATTLE BOTTOM BAR", "BATTLE RIGHT WING"};
static bool list_is_wing(int list) { return list == LIST_WING || list == LIST_BATTLE_WING; }
static int list_bar(int list) { return list >= LIST_BATTLE_TOP ? list - LIST_BATTLE_TOP + 2 : list; }
/* A list's items in their order (the ones on), then how many there are; what's off is every other item. */
static int layout_list(const struct pd_settings *st, int list, int *out) {
  return list == LIST_WING          ? wing_list_of(st, out)
         : list == LIST_BATTLE_WING ? battle_list_of(st, out)
                                    : bar_list_of(st, list_bar(list), out);
}
static void layout_store(struct pd_settings *st, int list, const int *items, int n) {
  if (list == LIST_WING) {
    memcpy(st->wingList, items, (size_t)n * sizeof *items);
    st->wingCount = n;
  } else if (list == LIST_BATTLE_WING) {
    memcpy(st->battleWingList, items, (size_t)n * sizeof *items);
    st->battleWingCount = n;
  } else {
    memcpy(st->barList[list_bar(list)], items, (size_t)n * sizeof *items);
    st->barCount[list_bar(list)] = n;
  }
}
static int layout_items(int list) {
  return list == LIST_WING ? PD_WING_SECTIONS : list == LIST_BATTLE_WING ? PD_BATTLE_SECTIONS : BEZEL_ITEMS;
}
static const char *layout_name(int list, int item) {
  return list == LIST_WING          ? pd_wing_section_names[item]
         : list == LIST_BATTLE_WING ? pd_battle_section_names[item]
                                    : BEZEL_ITEM_NAMES[item];
}
/* An item's place in its list, -1 = off. */
static int layout_at(const int *items, int n, int item) {
  for (int i = 0; i < n; i++)
    if (items[i] == item)
      return i;
  return -1;
}
enum {
  ROW_VIEW = 1,
  ROW_DS,
  ROW_DS_SIDE,
  ROW_DS_POPOUT,
  ROW_DISPLAY,
  ROW_FOLDERS,
  ROW_PATCH,
  ROW_TRADE,
  ROW_LINK,
  ROW_BOXART,
  ROW_SGDB,
  ROW_BOXART_WHERE,
  ROW_BUTTONS,
  ROW_HOTKEYS,
  ROW_UPDATES,
  ROW_SHADER,
  ROW_COLORS,
  ROW_BRIGHTNESS,
  ROW_DIM,
  ROW_MOTION,
  ROW_BACKGROUND,
  ROW_WING_LOOK,
  ROW_UI_SIZE,
  ROW_WING_ROOM,
  ROW_STRETCH,
  ROW_FULL,
  ROW_ORIENT,
  ROW_TOUCH,
  ROW_LAYOUT,
  ROW_SETUP,
  ROW_BEZEL_STYLE,
  ROW_BEZEL,
  ROW_RUMBLE,
  ROW_SAVER,
  ROW_SCREEN_LAYOUT,
  ROW_LIST_TOP,
  ROW_LIST_BOTTOM,
  ROW_LIST_WING,
  ROW_LIST_BATTLE_TOP, // ROW_LIST_TOP + LIST_*: in the LIST_ order
  ROW_LIST_BATTLE_BOTTOM,
  ROW_LIST_BATTLE_WING,
  ROW_PARTY_TYPES,
  ROW_LIST_RESET,
  ROW_RA,
  ROW_FILTER,
  ROW_GRID,
  ROW_BEZEL_SLOT0 = 80,
  ROW_HOTKEY0 = 100,
  ROW_LIST_ITEM0 = 200
};
static bool text_entry(const char *title, char *buf, size_t size);
static void link_setup(const char *firstRom, const char *firstName);
static bool textMask; // text_entry shows a star for each character (a password)
/* RETROACHIEVEMENTS > ACCOUNT: the name, then the password, then the server's answer (up to 30 s). */
static void ra_sign_in(void) {
  char user[64] = "", pass[64] = "";
  if (!text_entry("RETROACHIEVEMENTS USER NAME", user, sizeof user) || !*user)
    return;
  textMask = true;
  bool typed = text_entry("RETROACHIEVEMENTS PASSWORD", pass, sizeof pass) && *pass;
  textMask = false;
  if (!typed)
    return;
  notice("Signing in to RetroAchievements...");
  ra_login(user, pass);
  memset(pass, 0, sizeof pass);
  Uint32 until = SDL_GetTicks() + 30000;
  while (running && ra_login_state() == RA_LOGGING_IN && (Sint32)(until - SDL_GetTicks()) > 0) {
    events();
    SDL_Delay(16);
  }
  if (ra_login_state() != RA_LOGGED_IN) {
    char line[200];
    snprintf(line, sizeof line, "Couldn't sign in: %s", *ra_login_error() ? ra_login_error() : "no answer");
    notice(line);
  }
}
static int settings_page(bool *foldersChanged) {
  static const char *const ORIENTATIONS[PD_ORIENTATIONS] = {"LANDSCAPE", "PORTRAIT", "LANDSCAPE FLIPPED",
                                                            "PORTRAIT FLIPPED"};
  static const char *const SHADERS[PD_SHADERS] = {"OFF", "SHARP", "xBRZ"};
  static const char *const COLORS[PD_COLORS] = {"ORIGINAL", "HANDHELD", "OLED"};
  static const char *const UI_SIZES[3] = {"SMALL", "MEDIUM", "LARGE"};
  static const char *const WING_ROOMS[4] = {"GAME FIRST", "BALANCED", "WINGS FIRST", "AUTO"};
  static const char *const TITLES[] = {"SETTINGS", "DISPLAY OPTIONS", "GAME BUTTONS", "HOTKEYS", "LAYOUT", "LIST"};
  static int listOpen = LIST_TOP, layoutRow = -1;
  int focusId = -1;
  char path[1100];
  snprintf(path, sizeof path, "%s/settings.ini", root);
  int page = PAGE_SET_HOME, homeRow = -1, focus = PD_MENU_FOCUS_LIST;
  int selectedOn[6] = {-1, -1, -1, -1, -1, -1};
  while (running) {
    struct pd_settings st;
    pd_settings_load(&st, path);
    const struct pd_layout_option *views = companion_layouts();
    int dsCount = 0, dsLayout = 0;
    bool dsWingLeft = false, dsPopout = false;
    const struct pd_layout_option *dsLayouts = NULL;
#ifdef POCKET_DS
    dsLayouts = pd_ds_layouts(&dsCount);
    dsLayout = pd_ds_layout_load(root);
    dsWingLeft = pd_ds_wing_left_load(root);
    dsPopout = pd_ds_popout_load(root);
#else
    (void)dsCount;
#endif
    static char rows[PD_HOTKEY_BUTTONS + 48][96];
    const char *items[PD_HOTKEY_BUTTONS + 48];
    int ids[PD_HOTKEY_BUTTONS + 48], n = 0;
#define ROW(id, ...)                                                                                                   \
  do {                                                                                                                 \
    snprintf(rows[n], sizeof rows[n], __VA_ARGS__);                                                                    \
    ids[n++] = (id);                                                                                                   \
  } while (0)
    if (page == PAGE_SET_HOME) {
      ROW(0, PD_MENU_GROUP "SCREEN");
      ROW(ROW_VIEW, "COMPANION VIEW\t%s", views[st.companionView].name);
      if (dsLayouts) {
        ROW(ROW_DS, "DS LAYOUT\t%s", dsLayouts[dsLayout].name);
        ROW(ROW_DS_SIDE, "DS WING SIDE\tTOUCH %s", dsWingLeft ? "LEFT" : "RIGHT");
        ROW(ROW_DS_POPOUT, "TOUCH WINDOW\t%s", dsPopout ? "ON" : "OFF");
      }
      ROW(ROW_DISPLAY, "DISPLAY OPTIONS" PD_MENU_PAGE);
      ROW(ROW_SCREEN_LAYOUT, "LAYOUT" PD_MENU_PAGE);
      ROW(0, PD_MENU_GROUP "LIBRARY");
      ROW(ROW_FOLDERS, "GAME FOLDERS" PD_MENU_PAGE);
      ROW(ROW_PATCH, "PATCH A ROM" PD_MENU_PAGE);
      ROW(ROW_TRADE, "TRADE SAVES" PD_MENU_PAGE);
      ROW(ROW_LINK, "LINK PLAY" PD_MENU_PAGE);
      ROW(ROW_BOXART, "BOX ART DOWNLOAD	%s", st.boxartAuto ? "ON" : "OFF");
      {
        char key[80];
        boxart_key(key, sizeof key);
        ROW(ROW_SGDB, "STEAMGRIDDB KEY	%s", *key ? "SET" : "NONE");
        int where;
        char folder[1024];
        boxart_location(&where, folder, sizeof folder);
        ROW(ROW_BOXART_WHERE, "BOX ART LOCATION\t%s", where == BOXART_ONE_FOLDER ? "ONE FOLDER" : "BESIDE GAMES");
      }
      ROW(0, PD_MENU_GROUP "CONTROLS");
      ROW(ROW_BUTTONS, "GAME BUTTONS" PD_MENU_PAGE);
      ROW(ROW_HOTKEYS, "HOTKEYS" PD_MENU_PAGE);
      ROW(ROW_RUMBLE, "RUMBLE\t%s", st.rumble ? "ON" : "OFF");
      ROW(0, PD_MENU_GROUP "APP");
      {
        static const char *const SAVER[PD_SAVER_COUNT] = {"AUTO", "ON", "OFF"};
        int v = st.batterySaver >= 0 && st.batterySaver < PD_SAVER_COUNT ? st.batterySaver : 0;
        ROW(ROW_SAVER, "BATTERY SAVER\t%s", v == PD_SAVER_AUTO && saver_active() ? "AUTO, SAVING" : SAVER[v]);
      }
      ROW(ROW_UPDATES, "CHECK FOR UPDATES\t%s", st.updateCheck ? "ON" : "OFF");
      ROW(ROW_SETUP, "RUN SETUP" PD_MENU_PAGE);
      ROW(0, "VERSION\t" PD_MENU_MUTED "%s", POCKET_VERSION);
      ROW(0, PD_MENU_GROUP "RETROACHIEVEMENTS");
      ROW(ROW_RA, "ACCOUNT\t%s", ra_login_state() == RA_LOGGED_IN    ? ra_user()
                                 : ra_login_state() == RA_LOGGING_IN ? "SIGNING IN"
                                                                     : "SIGN IN");
    } else if (page == PAGE_SET_DISPLAY) {
      ROW(ROW_SHADER, "SHADER\t%s", SHADERS[st.shader >= 0 && st.shader < PD_SHADERS ? st.shader : 0]);
      ROW(ROW_COLORS, "COLORS\t%s", COLORS[st.colors >= 0 && st.colors < PD_COLORS ? st.colors : 0]);
      ROW(ROW_FILTER, "FILTER\t%s", pd_filter_names[st.filter >= 0 && st.filter < PD_FILTERS ? st.filter : 0]);
      if (st.filter == PD_FILTER_LCD || st.filter == PD_FILTER_LCD_PAPER)
        ROW(ROW_GRID, "GRID\t%s", pd_grid_names[st.grid >= 0 && st.grid < PD_GRIDS ? st.grid : 1]);
      ROW(ROW_BRIGHTNESS, "BRIGHTNESS\t%d%%", st.brightness);
      ROW(ROW_DIM, "DIM COMPANION\t%s", st.dimCompanion ? "ON" : "OFF");
      ROW(ROW_MOTION, "REDUCE MOTION\t%s", st.reduceMotion ? "ON" : "OFF");
      ROW(ROW_BACKGROUND, "BACKGROUND\t%s",
          pocket_background_name(pocket_background_resolve(st.background, st.colors == POCKET_COLORS_OLED, st.darkMode)));
      ROW(ROW_UI_SIZE, "UI SIZE\t%s", UI_SIZES[st.uiSize >= 0 && st.uiSize < 3 ? st.uiSize : 1]);
      ROW(ROW_WING_ROOM, "WING ROOM\t%s", WING_ROOMS[st.wingRoom >= 0 && st.wingRoom < 4 ? st.wingRoom : 3]);
      ROW(ROW_WING_LOOK, "WING LOOK\t%s", pocket_wing_look_name(st.wingLook));
      ROW(ROW_STRETCH, "STRETCH\t%s", st.stretch ? "ON" : "OFF");
      ROW(ROW_BEZEL_STYLE, "BEZEL STYLE\t%s",
          BEZEL_STYLE_NAMES[bezel_style_resolve(st.bezelStyle, pocket_background_resolve(st.background, st.colors ==
                                                                POCKET_COLORS_OLED, st.darkMode) == POCKET_BACKGROUND_BLACK)]);
      ROW(ROW_FULL, "FULLSCREEN\t%s", fullscreenOn ? "ON" : "OFF");
      ROW(ROW_ORIENT, "ORIENTATION\t%s", ORIENTATIONS[st.orientation]);
      ROW(ROW_TOUCH, "GAME TOUCH\t%s", st.gameTouch == PD_GAME_TOUCH_OFF ? "OFF" : "DS SCREEN ONLY");
    } else if (page == PAGE_SET_BEZEL) {
      ROW(0, PD_MENU_GROUP "WALKING");
      ROW(ROW_LIST_TOP, "TOP BAR" PD_MENU_PAGE);
      ROW(ROW_LIST_BOTTOM, "BOTTOM BAR" PD_MENU_PAGE);
      ROW(ROW_LIST_WING, "RIGHT WING" PD_MENU_PAGE);
      ROW(0, PD_MENU_GROUP "IN A BATTLE");
      ROW(ROW_LIST_BATTLE_TOP, "BATTLE TOP BAR" PD_MENU_PAGE);
      ROW(ROW_LIST_BATTLE_BOTTOM, "BATTLE BOTTOM BAR" PD_MENU_PAGE);
      ROW(ROW_LIST_BATTLE_WING, "BATTLE RIGHT WING" PD_MENU_PAGE);
      ROW(0, PD_MENU_GROUP "LEFT WING");
      ROW(ROW_PARTY_TYPES, "PARTY TYPES\t%s", st.partyTypes ? "ON" : "OFF");
    } else if (page == PAGE_SET_LIST) {
      int list[BEZEL_LIST_MAX > PD_WING_LIST_MAX ? BEZEL_LIST_MAX : PD_WING_LIST_MAX];
      int count = layout_list(&st, listOpen, list);
      // What's on in its order (a bar's number is its place), then what's off.
      ROW(0, PD_MENU_GROUP "SHOWN, IN ORDER");
      for (int i = 0; i < count; i++) {
        bool pinned = listOpen == LIST_WING && (list[i] == PD_WS_MAP || list[i] == PD_WS_BUTTONS);
        if (pinned)
          ROW(ROW_LIST_ITEM0 + list[i], "%s\t%s", layout_name(listOpen, list[i]), list[i] == PD_WS_MAP ? "TOP" : "FOOT");
        else
          ROW(ROW_LIST_ITEM0 + list[i], "%s\t%d", layout_name(listOpen, list[i]), i + 1);
      }
      ROW(0, PD_MENU_GROUP "OFF");
      for (int it = list_is_wing(listOpen) ? 0 : 1; it < layout_items(listOpen); it++)
        if (layout_at(list, count, it) < 0)
          ROW(ROW_LIST_ITEM0 + it, "%s\tOFF", layout_name(listOpen, it));
      ROW(0, PD_MENU_GROUP "");
      ROW(ROW_LIST_RESET, "RESET TO DEFAULT");
      if (listOpen == LIST_BATTLE_TOP && st.barCount[2] < 0)
        ROW(0, PD_MENU_MUTED "THE SAME AS THE TOP BAR UNTIL YOU CHANGE IT");
      ROW(0, PD_MENU_MUTED "%s", listOpen == LIST_WING          ? "WHAT DOESN'T FIT IS DROPPED FROM THE BOTTOM"
                                 : listOpen == LIST_BATTLE_WING ? "SHORT OF ROOM: POWER AND ACCURACY, SWITCH TO, FOE TEAM GO FIRST"
                                                                : "WHAT DOESN'T FIT MAKES ROOM FOR THE REST");
    } else if (page == PAGE_SET_BUTTONS) {
      ROW(ROW_LAYOUT, "BUTTON LAYOUT\t%s", st.buttonLayout == PD_LAYOUT_XBOX ? "XBOX" : "NINTENDO");
    } else {
      ROW(0, PD_MENU_GROUP "TRIGGERS AND STICKS");
      for (int r = 0; r < PD_HOTKEY_BUTTONS; r++) {
        int i = HOTKEY_ROW_ORDER[r];
        if (r == 4)
          ROW(0, PD_MENU_GROUP "EXTRA BUTTONS");
        ROW(ROW_HOTKEY0 + i, "%s\t%s", HOTKEY_BUTTON_NAMES[i], HOTKEY_ACTIONS[st.hotkeys[i]]);
      }
      ROW(0, PD_MENU_MUTED "IN A DS GAME L2 / R2 STAY TOUCH SCREEN / DS LAYOUT");
    }
#undef ROW
    for (int i = 0; i < n; i++) {
      items[i] = rows[i];
      if (focusId >= 0 && ids[i] == focusId)
        selectedOn[page] = i;
    }
    focusId = -1;
    struct pd_menu m;
    pd_menu_init(&m, page == PAGE_SET_LIST ? LIST_TITLES[listOpen] : TITLES[page], items, n);
    rail_setup(&m, RAIL_SETTINGS);
    m.focus = focus;
    if (selectedOn[page] >= 0 && pd_menu_pickable(&m, selectedOn[page]))
      m.selected = selectedOn[page];
    int pick = -1, open = -1;
    bool back = false;
    while (running && pick < 0 && open < 0 && !back) {
      events();
      repeat_directions();
      menu_script();
      int act;
      bool listKeys;
      open = rail_keys(&m, &act, &listKeys);
      int tapped = menu_taps(&m);
      int tapOpen = rail_taps(&m, &act);
      if (tapOpen >= 0)
        open = tapOpen;
      if (down(B) || down(MENU))
        back = true;
      else if (listKeys) {
        if (down(UP))
          pd_menu_move(&m, -1);
        if (down(DOWN))
          pd_menu_move(&m, 1);
        if (down(A) && pd_menu_pickable(&m, m.selected))
          pick = m.selected;
      }
      if (tapped >= 0)
        pick = tapped;
      if (open == RAIL_SETTINGS)
        open = -1; // already here
      companion_canvas();
      pd_menu_draw(&m, &canvas);
      if (menu_frame())
        return -1;
    }
    if (!running)
      return -1;
    focus = m.focus;
    selectedOn[page] = m.selected;
    if (open >= 0)
      return open;
    if (back) {
      if (page == PAGE_SET_HOME)
        return RAIL_GAMES;
      if (page == PAGE_SET_LIST) {
        page = PAGE_SET_BEZEL;
        if (layoutRow >= 0)
          selectedOn[PAGE_SET_BEZEL] = layoutRow;
        continue;
      }
      page = PAGE_SET_HOME;
      if (homeRow >= 0)
        selectedOn[PAGE_SET_HOME] = homeRow;
      continue;
    }
    int id = ids[pick];
    bool save = true;
    switch (id) {
    case ROW_VIEW: {
      int v = layout_pick("COMPANION VIEW", views, POCKET_VIEWS, st.companionView);
      if (v < 0)
        save = false;
      else
        st.companionView = v;
      break;
    }
    case ROW_DS:
#ifdef POCKET_DS
    {
      int l = layout_pick("DS LAYOUT", dsLayouts, dsCount, dsLayout);
      if (l >= 0 && !pd_ds_layout_save(root, l))
        notice("Couldn't save DS LAYOUT");
    }
#endif
      save = false;
      break;
    case ROW_DS_SIDE:
#ifdef POCKET_DS
      if (!pd_ds_wing_left_save(root, !dsWingLeft))
        notice("Couldn't save DS WING SIDE");
#endif
      save = false;
      break;
    case ROW_DS_POPOUT:
#ifdef POCKET_DS
      if (!pd_ds_popout_save(root, !dsPopout))
        notice("Couldn't save TOUCH WINDOW");
#endif
      save = false;
      break;
    case ROW_DISPLAY:
    case ROW_BUTTONS:
    case ROW_HOTKEYS:
      homeRow = pick;
      page = id == ROW_DISPLAY ? PAGE_SET_DISPLAY : id == ROW_BUTTONS ? PAGE_SET_BUTTONS : PAGE_SET_HOTKEYS;
      save = false;
      break;
    case ROW_FOLDERS:
      if (folders_screen())
        *foldersChanged = true;
      save = false;
      break;
    case ROW_PATCH: {
      char made[POCKET_FOLDER_MAX + 320];
      if (patch_rom_pick(made, sizeof made))
        *foldersChanged = true;
      save = false;
      break;
    }
    case ROW_TRADE:
      trade_saves(NULL, NULL);
      save = false;
      break;
    case ROW_LINK:
      link_setup(NULL, NULL);
      save = false;
      break;
    case ROW_UPDATES:
      st.updateCheck = !st.updateCheck;
      break;
    case ROW_SAVER:
      st.batterySaver = (st.batterySaver + 1) % PD_SAVER_COUNT;
      saverMode = st.batterySaver;
      break;
    case ROW_RUMBLE:
      st.rumble = !st.rumble;
      if (st.rumble) { // a taste of it, so you know it works
        rumbleOn = true;
        rumble(0x5000, 0x3000, 180);
      }
      break;
    case ROW_BOXART:
      st.boxartAuto = !st.boxartAuto;
      *foldersChanged = true; // the list looks again (or stops)
      break;
    case ROW_RA: {
      if (ra_login_state() == RA_LOGGED_IN) {
        static const char *const raRowsOut[] = {"SIGN OUT", "CANCEL"};
        char hint[160];
        snprintf(hint, sizeof hint, "Signed in as %s. GB and GBA games unlock achievements as you play.",
                 ra_user());
        if (menu_pick_hint("RETROACHIEVEMENTS", hint, raRowsOut, 2, 1) == 0) {
          ra_logout();
          notice("Signed out of RetroAchievements");
        }
      } else if (ra_login_state() == RA_LOGGED_OUT) {
        static const char *const raRowsIn[] = {"SIGN IN", "CANCEL"};
        if (menu_pick_hint("RETROACHIEVEMENTS",
                           "Sign in with your retroachievements.org account. Only its token is kept.",
                           raRowsIn, 2, 0) == 0)
          ra_sign_in();
      }
      save = false;
      break;
    }
    case ROW_SGDB: {
      static const char *const keyRows[] = {"PASTE THE KEY FROM THE CLIPBOARD", "REMOVE THE KEY"};
      int k = menu_pick_hint("STEAMGRIDDB KEY",
                             "More box art for ROM hacks. Make a free key at steamgriddb.com (Preferences, API), copy it, "
                             "then paste it here.",
                             keyRows, 2, 0);
      if (k == 0) {
        char *clip = SDL_HasClipboardText() ? SDL_GetClipboardText() : NULL;
        char key[128] = "";
        if (clip) {
          // Its letters and digits only (a copy often brings spaces or a line end).
          size_t n = 0;
          for (const char *c = clip; *c && n + 1 < sizeof key; c++)
            if ((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z'))
              key[n++] = *c;
          key[n] = 0;
          SDL_free(clip);
        }
        if (!*key)
          notice("Nothing to paste: copy the key first");
        else if (boxart_key_save(key)) {
          notice("SteamGridDB key saved");
          *foldersChanged = true;
        } else
          notice("That isn't a SteamGridDB key");
      } else if (k == 1) {
        boxart_key_save("");
        notice("SteamGridDB key removed");
      }
      save = false;
      break;
    }
    case ROW_BOXART_WHERE: {
      int where;
      char folder[POCKET_FOLDER_MAX];
      boxart_location(&where, folder, sizeof folder);
      static const char *const whereRows[] = {"BESIDE GAMES\tA BOXART FOLDER IN EACH GAME FOLDER",
                                              "ONE FOLDER\tPICK A FOLDER"};
      int w = menu_pick_hint("BOX ART LOCATION",
                             "Where each game's pictures are kept: Name.png is its box art, Name 1.png and so on its "
                             "alternates. Put your own there too; SYNC BOX ART picks them up.",
                             whereRows, 2, where);
      if (w == BOXART_BESIDE_GAMES && boxart_location_set(BOXART_BESIDE_GAMES, NULL)) {
        notice("Box art: beside the games");
        *foldersChanged = true;
      } else if (w == BOXART_ONE_FOLDER && browse_path(folder, sizeof folder, NULL, "PICK THE BOX ART FOLDER")) {
        if (boxart_location_set(BOXART_ONE_FOLDER, folder)) {
          notice("Box art: in that folder, by console (gba, gbc, gb, nds)");
          *foldersChanged = true;
        } else
          notice("Couldn't save that");
      }
      save = false;
      break;
    }
    case ROW_SETUP:
      if (get_started())
        *foldersChanged = true;
      save = false;
      break;
    case ROW_SHADER:
      st.shader = (st.shader + 1) % PD_SHADERS;
      break;
    case ROW_COLORS:
      st.colors = (st.colors + 1) % PD_COLORS;
      break;
    case ROW_FILTER: {
      static const char *const FILTER_ROWS[PD_FILTERS] = {
          "OFF", "LCD\tA GRID BETWEEN THE PIXELS", "LCD PAPER\tA SOFT GRID ON PAPER", "SCANLINES\tDARK LINES BETWEEN ROWS",
          "CRT\tAN OLD TV'S GLOW", "SUBPIXEL\tA FINE MASK OVER THE WINDOW"};
      int f = menu_pick("FILTER", FILTER_ROWS, PD_FILTERS, st.filter >= 0 && st.filter < PD_FILTERS ? st.filter : 0);
      if (f < 0)
        save = false;
      else {
        st.filter = f;
        st.lcdFilter = f == PD_FILTER_SUBPIXEL;
      }
      break;
    }
    case ROW_GRID:
      st.grid = (st.grid + 1) % PD_GRIDS;
      break;
    case ROW_BRIGHTNESS: {
      static char levels[(POCKET_BRIGHTNESS_MAX - POCKET_BRIGHTNESS_MIN) / POCKET_BRIGHTNESS_STEP + 1][24];
      const char *rows[sizeof levels / sizeof *levels];
      int count = 0, at = 0;
      for (int v = POCKET_BRIGHTNESS_MIN; v <= POCKET_BRIGHTNESS_MAX; v += POCKET_BRIGHTNESS_STEP, count++) {
        snprintf(levels[count], sizeof levels[count], v == 100 ? "%d%%\tTHE GAME'S OWN" :
                 v == POCKET_BRIGHTNESS_DEFAULT ? "%d%%\tDEFAULT" : "%d%%", v);
        rows[count] = levels[count];
        if (v == st.brightness)
          at = count;
      }
      int b = menu_pick("BRIGHTNESS", rows, count, at);
      if (b < 0)
        save = false;
      else
        st.brightness = POCKET_BRIGHTNESS_MIN + b * POCKET_BRIGHTNESS_STEP;
      break;
    }
    case ROW_DIM:
      st.dimCompanion = !st.dimCompanion;
      break;
    case ROW_MOTION:
      st.reduceMotion = !st.reduceMotion;
      break;
    case ROW_BACKGROUND: {
      static const char *const rows[POCKET_BACKGROUNDS] = {"BLACK\tTRUE BLACK, FOR OLED", "DARK\tA NEAR BLACK",
                                                           "THEME\tTHE THEME'S DARK SLATE",
                                                           "BLUR\tTHE GAME, BLURRED, ROUND THE CARDS"};
      int b = menu_pick("BACKGROUND", rows, POCKET_BACKGROUNDS,
                        pocket_background_resolve(st.background, st.colors == POCKET_COLORS_OLED, st.darkMode));
      if (b < 0)
        save = false;
      else
        st.background = b;
      break;
    }
    case ROW_UI_SIZE: {
      static const char *const rows[3] = {"SMALL\tMORE ON SCREEN", "MEDIUM\tTHE USUAL",
                                          "LARGE\tBIG TEXT, FROM A DISTANCE"};
      int l = menu_pick("UI SIZE", rows, 3, st.uiSize);
      if (l < 0)
        save = false;
      else
        st.uiSize = l;
      break;
    }
    case ROW_WING_ROOM: {
      // AUTO first in the list; its value is 3 (the three fixed rooms keep 0 to 2).
      static const char *const rows[4] = {"AUTO\tWIDE WINGS WHEN THE GAME STAYS LARGE",
                                          "GAME FIRST\tTHE GAME AS LARGE AS IT GOES",
                                          "BALANCED\tROOMY WINGS, THE GAME STILL LARGE",
                                          "WINGS FIRST\tWIDE WINGS, A SMALLER GAME"};
      int l = menu_pick("WING ROOM", rows, 4, st.wingRoom == 3 ? 0 : st.wingRoom + 1);
      if (l < 0)
        save = false;
      else
        st.wingRoom = l == 0 ? 3 : l - 1;
      break;
    }
    case ROW_STRETCH:
      st.stretch = !st.stretch;
      break;
    case ROW_WING_LOOK: {
      static const char *const rows[POCKET_WING_LOOKS] = {"COMPACT\tSMALLER, MORE FITS", "COMFY\tAS WORKED OUT",
                                                          "BIG\tLARGER, EASIER FROM A DISTANCE"};
      int l = menu_pick("WING LOOK", rows, POCKET_WING_LOOKS, st.wingLook);
      if (l < 0)
        save = false;
      else
        st.wingLook = l;
      break;
    }
    case ROW_FULL:
      fullscreen_toggle(); // saves FULLSCREEN itself
      save = false;
      break;
    case ROW_ORIENT: {
      int o = menu_pick("ORIENTATION", ORIENTATIONS, PD_ORIENTATIONS, st.orientation);
      if (o < 0)
        save = false;
      else
        st.orientation = o;
      break;
    }
    case ROW_TOUCH:
      st.gameTouch = st.gameTouch == PD_GAME_TOUCH_OFF ? PD_GAME_TOUCH_DS_SCREEN : PD_GAME_TOUCH_OFF;
      break;
    case ROW_LAYOUT:
      st.buttonLayout = (st.buttonLayout + 1) % PD_BUTTON_LAYOUTS;
      break;
    case ROW_BEZEL_STYLE: {
      static const char *const rows[BEZEL_STYLES] = {"BLACK\tNEAR BLACK, FOR OLED", "GLOW\tTHE GAME'S COLOURS, FAINT",
                                                     "THEME\tTHE THEME'S WINDOW"};
      int b = menu_pick("BEZEL STYLE", rows, BEZEL_STYLES,
                        bezel_style_resolve(st.bezelStyle, pocket_background_resolve(st.background, st.colors ==
                                                               POCKET_COLORS_OLED, st.darkMode) == POCKET_BACKGROUND_BLACK));
      if (b < 0)
        save = false;
      else
        st.bezelStyle = b;
      break;
    }
    case ROW_SCREEN_LAYOUT:
      homeRow = pick;
      page = PAGE_SET_BEZEL;
      save = false;
      break;
    case ROW_LIST_TOP:
    case ROW_LIST_BOTTOM:
    case ROW_LIST_WING:
    case ROW_LIST_BATTLE_TOP:
    case ROW_LIST_BATTLE_BOTTOM:
    case ROW_LIST_BATTLE_WING:
      layoutRow = pick;
      listOpen = id - ROW_LIST_TOP;
      page = PAGE_SET_LIST;
      selectedOn[PAGE_SET_LIST] = -1;
      save = false;
      break;
    case ROW_PARTY_TYPES:
      st.partyTypes = !st.partyTypes;
      break;
    case ROW_LIST_RESET:
      if (listOpen == LIST_WING)
        st.wingCount = -1;
      else if (listOpen == LIST_BATTLE_WING)
        st.battleWingCount = -1;
      else if (listOpen == LIST_BATTLE_TOP || listOpen == LIST_BATTLE_BOTTOM)
        st.barCount[list_bar(listOpen)] = -1;
      else {
        const int *d = listOpen ? BEZEL_DEFAULT_BOTTOM : BEZEL_DEFAULT_TOP;
        int dn = listOpen ? (int)(sizeof BEZEL_DEFAULT_BOTTOM / sizeof *d) : (int)(sizeof BEZEL_DEFAULT_TOP / sizeof *d);
        layout_store(&st, listOpen, d, dn);
      }
      notice("Back to the default");
      break;
    default:
      if (id >= ROW_LIST_ITEM0 && id < ROW_LIST_ITEM0 + 64) {
        // An item: on or off, or moved within what's on (the map and the buttons only on or off).
        int item = id - ROW_LIST_ITEM0;
        int list[BEZEL_LIST_MAX > PD_WING_LIST_MAX ? BEZEL_LIST_MAX : PD_WING_LIST_MAX];
        int count = layout_list(&st, listOpen, list), at = layout_at(list, count, item);
        bool pinned = listOpen == LIST_WING && (item == PD_WS_MAP || item == PD_WS_BUTTONS);
        static const char *const ON[] = {"TURN OFF", "MOVE UP", "MOVE DOWN", "MOVE TO THE TOP"};
        static const char *const OFF[] = {"TURN ON"};
        int act = at < 0 ? menu_pick(layout_name(listOpen, item), OFF, 1, 0)
                         : menu_pick(layout_name(listOpen, item), ON, pinned ? 1 : 4, 0);
        int cap = list_is_wing(listOpen) ? PD_WING_LIST_MAX : BEZEL_LIST_MAX;
        if (act < 0)
          save = false;
        else if (at < 0) {
          if (count < cap)
            list[count++] = item;
        } else if (act == 0) {
          memmove(&list[at], &list[at + 1], (size_t)(count - at - 1) * sizeof *list);
          count--;
        } else {
          int to = act == 1 ? at - 1 : act == 2 ? at + 1 : 0;
          if (to < 0 || to >= count)
            to = at;
          int t = list[at];
          memmove(to < at ? &list[to + 1] : &list[at], to < at ? &list[to] : &list[at + 1],
                  (size_t)(to < at ? at - to : to - at) * sizeof *list);
          list[to] = t;
        }
        if (save) {
          layout_store(&st, listOpen, list, count);
          focusId = id; // the cursor stays on the item it moved
        }
        break;
      }
      if (id >= ROW_HOTKEY0 && id < ROW_HOTKEY0 + PD_HOTKEY_BUTTONS) {
        int b = id - ROW_HOTKEY0;
        int action = menu_pick(HOTKEY_BUTTON_NAMES[b], HOTKEY_ACTIONS, PD_HOTKEY_ACTIONS, st.hotkeys[b]);
        if (action < 0)
          save = false;
        else
          st.hotkeys[b] = (unsigned char)action;
      } else
        save = false;
    }
    if (save) {
      pd_settings_save(&st, path);
      controls_load();
    }
  }
  return -1;
}
static bool update_prompt(void) {
  enum updater_state st = updater_state();
  if (st != UPDATER_AVAILABLE && st != UPDATER_DOWNLOADING &&
      !((st == UPDATER_READY || st == UPDATER_FAILED) && !updateDone))
    return false;
  bool install = updater_can_install();
  const char *rows[3] = {install ? "UPDATE NOW" : "OPEN DOWNLOAD PAGE", "LATER", "SKIP THIS VERSION"};
  int rowCount = st == UPDATER_AVAILABLE ? 3 : st == UPDATER_DOWNLOADING ? 0 : 1;
  if (st != UPDATER_AVAILABLE)
    rows[0] = "OK";
  if (rowCount) {
    if (down(UP))
      updateSel = (updateSel + rowCount - 1) % rowCount;
    if (down(DOWN))
      updateSel = (updateSel + 1) % rowCount;
    if (updateSel >= rowCount)
      updateSel = 0;
  }
  // POCKET_UPDATE_ACCEPT=1 picks UPDATE NOW by itself (a headless check of the download and install).
  if (st == UPDATER_AVAILABLE && getenv("POCKET_UPDATE_ACCEPT") && install) {
    updater_download();
  } else if (st == UPDATER_AVAILABLE && down(A)) {
    if (updateSel == 0)
      install ? updater_download() : updater_open_page();
    else if (updateSel == 1)
      updater_later();
    else
      updater_skip();
    updateSel = 0;
  } else if (st == UPDATER_AVAILABLE && down(B)) {
    updater_later();
  } else if ((st == UPDATER_READY || st == UPDATER_FAILED) && (down(A) || down(B))) {
    updateDone = true;
  }
  struct pd_canvas *c = &canvas;
  pd_dim(c, 0xC0);
  int w = c->w - 40 < 280 ? c->w - 40 : 280, h = 74 + 18 * (rowCount ? rowCount : 1);
  int x = (c->w - w) / 2, y = (c->h - h) / 2;
  gridModalRect = (SDL_Rect){x - 2, y - 2, w + 4, h + 4};
  int f = pd_title_box(c, x, y, w, h, PD_TITLE_FILL);
  int ix = x + f + 6, iw = w - 2 * f - 12, ty = y + f + 4;
  pd_text(c, ix, ty, st == UPDATER_READY ? "UPDATE READY" : st == UPDATER_FAILED ? "UPDATE FAILED" : "UPDATE AVAILABLE",
          PD_TITLE_TEXT, PD_TITLE_SHADOW);
  char line[160];
  if (st == UPDATER_AVAILABLE)
    snprintf(line, sizeof line, "PokeDaisy %s is out. You have %s.", updater_latest(), updater_current());
  else if (st == UPDATER_DOWNLOADING)
    snprintf(line, sizeof line, "Downloading PokeDaisy %s...", updater_latest());
  else if (st == UPDATER_READY)
    snprintf(line, sizeof line, "PokeDaisy %s goes in when you close PokeDaisy. Your saves and settings stay.",
             updater_latest());
  else
    snprintf(line, sizeof line, "The download didn't work. PokeDaisy asks again next time.");
  pd_text_wrap(c, ix, ty + 20, iw, 14, 3, line, PD_LABEL, PD_LABEL_SHADOW);
  int ry = y + h - f - 6 - 18 * (rowCount ? rowCount : 1);
  for (int i = 0; i < rowCount; i++) {
    bool cursor = i == updateSel;
    if (cursor)
      pd_fill(c, ix - 2, ry + 18 * i, iw + 4, 18, PD_ROW_SELECTED);
    pd_text(c, ix + 4, pd_text_y(ry + 18 * i, 18), rows[i], cursor ? PD_VALUE : PD_LABEL,
            cursor ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
  }
  return true;
}
/* RENAME's keyboard: the D-pad walks the keys and A presses one, a tap presses one, and a real keyboard just types.
 * B deletes (with nothing left, leaves), Y is a space, START is DONE. True with the name in BUF. */
static bool text_entry(const char *title, char *buf, size_t size) {
  static const char *const ROWS[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL'", "ZXCVBNM-.&"};
  static const char *const WIDE[] = {"aA", "SPACE", "DEL", "DONE"};
  enum { COLS = 10, ROW_N = 4 };
  int row = 1, col = 0, pressedKey = -1;
  bool lower = false, done = false, ok = false;
  held = 0;
  textEntry = true;
  textTyped[0] = 0;
  textBackspaces = 0;
  textEnter = textEscape = false;
  SDL_StartTextInput();
  int keyX[ROW_N + 1][COLS], keyW[ROW_N + 1][COLS], keyY[ROW_N + 1], keyH = 0;
  memset(keyX, 0, sizeof keyX);
  memset(keyW, 0, sizeof keyW);
  memset(keyY, 0, sizeof keyY);
#define DEL_ONE()                                                                                                      \
  do {                                                                                                                 \
    size_t n_ = strlen(buf);                                                                                           \
    while (n_ && ((unsigned char)buf[n_ - 1] & 0xC0) == 0x80)                                                          \
      n_--;                                                                                                            \
    if (n_)                                                                                                            \
      n_--;                                                                                                            \
    buf[n_] = 0;                                                                                                       \
  } while (0)
#define ADD(str)                                                                                                       \
  do {                                                                                                                 \
    size_t n_ = strlen(buf), a_ = strlen(str);                                                                         \
    if (n_ + a_ < size && n_ + a_ < 60)                                                                                \
      memcpy(buf + n_, str, a_ + 1);                                                                                   \
  } while (0)
  while (running && !done) {
    events();
    repeat_directions();
    menu_script();
    // A real keyboard.
    if (*textTyped) {
      ADD(textTyped);
      textTyped[0] = 0;
    }
    for (; textBackspaces > 0; textBackspaces--)
      DEL_ONE();
    if (textEnter)
      done = ok = true;
    if (textEscape || down(MENU))
      done = true;
    int press = -1; // a key: row * COLS + col
    if (down(LEFT))
      col = row == ROW_N ? (col + 3) % 4 : (col + COLS - 1) % COLS;
    if (down(RIGHT))
      col = row == ROW_N ? (col + 1) % 4 : (col + 1) % COLS;
    if (down(UP) && row > 0) {
      if (row == ROW_N)
        col = col * COLS / 4 + 1;
      row--;
    }
    if (down(DOWN) && row < ROW_N) {
      row++;
      if (row == ROW_N)
        col = col * 4 / COLS;
    }
    if (down(A))
      press = row * COLS + col;
    if (down(Y))
      ADD(" ");
    if (down(X)) // the pad's X (Xbox's left face button) is a backspace
      DEL_ONE();
    if (down(START))
      done = ok = true;
    if (down(B)) {
      if (*buf)
        DEL_ONE();
      else
        done = true;
    }
    for (int i = 0; i < tapCount; i++) {
      int hit = -1;
      for (int r = 0; r <= ROW_N && hit < 0; r++)
        for (int c2 = 0; c2 < (r == ROW_N ? 4 : COLS); c2++)
          if (taps[i].y >= keyY[r] && taps[i].y < keyY[r] + keyH && taps[i].x >= keyX[r][c2] &&
              taps[i].x < keyX[r][c2] + keyW[r][c2])
            hit = r * COLS + c2;
      if (taps[i].down)
        pressedKey = hit;
      else if (hit >= 0 && hit == pressedKey) {
        press = hit;
        row = hit / COLS;
        col = hit % COLS;
        pressedKey = -1;
      } else
        pressedKey = -1;
    }
    tapCount = 0;
    if (press >= 0) {
      int r = press / COLS, c2 = press % COLS;
      if (r < ROW_N) {
        char ch[2] = {ROWS[r][c2], 0};
        if (lower && ch[0] >= 'A' && ch[0] <= 'Z')
          ch[0] = (char)(ch[0] - 'A' + 'a');
        ADD(ch);
      } else if (c2 == 0)
        lower = !lower;
      else if (c2 == 1)
        ADD(" ");
      else if (c2 == 2)
        DEL_ONE();
      else
        done = ok = true;
    }
    // Drawn.
    bool dark = pd_dark;
    uint32_t page = dark ? 0x111318 : 0xE9ECF1, keyC = dark ? 0x1C1F26 : 0xFFFFFF,
             edge = dark ? 0x2A2E37 : 0xD3D8E0, text = dark ? 0xF2F4F8 : 0x1E2229, muted = dark ? 0x8B93A1 : 0x6B7280;
    const uint32_t ring = 0xFF8A1E;
    companion_canvas();
    pd_fill(&canvas, 0, 0, canvas.w, canvas.h, page);
    pd_heading_fit(&canvas, 10, 6, canvas.w - 20, title, ring, PD_NO_SHADOW);
    int fy = 28;
    pd_round_rect(&canvas, 10, fy, canvas.w - 20, 24, 5, edge);
    pd_round_rect(&canvas, 11, fy + 1, canvas.w - 22, 22, 4, keyC);
    char shown[64];
    const char *field = buf;
    if (textMask) { // a password: a star for each character
      size_t m = 0;
      for (const char *q = buf; *q && m + 1 < sizeof shown; q++)
        if (((unsigned char)*q & 0xC0) != 0x80)
          shown[m++] = '*';
      shown[m] = 0;
      field = shown;
    }
    int tw = pd_text_width(field);
    int tx = 16 + (tw > canvas.w - 40 ? canvas.w - 40 - tw : 0);
    pd_clip(&canvas, 12, fy, canvas.w - 24, 24);
    pd_text(&canvas, tx, fy + 2, field, text, PD_NO_SHADOW);
    if ((SDL_GetTicks() / 500) % 2)
      pd_fill(&canvas, tx + tw + 1, fy + 5, 1, 14, ring);
    pd_unclip(&canvas);
    int gap = 3, top = fy + 32, bottom = canvas.h - 18;
    keyH = (bottom - top - gap * ROW_N) / (ROW_N + 1);
    if (keyH > 26)
      keyH = 26;
    int kw = (canvas.w - 20 - gap * (COLS - 1)) / COLS;
    for (int r = 0; r <= ROW_N; r++) {
      keyY[r] = top + r * (keyH + gap);
      int n = r == ROW_N ? 4 : COLS;
      for (int c2 = 0; c2 < n; c2++) {
        int w = r == ROW_N ? (canvas.w - 20 - gap * 3) / 4 : kw;
        int x = 10 + c2 * (w + gap);
        keyX[r][c2] = x;
        keyW[r][c2] = w;
        bool on = r == row && c2 == col;
        pd_round_rect(&canvas, x, keyY[r], w, keyH, 4, on ? ring : edge);
        pd_round_rect(&canvas, x + 1, keyY[r] + 1, w - 2, keyH - 2, 3, on ? ring : keyC);
        char ch[8];
        if (r < ROW_N) {
          ch[0] = ROWS[r][c2];
          ch[1] = 0;
          if (lower && ch[0] >= 'A' && ch[0] <= 'Z')
            ch[0] = (char)(ch[0] - 'A' + 'a');
        } else
          snprintf(ch, sizeof ch, "%s", WIDE[c2 & 3]);
        pd_text(&canvas, x + (w - pd_text_width(ch)) / 2, keyY[r] + (keyH - 16) / 2 - 1, ch, on ? 0xFFFFFF : text,
                PD_NO_SHADOW);
      }
    }
    pd_text_fit(&canvas, 10, canvas.h - 18, canvas.w - 20, "A KEY   B/X DELETE   Y SPACE   START DONE   OR JUST TYPE",
                muted, PD_NO_SHADOW);
    if (menu_frame())
      break;
  }
#undef DEL_ONE
#undef ADD
  SDL_StopTextInput();
  textEntry = false;
  held = 0;
  size_t n = strlen(buf);
  while (n && buf[n - 1] == ' ')
    buf[--n] = 0;
  return ok && running;
}

/* The landing page: the games as a grid of box art (game_grid.h), GAME FOLDERS and SETTINGS in its top bar. A game's
 * options (X, a right click, a long press): PLAY, BOX ART, RENAME. */
static int grid_system(const char *file) {
  const char *dot = strrchr(file, '.');
  if (!dot)
    return BOXART_GBA;
  if (!strcasecmp(dot, ".nds"))
    return BOXART_NDS;
  if (!strcasecmp(dot, ".gbc"))
    return BOXART_GBC;
  if (!strcasecmp(dot, ".gb"))
    return BOXART_GB;
  return BOXART_GBA;
}
static void grid_stem(const char *file, char *out, size_t size) {
  snprintf(out, size, "%s", file);
  char *dot = strrchr(out, '.');
  if (dot && dot != out)
    *dot = 0;
}
/* SET BOX ART from a path or link: quotes and spaces round it dropped (a copied path keeps them). */
static void grid_set_art(const char *stem, const char *source) {
  char s[1500];
  snprintf(s, sizeof s, "%s", source);
  char *p = s;
  while (*p == ' ' || *p == '"' || *p == '\'' || *p == '\t')
    p++;
  size_t n = strlen(p);
  while (n && (p[n - 1] == ' ' || p[n - 1] == '"' || p[n - 1] == '\'' || p[n - 1] == '\n' || p[n - 1] == '\r' ||
               p[n - 1] == '\t'))
    p[--n] = 0;
  if (!*p) {
    notice("Nothing to use: copy an image file's path or a link first");
    return;
  }
  char err[160] = "";
  if (boxart_set_custom(stem, p, err, sizeof err))
    notice("Box art set");
  else
    notice(*err ? err : "Couldn't use that picture");
}
enum { GRID_BAR_SYNC, GRID_BAR_FOLDERS, GRID_BAR_SETTINGS, GRID_BAR_THEME };
struct library {
  struct rom_list games;
  struct game_grid_item *items;
  struct boxart_game *art;
  char **stems;
  char (*versions)[48];
  struct game_grid grid;
  int count;
};
/* The bar's buttons; the last is DARK MODE's switch, a moon while dark (GAME_GRID_ICON_MOON) and a sun while light. */
static const char *LIBRARY_BAR[] = {"SYNC BOX ART", "GAME FOLDERS", "SETTINGS", GAME_GRID_ICON_MOON};
static void library_free(struct library *l) {
  for (int i = 0; i < l->count; i++)
    free(l->stems[i]);
  free(l->stems);
  free(l->items);
  free(l->art);
  free(l->versions);
  rom_list_free(&l->games);
  l->stems = NULL;
  l->items = NULL;
  l->art = NULL;
  l->versions = NULL;
  l->count = 0;
}
static void library_load(struct library *l) {
#ifdef POCKET_DS
  const bool listNds = true;
#else
  const bool listNds = false;
#endif
  int keep = l->grid.selected, focus = l->grid.focus, scroll = l->grid.scrollY;
  library_free(l);
  rom_list_load(&l->games, root, listNds);
  int count = l->games.count;
  l->items = calloc((size_t)count + 1, sizeof *l->items);
  l->art = calloc((size_t)count + 1, sizeof *l->art);
  l->stems = calloc((size_t)count + 1, sizeof *l->stems);
  l->versions = calloc((size_t)count + 1, sizeof *l->versions);
  if (!l->items || !l->art || !l->stems || !l->versions)
    count = 0;
  for (int i = 0; i < count; i++) {
    const char *label = l->games.labels[i];
    grid_stem(label, l->art[i].stem, sizeof l->art[i].stem);
    snprintf(l->art[i].title, sizeof l->art[i].title, "%s", l->games.names[i]);
    l->art[i].system = grid_system(label);
    // The ROM's folder: BOX ART LOCATION's BESIDE GAMES keeps its art in a "boxart" folder there.
    snprintf(l->art[i].dir, sizeof l->art[i].dir, "%s", l->games.paths[i]);
    char *cut = strrchr(l->art[i].dir, '/'), *back = strrchr(l->art[i].dir, '\\');
    if (back > cut)
      cut = back;
    if (cut)
      *cut = 0;
    else
      l->art[i].dir[0] = 0;
    l->stems[i] = strdup(l->art[i].stem);
    if (!l->stems[i]) {
      count = i;
      break;
    }
    l->art[i].hack = !game_is_original(label);
    boxart_base_of_rom(l->games.paths[i], l->art[i].base, sizeof l->art[i].base, &l->art[i].baseSystem);
    game_version(label, l->versions[i], sizeof l->versions[i]);
    l->items[i] = (struct game_grid_item){l->games.names[i], l->versions[i], l->art[i].system, l->art[i].hack,
                                          boxart_is_base(l->stems[i]), GAME_SUPPORT_UNKNOWN, NULL, 0};
  }
  l->count = count;
  // The art folders (and older versions' art copied into them, once); then which tiles show a base game's box.
  boxart_games(l->art, count);
  for (int i = 0; i < count; i++)
    l->items[i].banner = boxart_is_base(l->stems[i]);
  // FULL / PARTLY / PLAY ONLY, worked out in the background (cached in DATA).
  game_support_start(root, (const char *const *)l->games.paths, count, POCKET_VERSION);
  game_grid_init(&l->grid, l->items, count);
  l->grid.selected = keep >= 0 && keep < count ? keep : 0;
  l->grid.focus = count ? focus : GAME_GRID_TILES;
  l->grid.scrollY = scroll;
  l->grid.title = "PokeDaisy"; // the platform stays in the window title and SETTINGS' HOST row
  l->grid.bar = LIBRARY_BAR;
  l->grid.barCount = 4;
  l->grid.hint = "A PLAY   X OPTIONS   Y SETTINGS   B QUIT";
  l->grid.emptyPath = ROMS_PATH;
  char path[1100];
  snprintf(path, sizeof path, "%s/settings.ini", root);
  struct pd_settings st;
  pd_settings_load(&st, path);
  pd_look_apply(NULL, &st); // DARK MODE on the landing page too
  LIBRARY_BAR[GRID_BAR_THEME] = st.darkMode ? GAME_GRID_ICON_MOON : GAME_GRID_ICON_SUN;
  // Never from a headless check unless asked (POCKET_BOXART): those don't go online.
  if (st.boxartAuto && (!menuFrames || getenv("POCKET_BOXART")))
    boxart_queue(l->art, count);
}
/* ---- SAVE EDITOR (save_edit.h): the library's EDIT SAVE, every page a pick-list, nothing written until SAVE ---- */
static void editor_say(const char *title, const char *text) {
  static const char *const ok[] = {"OK"};
  menu_pick_hint(title, text, ok, 1, 0);
}
/* A number typed on RENAME's keyboard: false for B or anything that isn't MIN..MAX. */
static bool editor_number(const char *title, long now, long min, long max, long *out) {
  char buf[16];
  snprintf(buf, sizeof buf, "%ld", now);
  if (!text_entry(title, buf, sizeof buf))
    return false;
  char *end;
  long v = strtol(buf, &end, 10);
  if (!*buf || *end || v < min || v > max) {
    char why[96];
    snprintf(why, sizeof why, "Type a number from %ld to %ld.", min, max);
    editor_say(title, why);
    return false;
  }
  *out = v;
  return true;
}
static bool editor_contains(const char *name, const char *query) {
  for (const char *s = name; *s; s++) {
    const char *a = s, *b = query;
    while (*a && *b && SDL_toupper((unsigned char)*a) == SDL_toupper((unsigned char)*b))
      a++, b++;
    if (!*b)
      return true;
  }
  return !*query;
}
/* A species / move / item picked by part of its name: its id, 0 for NONE (when allowed), -1 for B. */
static int editor_search(struct save_edit *e, const char *title, const char *(*name)(struct save_edit *, int), int max,
                         bool none) {
  char query[32] = "";
  if (!text_entry(title, query, sizeof query))
    return -1;
  enum { CAP = 200 };
  static const char *rows[CAP + 1];
  static int ids[CAP + 1];
  int n = 0;
  if (none)
    rows[n] = "(NONE)", ids[n++] = 0;
  for (int id = 1; id <= max && n < CAP; id++) {
    const char *s = name(e, id);
    if (s && editor_contains(s, query))
      rows[n] = s, ids[n++] = id;
  }
  if (n == (none ? 1 : 0)) {
    editor_say(title, "Nothing this game has goes by that name.");
    return -1;
  }
  int pick = menu_pick(title, rows, n, 0);
  return pick >= 0 ? ids[pick] : -1;
}

/* A move for M: the ones it can learn first, by name; a search for any other (refused on apply where the game is retail
 * and it can't learn it). */
static int editor_pick_move(struct save_edit *e, const struct se_mon *m, bool none) {
  const char *title = "WHICH MOVE?";
  enum { CAP = 512 };
  static uint16_t moves[CAP];
  static const char *rows[CAP + 2];
  static int ids[CAP + 2];
  static char names[CAP][24]; /* copied: a name can come back in one shared buffer */
  bool complete = false;
  int count = e->ops->learnable ? e->ops->learnable(e, m->species, m->level, moves, CAP, &complete) : -1;
  if (count <= 0)
    return editor_search(e, title, e->ops->move_name, e->ops->move_max(e), none);
  int n = 0, fixed;
  rows[n] = "SEARCH ALL MOVES", ids[n++] = -1;
  if (none)
    rows[n] = "(NONE)", ids[n++] = 0;
  fixed = n;
  for (int i = 0; i < count; i++) {
    const char *got = e->ops->move_name(e, moves[i]);
    if (!got)
      continue;
    snprintf(names[i], sizeof names[i], "%s", got);
    const char *s = names[i];
    int at = n++;
    while (at > fixed && strcmp(rows[at - 1], s) > 0)
      rows[at] = rows[at - 1], ids[at] = ids[at - 1], at--;
    rows[at] = s, ids[at] = moves[i];
  }
  int pick = menu_pick_hint(title,
                            complete ? "The moves it can learn by now. One it knows from an egg stays."
                                     : "From the game's own lists; it may learn others too.",
                            rows, n, fixed < n ? fixed : 0);
  if (pick < 0)
    return -1;
  return ids[pick] >= 0 ? ids[pick] : editor_search(e, title, e->ops->move_name, e->ops->move_max(e), none);
}

/* PRESETS: the sets imported into <data>/presets (EDIT SAVE > PRESETS > IMPORT SETS), read once. */
static struct preset_list presetSets;
static bool presetsRead;
static void presets_folder(char *out, size_t n) { snprintf(out, n, "%s/presets", root); }
static struct preset_list *presets_all(bool reload) {
  if (!presetsRead || reload) {
    char dir[1100];
    presets_folder(dir, sizeof dir);
    presets_free(&presetSets);
    presets_load_dir(dir, &presetSets);
    presetsRead = true;
  }
  return &presetSets;
}
/* GET SETS' progress: a screen drawn once before each generation's download (it blocks the frame while it runs). */
static void presets_progress(int gen, void *ctx) {
  (void)ctx;
  static char hint[96];
  snprintf(hint, sizeof hint, "Generation %d of %d. Smogon's sets, as the pkmn project publishes them.", gen,
           PRESETS_GENS);
  struct pd_menu m;
  pd_menu_init(&m, "GETTING SETS", NULL, 0);
  m.hint = hint;
  companion_canvas();
  pd_menu_draw(&m, &canvas);
  present();
}
/* Downloads every generation's sets into <data>/presets/smogon and reloads them. Headless looks stay offline unless
 * POCKET_PRESETS_ONLINE is set. */
static void presets_get(bool quiet) {
  if (menuFrames && !getenv("POCKET_PRESETS_ONLINE")) {
    if (!quiet)
      notice("Headless looks don't go online");
    return;
  }
  char dir[1100], msg[120];
  presets_folder(dir, sizeof dir);
  presets_fetch(dir, presets_progress, NULL, msg, sizeof msg);
  presets_all(true);
  notice(msg); // a toast: the key that started it would close a message at once
}
/* What of a set to copy (PRESETS and PASTE SET): all of it, or some of its parts, so a Pokemon can take a set's
 * nature, IVs and EVs and keep its own moves. */
static int editor_parts(void) {
  static int parts = PART_ALL; // remembered for the next set
  static const char *const names[] = {"NATURE", "IVS", "EVS", "MOVES", "HELD ITEM", "ABILITY"};
  int at = 0;
  while (running) {
    static char text[6][48];
    const char *rows[9];
    rows[0] = "COPY ALL OF IT";
    rows[1] = "STATS ONLY\tNATURE, IVS, EVS";
    for (int i = 0; i < 6; i++) {
      snprintf(text[i], sizeof text[i], "%s\t%s", names[i], parts & (1 << i) ? "COPY" : "KEEP MINE");
      rows[2 + i] = text[i];
    }
    rows[8] = "COPY THESE";
    at = menu_pick_hint("COPY WHAT?", "A switches a part between COPY and KEEP MINE.", rows, 9, at);
    if (at < 0)
      return 0;
    if (at == 0)
      return PART_ALL;
    if (at == 1)
      return PART_STATS;
    if (at == 8)
      return parts;
    parts ^= 1 << (at - 2);
  }
  return 0;
}
/* P filled into M as far as the game allows, and a list of what it couldn't. */
static void editor_fill(struct save_edit *e, const struct preset *p, struct se_mon *m) {
  int parts = editor_parts();
  if (!parts)
    return;
  struct se_mon next = *m;
  struct preset_notes notes;
  int filled = preset_apply(e, p, &next, &notes);
  if (filled)
    preset_take(m, &next, parts);
  if (!notes.count) {
    notice("Filled in: KEEP THESE CHANGES keeps it");
    return;
  }
  const char *rows[PRESET_NOTES];
  for (int i = 0; i < notes.count; i++)
    rows[i] = notes.line[i];
  menu_pick_hint(filled ? "WHAT THIS GAME CAN'T DO" : "NOTHING FILLED IN",
                 filled ? "The rest is filled in. KEEP THESE CHANGES keeps it." : NULL, rows, notes.count, 0);
}
/* M's sets for this game's generation (OTHER GENERATIONS for the rest); one picked fills M in. */
static void editor_presets(struct save_edit *e, struct se_mon *m) {
  struct preset_list *all = presets_all(false);
  if (!all->count) { // nothing imported yet: the sets come down by themselves the first time
    presets_get(true);
    all = presets_all(false);
  }
  int gen = se_generation(e);
  char species[32];
  const char *s = e->ops->species_name(e, m->species);
  snprintf(species, sizeof species, "%s", s ? s : "");
  enum { CAP = 400 };
  static char text[CAP][112];
  static const char *rows[CAP + 1];
  static int ids[CAP + 1];
  bool others = false;
  while (running) {
    int n = 0, here = 0, elsewhere = 0;
    rows[n] = "", ids[n++] = -1; // the generations row, named below
    for (int i = 0; i < all->count; i++) {
      const struct preset *p = &all->v[i];
      if (!preset_for(p, species))
        continue;
      bool same = !p->gen || p->gen == gen;
      here += same, elsewhere += !same;
      if (same == others || n > CAP)
        continue;
      const char *fmt = p->format;
      if (!strncasecmp(fmt, "Gen ", 4)) { // "Gen 3 OU" as "OU"
        fmt += 4;
        while (isdigit((unsigned char)*fmt))
          fmt++;
        while (*fmt == ' ')
          fmt++;
      }
      char head[48] = "";
      if (others)
        snprintf(head, sizeof head, "GEN %d ", p->gen);
      if (snprintf(text[n - 1], sizeof text[0], "%s%s%s%s\t%s", head, fmt, *fmt && p->name[0] ? ": " : "", p->name,
                   p->item) >= (int)sizeof text[0])
        text[n - 1][sizeof text[0] - 1] = 0; // cut short
      rows[n] = text[n - 1], ids[n++] = i;
    }
    if (!here && !elsewhere) {
      editor_say("PRESETS", all->count ? "No sets for this Pokemon yet. EDIT SAVE > PRESETS > IMPORT SETS adds more."
                                       : "No sets yet. EDIT SAVE > PRESETS > GET SETS downloads them, and IMPORT "
                                         "SETS adds your own.");
      return;
    }
    if (!others && !here) {
      others = true;
      continue;
    }
    static char toggle[64];
    if (others)
      snprintf(toggle, sizeof toggle, here ? "BACK TO GEN %d\t%d SETS" : "NO GEN %d SETS FOR IT", gen, here);
    else
      snprintf(toggle, sizeof toggle, "OTHER GENERATIONS\t%d SETS", elsewhere);
    int first = 0;
    if (others || elsewhere)
      rows[0] = toggle;
    else
      first = 1;
    int pick = menu_pick_hint(others ? "PRESETS: OTHER GENERATIONS" : "PRESETS",
                              others ? "What this game can't do is listed after." : "A fills the set in; nothing is kept "
                                                                                     "until KEEP THESE CHANGES.",
                              rows + first, n - first, n - first > 1 ? 1 : 0);
    if (pick < 0)
      return;
    pick += first;
    if (ids[pick] < 0) {
      if (others ? here > 0 : true)
        others = !others;
      continue;
    }
    editor_fill(e, &all->v[ids[pick]], m);
    return;
  }
}

static const char *const STAT_NAME[6] = {"HP", "ATTACK", "DEFENSE", "SPEED", "SP. ATK", "SP. DEF"};
/* IVs / EVs: a slider per stat. */
static void editor_ivev(struct se_mon *m, bool ivs) {
  static const char *const *const statName = STAT_NAME;
  long lv;
    // A slider per stat: LEFT / RIGHT step it (held, it repeats), the shoulders step by 10, Y is its most, X is 0,
    // A types it. EVs keep to 252 a stat and 510 in all (a typed value can go to the game's own 255).
    int *values = ivs ? m->ivs : m->evs, stat = 0, key;
    while (running) {
      int total = 0;
      for (int i = 0; i < 6; i++)
        total += values[i];
      char lines[8][64], hint[160];
      const char *r8[8];
      for (int i = 0; i < 6; i++) {
        int top = ivs ? 31 : 252, n = (values[i] > top ? top : values[i]) * 10 / top;
        char bar[11];
        for (int b = 0; b < 10; b++)
          bar[b] = b < n ? '#' : '.';
        bar[10] = 0;
        snprintf(lines[i], sizeof lines[i], "%s%s%s %3d", statName[i], "\t", bar, values[i]);
        r8[i] = lines[i];
      }
      r8[6] = ivs ? "ALL TO 31" : "ALL TO 0";
      r8[7] = "DONE";
      if (ivs)
        snprintf(hint, sizeof hint, "LEFT/RIGHT 1, LB/RB 10, Y 31, X 0, A types it.");
      else
        snprintf(hint, sizeof hint, "TOTAL %d / 510 (%d left). LEFT/RIGHT 1, LB/RB 10, Y most, X 0, A types it.",
                 total, total < 510 ? 510 - total : 0);
      unsigned keys = 1u << LEFT | 1u << RIGHT | 1u << L | 1u << R | 1u << L2 | 1u << R2 | 1u << X | 1u << Y;
      stat = menu_pick_keys(ivs ? "IVs" : "EVs", hint, r8, 8, stat, keys, &key);
      if (stat < 0 || stat == 7)
        break;
      if (stat == 6) {
        for (int i = 0; i < 6; i++)
          values[i] = ivs ? 31 : 0;
        continue;
      }
      int now = values[stat], others = total - now;
      int most = ivs ? 31 : (510 - others < 252 ? 510 - others : 252);
      if (most < 0)
        most = 0;
      int next = now;
      if (key == LEFT)
        next = now - 1;
      else if (key == RIGHT)
        next = now + 1;
      else if (key == L || key == L2)
        next = now - 10;
      else if (key == R || key == R2)
        next = now + 10;
      else if (key == Y)
        next = most;
      else if (key == X)
        next = 0;
      else if (editor_number(statName[stat], now, 0, ivs ? 31 : 255, &lv)) {
        next = (int)lv;
        if (!ivs && next > 510 - others) {
          char why[96];
          snprintf(why, sizeof why, "EVs come to 510 in all: this one can have %d.", 510 - others);
          editor_say(statName[stat], why);
          next = now;
        }
        values[stat] = next;
        continue;
      }
      if (next > most && next > now)
        next = most > now ? most : now; // a step up stops at the cap, never pushing an old value down
      if (next < 0)
        next = 0;
      values[stat] = next;
    }
}
static int editorRole = -1; // the QUICK EVS role picked last, tagged in the nature list
/* How often each nature turns up in SPECIES' sets (this generation's, or any when it has none). */
static void editor_set_natures(struct save_edit *e, int species, int counts[25]) {
  memset(counts, 0, 25 * sizeof *counts);
  const char *name = e->ops->species_name(e, species);
  struct preset_list *all = presets_all(false);
  if (!name || !all->count)
    return;
  int gen = se_generation(e);
  for (int pass = 0; pass < 2; pass++) {
    int found = 0;
    for (int i = 0; i < all->count; i++) {
      const struct preset *pr = &all->v[i];
      if ((pass == 0 && pr->gen && pr->gen != gen) || !preset_for(pr, name))
        continue;
      for (int k = 0; k < 25; k++)
        if (!strcasecmp(pr->nature, se_nature_name(k)))
          counts[k]++, found++;
    }
    if (found)
      return;
  }
}
/* NATURE: the suggested ones first (from the sets, else the base stats), each with its +/-, then all 25. */
static void editor_natures(struct save_edit *e, struct se_mon *m) {
  int base[6], counts[25], rec[3] = {0}, recN = 0, roleNature = -1;
  bool fromSets[3] = {false};
  bool known = e->ops->base_stats && e->ops->base_stats(e, m->species, base);
  if (known) {
    editor_set_natures(e, m->species, counts);
    recN = st_recommend(base, counts, rec, fromSets);
    if (editorRole >= 0) {
      int evs[6];
      st_role_spread(editorRole, base, evs, &roleNature);
    }
  }
  static char text[3 + 25][96];
  const char *rows[3 + 25];
  int ids[3 + 25], n = 0;
  char fx[32];
  for (int i = 0; i < recN; i++) {
    snprintf(text[n], sizeof text[n], "%s  %s\t%s%s", se_nature_name(rec[i]), st_nature_effect(rec[i], fx, sizeof fx),
             fromSets[i] ? "USED IN SETS" : "SUITS ITS STATS", rec[i] == roleNature ? ", FITS THE ROLE" : "");
    rows[n] = text[n], ids[n] = rec[i], n++;
  }
  for (int k = 0; k < 25; k++) {
    snprintf(text[n], sizeof text[n], "%s%s\t%s", se_nature_name(k), k == m->nature ? " (NOW)" : "",
             st_nature_effect(k, fx, sizeof fx));
    rows[n] = text[n], ids[n] = k, n++;
  }
  int at = recN ? 0 : m->nature; // the suggestions in view first
  int k = menu_pick_hint("NATURE", recN ? "The first ones suit this Pokemon; all 25 follow." : NULL, rows, n, at);
  if (k >= 0)
    m->nature = ids[k];
}
/* QUICK EVS: a spread by role, the best fit for the base stats first; its nature offered after. */
static void editor_roles(struct save_edit *e, struct se_mon *m) {
  int base[6];
  if (!e->ops->base_stats || !e->ops->base_stats(e, m->species, base)) {
    editor_say("QUICK EVS", "This game's base stats aren't known.");
    return;
  }
  int best = st_best_role(base);
  static char text[ROLE_COUNT][128];
  const char *rows[ROLE_COUNT];
  char spread[64], fx[32];
  for (int r = 0; r < ROLE_COUNT; r++) {
    int evs[6], nature;
    st_role_spread(r, base, evs, &nature);
    snprintf(text[r], sizeof text[r], "%s%s\t%s, %s", st_role_name(r), r == best ? " (BEST FIT)" : "",
             st_spread_text(evs, spread, sizeof spread), se_nature_name(nature));
    rows[r] = text[r];
  }
  int r = menu_pick_hint("QUICK EVS", "A sets the EVs; the role's nature is offered next.", rows, ROLE_COUNT, best);
  if (r < 0)
    return;
  int evs[6], nature;
  st_role_spread(r, base, evs, &nature);
  memcpy(m->evs, evs, sizeof evs);
  editorRole = r;
  if (m->nature != nature) {
    char q[64];
    snprintf(q, sizeof q, "%s (%s)", se_nature_name(nature), st_nature_effect(nature, fx, sizeof fx));
    const char *yn[2] = {"YES", "KEEP MINE"};
    if (menu_pick_hint("SET THE NATURE TOO?", q, yn, 2, 0) == 0)
      m->nature = nature;
  }
}
/* STATS: each stat's base, now, least and most (31 IV, 252 EV, this nature), worked out again after every change. */
static void editor_stats(struct save_edit *e, struct se_mon *m, const struct se_mon *was) {
  int base[6];
  if (!e->ops->base_stats || !e->ops->base_stats(e, m->species, base)) {
    editor_say("STATS", "This game's base stats aren't known.");
    return;
  }
  int at = 0;
  while (running) {
    int now[6], before[6];
    st_stats(base, m->ivs, m->evs, m->level, m->nature, now);
    st_stats(base, was->ivs, was->evs, was->level, was->nature, before);
    static char text[10][128];
    const char *rows[10];
    char fx[32];
    int up = st_nature_up(m->nature), down = st_nature_down(m->nature);
    for (int i = 0; i < 6; i++) {
      int lo = st_stat_min(base, i, m->level, m->nature), hi = st_stat_max(base, i, m->level, m->nature);
      int fill = hi > lo ? (now[i] - lo) * 10 / (hi - lo) : 10;
      char bar[9], change[24] = "";
      fill = fill * 8 / 10;
      for (int b = 0; b < 8; b++)
        bar[b] = b < fill ? '#' : '.';
      bar[8] = 0;
      if (now[i] != before[i])
        snprintf(change, sizeof change, " WAS %d", before[i]);
      snprintf(text[i], sizeof text[i], "%s%s  BASE %d\t%d%s  %s  %d-%d", STAT_NAME[i],
               i == up ? " +" : i == down ? " -" : "", base[i], now[i], change, bar, lo, hi);
      rows[i] = text[i];
    }
    snprintf(text[6], sizeof text[6], "NATURE: %s\t%s", se_nature_name(m->nature),
             st_nature_effect(m->nature, fx, sizeof fx));
    rows[6] = text[6];
    rows[7] = "QUICK EVS\tBY ROLE";
    rows[8] = "IVs\tA SLIDER EACH";
    rows[9] = "EVs\tA SLIDER EACH";
    char hint[96];
    snprintf(hint, sizeof hint, "Lv%d. The range runs from 0 IVs and EVs to 31 and 252, with this nature.",
             m->level);
    at = menu_pick_hint("STATS: NOW, THEN LEAST TO MOST", hint, rows, 10, at);
    if (at < 0)
      return;
    if (at < 6 || at == 9)
      editor_ivev(m, false);
    else if (at == 6)
      editor_natures(e, m);
    else if (at == 7)
      editor_roles(e, m);
    else
      editor_ivev(m, true);
  }
}
/* QUICK SET: a level in one press, PERFECT (31 IVs, the best-fit role's EVs and nature), Hidden Power's type, and
 * the Pokemon copied as a Showdown set. */
static void editor_quick(struct save_edit *e, struct se_mon *m) {
  int at = 0;
  while (running) {
    int base[6], role = -1, evs[6], nature = 0;
    bool known = e->ops->base_stats && e->ops->base_stats(e, m->species, base);
    if (known) {
      role = st_best_role(base);
      st_role_spread(role, base, evs, &nature);
    }
    static char perfect[96], hidden[64];
    snprintf(perfect, sizeof perfect, "PERFECT\t31 IVS, %s EVS, %s", known ? st_role_name(role) : "?",
             se_nature_name(nature));
    snprintf(hidden, sizeof hidden, "HIDDEN POWER\t%s, %d POWER", preset_hp_type_name(preset_hp_type(m->ivs)),
             preset_hp_power(m->ivs));
    const char *rows[5];
    int ids[5], n = 0;
    rows[n] = "LEVEL 50", ids[n++] = 0;
    rows[n] = "LEVEL 100", ids[n++] = 1;
    if (known)
      rows[n] = perfect, ids[n++] = 2;
    rows[n] = hidden, ids[n++] = 3;
    rows[n] = "COPY AS SHOWDOWN\tFOR PASTE SET OR A TEAM BUILDER", ids[n++] = 4;
    at = menu_pick_hint("QUICK SET", "Nothing is kept until KEEP THESE CHANGES.", rows, n, at);
    if (at < 0)
      return;
    switch (ids[at]) {
    case 0:
    case 1:
      m->level = ids[at] ? 100 : 50;
      notice(ids[at] ? "Level 100" : "Level 50");
      break;
    case 2:
      for (int i = 0; i < 6; i++)
        m->ivs[i] = 31;
      if (role == ROLE_TRICK_ROOM)
        m->ivs[3] = 0; // the slowest it can be
      memcpy(m->evs, evs, sizeof evs);
      m->nature = nature;
      editorRole = role;
      notice("Perfect: KEEP THESE CHANGES keeps it");
      break;
    case 3: {
      static char names[16][48];
      const char *types[16];
      for (int k = 0; k < 16; k++) {
        int iv[6];
        memcpy(iv, m->ivs, sizeof iv);
        preset_hp_set(iv, k);
        snprintf(names[k], sizeof names[k], "%s\t%d POWER", preset_hp_type_name(k), preset_hp_power(iv));
        types[k] = names[k];
      }
      int k = menu_pick_hint("HIDDEN POWER TYPE", "Its IVs move by one point at most to give the type.", types, 16,
                             preset_hp_type(m->ivs));
      if (k >= 0)
        preset_hp_set(m->ivs, k);
      break;
    }
    default: {
      char text[1024];
      preset_export(e, m, text, sizeof text);
      if (!getenv("POCKET_PASTE") && SDL_SetClipboardText(text) != 0) {
        editor_say("COPY AS SHOWDOWN", "The clipboard couldn't take it.");
        break;
      }
      // The set shown a line a row, as it went to the clipboard.
      const char *lines[16];
      int count = 0;
      for (char *s = text; *s && count < 16;) {
        lines[count++] = s;
        char *end = strchr(s, '\n');
        if (!end)
          break;
        *end = 0;
        s = end + 1;
      }
      menu_pick_hint("COPIED TO THE CLIPBOARD", "PASTE SET or a team builder reads it back.", lines, count, 0);
      break;
    }
    }
  }
}
static bool editor_move(struct save_edit *e, int box, int slot);
/* LEGALITY: M's issues (legality.h), each with FIX where that's safe, and FIX ALL. */
static void editor_legality(struct save_edit *e, struct se_mon *m) {
  int at = 0;
  while (running) {
    struct lg_issue v[LG_MAX];
    int n = lg_check(e, m, v, LG_MAX), fixable = 0;
    if (!n) {
      editor_say("LEGAL", "Nothing here the game couldn't have made.");
      return;
    }
    static char text[LG_MAX + 1][128];
    const char *rows[LG_MAX + 1];
    int count = 0;
    for (int i = 0; i < n; i++) {
      fixable += v[i].fixable;
      snprintf(text[count], sizeof text[count], "%s\t%s", v[i].text, v[i].fixable ? "FIX" : v[i].hard ? "" : "CHECK");
      rows[count] = text[count];
      count++;
    }
    if (fixable)
      rows[count++] = "FIX ALL";
    at = menu_pick_hint(lg_soft_only(v, n) ? "CHECK" : "NOT LEGAL",
                        "A on FIX puts it right (KEEP THESE CHANGES keeps it). CHECK may be fine: an event's move.",
                        rows, count, at);
    if (at < 0)
      return;
    if (at == n)
      lg_fix_all(e, m);
    else if (v[at].fixable)
      lg_fix(e, m, &v[at]);
  }
}
static void editor_mon(struct save_edit *e, int box, int slot) {
  struct se_mon m;
  if (!e->ops->mon(e, box, slot, &m))
    return;
  struct se_mon was = m; // as the save has it: STATS shows what changed
  editorRole = -1;
  int selected = 0;
  while (running) {
    enum { R_SPECIES, R_NICK, R_LEVEL, R_EXP, R_NATURE, R_ABILITY, R_HELD, R_FRIEND, R_MOVE, R_PPUPS = R_MOVE + 4,
           R_IVS, R_EVS, R_STATS, R_ROLES, R_QUICK, R_SHINY, R_OT, R_FROM, R_MAX_IVS, R_PRESET, R_PASTE, R_LEGAL, R_APPLY, R_MOVE_IT, R_COUNT };
    static char text[R_COUNT][96];
    const char *rows[R_COUNT];
    int ids[R_COUNT], shown = 0;
    const char *a1 = NULL, *a2 = NULL;
    e->ops->abilities(e, m.species, &a1, &a2);
    const char *species = e->ops->species_name(e, m.species), *held = m.held ? e->ops->item_name(e, m.held) : "NONE";
    snprintf(text[R_SPECIES], 96, "POKEMON: %s", species ? species : "?");
    snprintf(text[R_NICK], 96, "NAME: %s", m.nick);
    snprintf(text[R_LEVEL], 96, "LEVEL: %d", m.level);
    snprintf(text[R_EXP], 96, "EXP: %ld", m.exp);
    snprintf(text[R_FRIEND], 96, "FRIENDSHIP: %d", m.friendship);
    snprintf(text[R_PPUPS], 96, "PP BOOSTS: %d/%d/%d/%d", m.ppUps[0], m.ppUps[1], m.ppUps[2], m.ppUps[3]);
    {
      char fx[32];
      snprintf(text[R_NATURE], 96, "NATURE: %s\t%s", se_nature_name(m.nature), st_nature_effect(m.nature, fx, sizeof fx));
    }
    snprintf(text[R_STATS], 96, "STATS\tBASE, NOW, LEAST, MOST");
    snprintf(text[R_ROLES], 96, "QUICK EVS\tBY ROLE");
    snprintf(text[R_QUICK], 96, "QUICK SET\tLEVEL, PERFECT, HIDDEN POWER, SHOWDOWN");
    snprintf(text[R_ABILITY], 96, "ABILITY: %s", m.ability ? (a2 ? a2 : "?") : (a1 ? a1 : "?"));
    snprintf(text[R_HELD], 96, "HOLDS: %s", held ? held : "?");
    for (int i = 0; i < 4; i++) {
      const char *mv = m.moves[i] ? e->ops->move_name(e, m.moves[i]) : "-";
      snprintf(text[R_MOVE + i], 96, "MOVE %d: %s", i + 1, mv ? mv : "?");
    }
    snprintf(text[R_IVS], 96, "IVs: %d/%d/%d/%d/%d/%d", m.ivs[0], m.ivs[1], m.ivs[2], m.ivs[3], m.ivs[4], m.ivs[5]);
    snprintf(text[R_EVS], 96, "EVs: %d/%d/%d/%d/%d/%d", m.evs[0], m.evs[1], m.evs[2], m.evs[3], m.evs[4], m.evs[5]);
    snprintf(text[R_SHINY], 96, "SHINY: %s", m.shiny ? "YES" : "NO");
    snprintf(text[R_OT], 96, "OT: %s  %05ld", m.ot, m.tid);
    const char *game = se_game_name(m.metGame);
    if (m.metLevel)
      snprintf(text[R_FROM], 96, "FROM: %s, MET AT LEVEL %d", game ? game : "ANOTHER GAME", m.metLevel);
    else
      snprintf(text[R_FROM], 96, "FROM: %s, HATCHED", game ? game : "ANOTHER GAME");
    snprintf(text[R_MAX_IVS], 96, "ALL IVs TO 31");
    snprintf(text[R_PRESET], 96, "PRESETS\tFILL IN A SET");
    snprintf(text[R_PASTE], 96, "PASTE SET\tA COPIED SHOWDOWN SET");
    {
      struct lg_issue v[LG_MAX];
      int n = lg_check(e, &m, v, LG_MAX);
      if (!n)
        snprintf(text[R_LEGAL], 96, "LEGALITY\tLEGAL");
      else
        snprintf(text[R_LEGAL], 96, "LEGALITY\t%s: %d TO LOOK AT", lg_soft_only(v, n) ? "CHECK" : "NOT LEGAL", n);
    }
    snprintf(text[R_APPLY], 96, "KEEP THESE CHANGES");
    snprintf(text[R_MOVE_IT], 96, "MOVE IT\tTO ANOTHER SLOT OR BOX");
    for (int i = 0; i < R_COUNT; i++)
      if ((m.extras || (i != R_EXP && i != R_FRIEND && i != R_PPUPS)) && (m.origin || (i != R_OT && i != R_FROM)) &&
          (i != R_MOVE_IT || se_trade_ready(e)) && ((i != R_STATS && i != R_ROLES) || e->ops->base_stats))
        rows[shown] = text[i], ids[shown++] = i;
    int row = menu_pick_hint(m.nick, "B leaves this Pokemon as it was.", rows, shown, selected);
    if (row < 0)
      return;
    selected = row;
    int pick = ids[row];
    long v;
    int id;
    switch (pick) {
    case R_SPECIES:
      if ((id = editor_search(e, "WHICH POKEMON?", e->ops->species_name, e->ops->species_max(e), false)) > 0)
        m.species = id, m.ability = 0;
      break;
    case R_NICK: {
      char name[32];
      snprintf(name, sizeof name, "%s", m.nick);
      if (text_entry("NAME", name, sizeof name) && *name)
        snprintf(m.nick, sizeof m.nick, "%s", name);
      break;
    }
    case R_LEVEL:
      if (editor_number("LEVEL", m.level, 1, 100, &v))
        m.level = (int)v;
      break;
    case R_EXP: // checked against the level's range when it's kept
      if (editor_number("EXP", m.exp, 0, 2000000, &v))
        m.exp = v;
      break;
    case R_FRIEND:
      if (editor_number("FRIENDSHIP", m.friendship, 0, 255, &v))
        m.friendship = (int)v;
      break;
    case R_PPUPS: {
      int k = 0;
      while (running) {
        char lines[4][96];
        const char *r4[4];
        int at[4], count = 0;
        for (int i = 0; i < 4; i++) {
          if (!m.moves[i])
            continue;
          const char *mv = e->ops->move_name(e, m.moves[i]);
          snprintf(lines[count], sizeof lines[count], "%s\t%d BOOSTS", mv ? mv : "?", m.ppUps[i]);
          r4[count] = lines[count];
          at[count++] = i;
        }
        k = menu_pick_hint("PP BOOSTS", "A adds one, up to 3, then back to none.", r4, count, k);
        if (k < 0)
          break;
        m.ppUps[at[k]] = (m.ppUps[at[k]] + 1) % 4;
      }
      break;
    }
    case R_NATURE:
      editor_natures(e, &m);
      break;
    case R_ABILITY: {
      if (!a2) {
        editor_say("ABILITY", "This Pokemon has only one ability.");
        break;
      }
      const char *both[2] = {a1 ? a1 : "?", a2};
      int k = menu_pick("ABILITY", both, 2, m.ability);
      if (k >= 0)
        m.ability = k;
      break;
    }
    case R_HELD:
      if ((id = editor_search(e, "HOLDS WHAT?", e->ops->item_name, e->ops->item_max(e), true)) >= 0)
        m.held = id;
      break;
    case R_IVS:
    case R_EVS:
      editor_ivev(&m, pick == R_IVS);
      break;
    case R_STATS:
      editor_stats(e, &m, &was);
      break;
    case R_ROLES:
      editor_roles(e, &m);
      break;
    case R_QUICK:
      editor_quick(e, &m);
      break;
    case R_SHINY:
      m.shiny = !m.shiny;
      break;
    case R_MAX_IVS:
      for (int i = 0; i < 6; i++)
        m.ivs[i] = 31;
      break;
    case R_PRESET:
      editor_presets(e, &m);
      break;
    case R_PASTE: {
      // POCKET_PASTE stands in for the clipboard in headless looks.
      const char *fake = getenv("POCKET_PASTE");
      char *clip = fake ? NULL : SDL_HasClipboardText() ? SDL_GetClipboardText() : NULL;
      const char *text = fake ? fake : clip;
      struct preset_list one = {0};
      if (text && *text)
        presets_parse("paste.txt", text, strlen(text), se_generation(e), &one);
      if (clip)
        SDL_free(clip);
      if (!one.count)
        editor_say("PASTE SET", "Copy a set first, as Showdown exports it: the Pokemon, then a line for each move "
                                "starting with -.");
      else
        editor_fill(e, &one.v[0], &m);
      presets_free(&one);
      break;
    }
    case R_APPLY: {
      char err[160] = "";
      if (e->ops->set_mon(e, box, slot, &m, err, sizeof err)) {
        notice("Changed: SAVE CHANGES writes it");
        return;
      }
      editor_say("CAN'T CHANGE IT", err);
      break;
    }
    case R_LEGAL:
      editor_legality(e, &m);
      break;
    case R_OT:
    case R_FROM:
      break; // shown, not changed
    case R_MOVE_IT: {
      struct se_mon now;
      if (e->ops->mon(e, box, slot, &now) && memcmp(&now, &m, sizeof m)) {
        editor_say("MOVE IT", "KEEP THESE CHANGES first, or press B to leave them, then move it.");
        break;
      }
      if (editor_move(e, box, slot))
        return;
      break;
    }
    default:
      if (pick >= R_MOVE && pick < R_MOVE + 4 &&
          (id = editor_pick_move(e, &m, pick != R_MOVE)) >= 0) {
        if (id != m.moves[pick - R_MOVE])
          m.ppUps[pick - R_MOVE] = 0; // a new move starts with none
        m.moves[pick - R_MOVE] = id;
      }
      break;
    }
  }
}
/* A slot's row: "3  NAME  Lv12  SPECIES", "3  EGG", "3  BAD EGG" or "3  (EMPTY)". False for an empty slot. */
static bool editor_slot_label(struct save_edit *e, int box, int s, char *out, size_t n) {
  struct se_mon m;
  if (!e->ops->mon(e, box, s, &m)) {
    bool bad = se_trade_ready(e) && se_slot(e, box, s) < 0;
    snprintf(out, n, "%d  %s", s + 1, bad ? "BAD EGG" : "(EMPTY)");
    return bad;
  }
  const char *sp = e->ops->species_name(e, m.species);
  if (m.egg)
    snprintf(out, n, "%d  EGG", s + 1);
  else {
    struct lg_issue v[LG_MAX];
    int issues = lg_check(e, &m, v, LG_MAX);
    snprintf(out, n, "%d  %s  Lv%d%s%s\t%s", s + 1, m.nick, m.level, sp && strcmp(sp, m.nick) ? "  " : "",
             sp && strcmp(sp, m.nick) ? sp : "", !issues ? "LEGAL" : lg_soft_only(v, issues) ? "CHECK" : "NOT LEGAL");
  }
  return true;
}
/* MOVE IT: where the Pokemon at BOX / SLOT goes, the party or a box and then a slot there (a Pokemon in it swaps
 * places, an empty one takes it), the game's PC rules applied (save_edit.h's se_move). True when it moved. */
static bool editor_move(struct save_edit *e, int box, int slot) {
  int boxes = e->ops->boxes(e), slots = e->ops->box_slots(e), where = 0;
  if (boxes > 32)
    boxes = 32;
  if (slots > 32)
    slots = 32;
  while (running) {
    static char names[33][32];
    const char *rows[33];
    int count = se_party_count(e);
    snprintf(names[0], sizeof names[0], "PARTY\t%d/6", count);
    rows[0] = names[0];
    for (int b = 0; b < boxes; b++) {
      int used = 0;
      for (int s = 0; s < slots; s++)
        used += se_slot(e, b, s) != 0;
      snprintf(names[b + 1], sizeof names[b + 1], "BOX %d\t%d/%d", b + 1, used, slots);
      rows[b + 1] = names[b + 1];
    }
    where = menu_pick_hint("MOVE IT WHERE?", "A Pokemon in the slot you pick swaps places with it.", rows, boxes + 1,
                           where);
    if (where < 0)
      return false;
    int to = where ? where - 1 : SE_PARTY, n = to == SE_PARTY ? (count < 6 ? count + 1 : 6) : slots;
    static char text[32][80];
    const char *srows[32];
    for (int s = 0; s < n; s++) {
      if (to == box && s == slot)
        snprintf(text[s], sizeof text[s], "%d  (THIS ONE)", s + 1);
      else
        editor_slot_label(e, to, s, text[s], sizeof text[s]);
      srows[s] = text[s];
    }
    char title[24];
    if (to == SE_PARTY)
      snprintf(title, sizeof title, "PARTY");
    else
      snprintf(title, sizeof title, "BOX %d", to + 1);
    int s = menu_pick(title, srows, n, 0);
    if (s < 0)
      continue;
    char err[160] = "";
    if (se_move(e, box, slot, to, s, err, sizeof err)) {
      notice("Moved: SAVE CHANGES writes it");
      return true;
    }
    editor_say("CAN'T MOVE IT", err);
  }
  return false;
}
/* A box's (or the party's) Pokemon to pick from. */
static void editor_box(struct save_edit *e, int box, const char *title) {
  int slots = box == SE_PARTY ? 6 : e->ops->box_slots(e), selected = 0;
  while (running) {
    static char text[32][80];
    const char *rows[32];
    int ids[32], n = 0;
    for (int s = 0; s < slots && n < 32; s++) {
      if (!editor_slot_label(e, box, s, text[n], sizeof text[n]))
        continue;
      rows[n] = text[n];
      ids[n++] = s;
    }
    if (!n) {
      editor_say(title, "No Pokemon here.");
      return;
    }
    int pick = menu_pick(title, rows, n, selected);
    if (pick < 0)
      return;
    selected = pick;
    struct se_mon m;
    if (!e->ops->mon(e, box, ids[pick], &m))
      editor_say(title, "A Bad Egg can't be changed or moved.");
    else if (!m.egg)
      editor_mon(e, box, ids[pick]);
    else if (!se_trade_ready(e))
      editor_say(title, "Eggs aren't changed: hatch it first.");
    else {
      static const char *const egg[] = {"MOVE IT\tTO ANOTHER SLOT OR BOX"};
      if (menu_pick_hint("EGG", "Eggs aren't changed: hatch it first. You can move it.", egg, 1, 0) == 0)
        editor_move(e, box, ids[pick]);
    }
  }
}
/* An item for pocket P from the game's own list (P -1: any that goes in a pocket the editor changes): the pocket's
 * items to scroll through, SEARCH BY NAME first to narrow them. A game that can't tell its pockets searches every
 * item instead. The item, or -1. */
static int recentItems[8], recentCount; // the items added last, newest first, offered at the top of ADD AN ITEM
static void editor_recent(int id) {
  int n = 0, keep[8];
  keep[n++] = id;
  for (int i = 0; i < recentCount && n < 8; i++)
    if (recentItems[i] != id)
      keep[n++] = recentItems[i];
  memcpy(recentItems, keep, sizeof keep);
  recentCount = n;
}
/* HOW MANY?: left / right step it by one (held, it repeats), LB / RB by ten, Y is the most, X the least, A keeps it;
 * TYPE A NUMBER for the keyboard. False for B. */
static bool editor_count(const char *title, int now, int min, int max, int *out) {
  int v = now < min ? min : now > max ? max : now, at = 0, key;
  while (running) {
    static char line[48], most[48];
    snprintf(line, sizeof line, "x%d\tKEEP IT", v);
    snprintf(most, sizeof most, "MOST\tx%d", max);
    const char *rows[3] = {line, most, "TYPE A NUMBER"};
    unsigned keys = 1u << LEFT | 1u << RIGHT | 1u << L | 1u << R | 1u << L2 | 1u << R2 | 1u << X | 1u << Y;
    at = menu_pick_keys(title, "LEFT/RIGHT 1, LB/RB 10, Y most, X least, A keeps it.", rows, 3, at, keys, &key);
    if (at < 0)
      return false;
    if (key == LEFT)
      v--;
    else if (key == RIGHT)
      v++;
    else if (key == L || key == L2)
      v -= 10;
    else if (key == R || key == R2)
      v += 10;
    else if (key == Y)
      v = max;
    else if (key == X)
      v = min;
    else if (at == 0) {
      *out = v;
      return true;
    } else if (at == 1) {
      *out = max;
      return true;
    } else {
      long typed;
      if (editor_number(title, v, min, max, &typed)) {
        *out = (int)typed;
        return true;
      }
    }
    v = v < min ? min : v > max ? max : v;
    at = 0;
  }
  return false;
}
static int editor_pick_item(struct save_edit *e, int p, const char *title) {
  if (!e->ops->item_pocket)
    return editor_search(e, title, e->ops->item_name, e->ops->item_max(e), false);
  enum { CAP = 1024 };
  static const char *rows[CAP + 1];
  static int ids[CAP + 1];
  char query[32] = "";
  bool searched = false;
  while (running) {
    int n = 0, max = e->ops->item_max(e);
    if (!searched)
      rows[n] = "SEARCH BY NAME", ids[n++] = -1;
    static char recent[8][64];
    for (int i = 0; i < recentCount && !searched; i++) {
      int id = recentItems[i], ip = id <= max ? e->ops->item_pocket(e, id) : -1;
      const char *s = ip >= 0 && (p < 0 || ip == p) ? e->ops->item_name(e, id) : NULL;
      if (!s)
        continue;
      snprintf(recent[i], sizeof recent[i], "%s\tADDED LAST", s);
      rows[n] = recent[i], ids[n++] = id;
    }
    for (int id = 1; id <= max && n < CAP; id++) {
      int ip = e->ops->item_pocket(e, id);
      if (ip < 0 || (p >= 0 ? ip != p : e->ops->pocket_locked(e, ip) != NULL))
        continue;
      const char *s = e->ops->item_name(e, id);
      if (s && (!searched || editor_contains(s, query)))
        rows[n] = s, ids[n++] = id;
    }
    if (n == (searched ? 0 : 1)) {
      editor_say(title, searched ? "Nothing here goes by that name." : "This game has no items for that pocket.");
      if (!searched)
        return -1;
      searched = false;
      continue;
    }
    int pick = menu_pick(title, rows, n, 0);
    if (pick < 0) {
      if (!searched)
        return -1;
      searched = false; // B from a search: the whole list again
      continue;
    }
    if (ids[pick] >= 0)
      return ids[pick];
    query[0] = 0;
    searched = text_entry(title, query, sizeof query) && *query;
  }
  return -1;
}
/* ID into pocket P: onto its stack when the bag has one (up to a full stack), else into the first empty slot. */
static void editor_add_item(struct save_edit *e, int p, int id) {
  char err[160] = "", line[128];
  const char *name = e->ops->item_name(e, id);
  int slots = e->ops->pocket_slots(e, p), have = -1, empty = -1, max = e->ops->item_max_count(e, p, id);
  struct se_item it;
  for (int i = 0; i < slots; i++)
    if (e->ops->pocket_item(e, p, i, &it)) {
      if (it.id == id && have < 0)
        have = i;
      else if (!it.id && empty < 0)
        empty = i;
    }
  int count;
  if (have >= 0) {
    e->ops->pocket_item(e, p, have, &it);
    if (it.count >= max) {
      editor_say(name ? name : "CAN'T ADD IT", "That stack is full already.");
      return;
    }
    if (!editor_count("ADD HOW MANY?", 1, 1, max - it.count, &count))
      return;
    count += it.count;
  } else if (empty < 0) {
    editor_say("CAN'T ADD IT", "That pocket is full: take something out first.");
    return;
  } else if (!editor_count("HOW MANY?", 1, 1, max, &count))
    return;
  if (!e->ops->set_item(e, p, have >= 0 ? have : empty, id, count, err, sizeof err)) {
    editor_say("CAN'T ADD IT", err);
    return;
  }
  snprintf(line, sizeof line, "%.40s x%d in %.20s: SAVE CHANGES writes it", name ? name : "?", count,
           e->ops->pocket_name(e, p));
  notice(line);
  editor_recent(id);
}
/* ADD ALL: every item of pocket P the bag lacks, COUNT each (or a full stack), into its empty slots. */
static void editor_add_all(struct save_edit *e, int p, int count) {
  int slots = e->ops->pocket_slots(e, p), max = e->ops->item_max(e), added = 0, left = 0;
  bool have[4096] = {false};
  struct se_item it;
  for (int i = 0; i < slots; i++)
    if (e->ops->pocket_item(e, p, i, &it) && it.id > 0 && it.id < 4096)
      have[it.id] = true;
  int slot = 0;
  for (int id = 1; id <= max && id < 4096; id++) {
    if (have[id] || e->ops->item_pocket(e, id) != p)
      continue;
    while (slot < slots && (!e->ops->pocket_item(e, p, slot, &it) || it.id))
      slot++;
    if (slot >= slots) {
      left++;
      continue;
    }
    int full = e->ops->item_max_count(e, p, id), n = count > 0 && count < full ? count : full;
    char err[160];
    if (e->ops->set_item(e, p, slot, id, n, err, sizeof err))
      added++;
  }
  char line[128];
  if (left)
    snprintf(line, sizeof line, "%d added; %d more didn't fit: the pocket is full.", added, left);
  else
    snprintf(line, sizeof line, added ? "%d added: SAVE CHANGES writes them." : "The bag has all of them already.",
             added);
  editor_say("ADD ALL", line);
}
/* MAX ALL: every stack in pocket P at its most. How many changed. */
static int editor_max_all(struct save_edit *e, int p) {
  int slots = e->ops->pocket_slots(e, p), changed = 0;
  for (int i = 0; i < slots; i++) {
    struct se_item it;
    char err[160];
    if (!e->ops->pocket_item(e, p, i, &it) || !it.id)
      continue;
    int full = e->ops->item_max_count(e, p, it.id);
    if (it.count < full && e->ops->set_item(e, p, i, it.id, full, err, sizeof err))
      changed++;
  }
  return changed;
}
static void editor_pocket(struct save_edit *e, int p) {
  const char *title = e->ops->pocket_name(e, p), *locked = e->ops->pocket_locked(e, p);
  int selected = 0;
  while (running) {
    int slots = e->ops->pocket_slots(e, p), n = 0;
    static char text[200][80];
    static const char *rows[201];
    static int ids[201];
    for (int i = 0; i < slots && n < 200; i++) {
      struct se_item it;
      if (!e->ops->pocket_item(e, p, i, &it) || !it.id)
        continue;
      const char *name = e->ops->item_name(e, it.id);
      snprintf(text[n], 80, "%s  x%d", name ? name : "?", it.count);
      rows[n] = text[n];
      ids[n++] = i;
    }
    int used = n;
    bool tms = strstr(title, "TM") != NULL, berries = strstr(title, "BERR") != NULL;
    if (!locked) {
      rows[n] = "ADD AN ITEM";
      ids[n++] = -1;
      if (used)
        rows[n] = "MAX ALL\tEVERY STACK AT ITS MOST", ids[n++] = -2;
      if (tms || berries)
        rows[n] = tms ? "ADD ALL TMS\tONE EACH" : "ADD ALL BERRIES\tA FULL STACK EACH", ids[n++] = -3;
    }
    if (!n) {
      editor_say(title, locked);
      return;
    }
    char hint[96];
    if (!locked)
      snprintf(hint, sizeof hint, used >= slots ? "%d of %d slots used: the pocket is full." : "%d of %d slots used.",
               used, slots);
    int pick = menu_pick_hint(title, locked ? locked : hint, rows, n, selected);
    if (pick < 0)
      return;
    selected = pick;
    if (locked)
      continue;
    char err[160] = "";
    if (ids[pick] == -2) {
      int changed = editor_max_all(e, p);
      notice(changed ? "Every stack at its most: SAVE CHANGES writes it" : "Every stack is full already");
      continue;
    }
    if (ids[pick] == -3) {
      editor_add_all(e, p, tms ? 1 : 0);
      continue;
    }
    if (ids[pick] < 0) {
      int id = editor_pick_item(e, p, "ADD WHICH ITEM?");
      if (id > 0)
        editor_add_item(e, p, id);
      continue;
    }
    struct se_item it;
    e->ops->pocket_item(e, p, ids[pick], &it);
    static const char *const what[] = {"CHANGE HOW MANY", "TAKE IT OUT"};
    int k = menu_pick(rows[pick], what, 2, 0);
    int c;
    if (k == 0 && editor_count("HOW MANY?", it.count, 1, e->ops->item_max_count(e, p, it.id), &c) &&
        !e->ops->set_item(e, p, ids[pick], it.id, c, err, sizeof err))
      editor_say("CAN'T CHANGE IT", err);
    if (k == 1 && !e->ops->set_item(e, p, ids[pick], 0, 0, err, sizeof err))
      editor_say("CAN'T TAKE IT OUT", err);
  }
}
/* The save the game reads: a DS game's beside it (melonDS) or in DATA/saves, a GBA game's as play_game finds it. */
static void editor_save_path(const char *rom, char *out, size_t size) {
  const char *name = rom;
  for (const char *s = rom; *s; s++)
    if (*s == '/' || *s == '\\')
      name = s + 1;
  const char *ext = strrchr(name, '.');
  if (ext && !strcasecmp(ext, ".nds")) {
    snprintf(out, size, "%.*s.sav", (int)(ext - rom), rom);
    if (pocket_exists(out))
      return;
  }
  snprintf(out, size, "%s/saves/%s.sav", root, name);
  char ini[1024], beside[1024];
  snprintf(ini, sizeof ini, "%s/settings.ini", root);
  struct pd_settings st;
  pd_settings_load(&st, ini);
  if (!(ext && !strcasecmp(ext, ".nds")) && st.savesBeside && rom_in_added_folder(root, rom)) {
    rom_save_beside(beside, sizeof beside, rom);
    if (pocket_exists(beside) || !pocket_exists(out))
      snprintf(out, size, "%s", beside);
  }
}
/* A resume state from before an edit would bring the old save back with CONTINUE: set it aside. <0 when it couldn't. */
static int editor_set_aside(const char *rom) {
  char states[1100];
  const char *file = rom;
  for (const char *s = rom; *s; s++)
    if (*s == '/' || *s == '\\')
      file = s + 1;
  snprintf(states, sizeof states, "%s/states", root);
  return se_set_resume_aside(states, file, time(NULL));
}
/* CHANGES: what differs from the save on disk (ORIG, opened again), a row each; A on one puts it back as it was. */
enum { CH_MONEY, CH_COINS, CH_ITEM, CH_MON, CH_TIME, CH_BADGES, CH_DEX };
struct editor_change {
  int kind, a, b; // a pocket and slot, or a box and slot
  char text[112];
};
static int editor_mon_diff(const struct se_mon *x, const struct se_mon *y, char *out, size_t n) {
  out[0] = 0;
  size_t k = 0;
#define DIFF(cond, name)                                                                                               \
  if ((cond) && k < n)                                                                                                 \
    k += (size_t)snprintf(out + k, n - k, "%s%s", k ? ", " : "", name);
  DIFF(x->species != y->species, "SPECIES")
  DIFF(strcmp(x->nick, y->nick), "NAME")
  DIFF(x->level != y->level || x->exp != y->exp, "LEVEL")
  DIFF(x->nature != y->nature, "NATURE")
  DIFF(x->ability != y->ability, "ABILITY")
  DIFF(x->held != y->held, "ITEM")
  DIFF(memcmp(x->moves, y->moves, sizeof x->moves) || memcmp(x->ppUps, y->ppUps, sizeof x->ppUps), "MOVES")
  DIFF(memcmp(x->ivs, y->ivs, sizeof x->ivs), "IVS")
  DIFF(memcmp(x->evs, y->evs, sizeof x->evs), "EVS")
  DIFF(x->shiny != y->shiny, "SHINY")
  DIFF(x->friendship != y->friendship, "FRIENDSHIP")
  DIFF(x->hp != y->hp || x->maxHp != y->maxHp, "HP")
  DIFF(x->egg != y->egg || strcmp(x->ot, y->ot) || x->tid != y->tid || x->metGame != y->metGame ||
           x->metLevel != y->metLevel,
       "ORIGIN")
#undef DIFF
  return (int)k;
}
static int editor_diff(struct save_edit *e, struct save_edit *orig, struct editor_change *out, int cap) {
  int n = 0;
  const struct se_ops *o = e->ops;
  int h1, m1, s1, h2, m2, s2;
  if (se_play_time(e, &h1, &m1, &s1) && se_play_time(orig, &h2, &m2, &s2) && (h1 != h2 || m1 != m2) && n < cap) {
    out[n].kind = CH_TIME;
    snprintf(out[n++].text, sizeof out[0].text, "PLAY TIME\t%d:%02d TO %d:%02d", h2, m2, h1, m1);
  }
  if (se_badges(e) != se_badges(orig) && n < cap) {
    out[n].kind = CH_BADGES;
    snprintf(out[n++].text, sizeof out[0].text, "BADGES\tCHANGED");
  }
  if (se_dex_count(e, true) != se_dex_count(orig, true) && n < cap) {
    out[n].kind = CH_DEX;
    snprintf(out[n++].text, sizeof out[0].text, "POKEDEX\t%d TO %d CAUGHT", se_dex_count(orig, true),
             se_dex_count(e, true));
  }
  if (o->money(e) != o->money(orig) && n < cap) {
    out[n].kind = CH_MONEY;
    snprintf(out[n++].text, sizeof out[0].text, "MONEY\t%ld TO %ld", o->money(orig), o->money(e));
  }
  if (o->coins(e) != o->coins(orig) && n < cap) {
    out[n].kind = CH_COINS;
    snprintf(out[n++].text, sizeof out[0].text, "COINS\t%ld TO %ld", o->coins(orig), o->coins(e));
  }
  for (int p = 0; p < o->pockets(e); p++)
    for (int i = 0; i < o->pocket_slots(e, p) && n < cap; i++) {
      struct se_item now = {0}, was = {0};
      o->pocket_item(e, p, i, &now);
      o->pocket_item(orig, p, i, &was);
      if (now.id == was.id && now.count == was.count)
        continue;
      const char *nn = now.id ? o->item_name(e, now.id) : NULL, *wn = was.id ? o->item_name(e, was.id) : NULL;
      out[n].kind = CH_ITEM, out[n].a = p, out[n].b = i;
      if (!was.id)
        snprintf(out[n].text, sizeof out[0].text, "%s\tADDED x%d", nn ? nn : "?", now.count);
      else if (!now.id)
        snprintf(out[n].text, sizeof out[0].text, "%s\tTAKEN OUT", wn ? wn : "?");
      else if (now.id == was.id)
        snprintf(out[n].text, sizeof out[0].text, "%s\tx%d TO x%d", nn ? nn : "?", was.count, now.count);
      else
        snprintf(out[n].text, sizeof out[0].text, "%s\tNOW %s x%d", wn ? wn : "?", nn ? nn : "?", now.count);
      n++;
    }
  for (int b = SE_PARTY; b < o->boxes(e) && n < cap; b++) {
    int slots = b == SE_PARTY ? 6 : o->box_slots(e);
    for (int s = 0; s < slots && n < cap; s++) {
      struct se_mon x, y;
      bool hx = o->mon(e, b, s, &x), hy = o->mon(orig, b, s, &y);
      if (!hx && !hy)
        continue;
      char what[80], where[24];
      // Field by field: two reads of one Pokemon can differ in their padding.
      if (hx && hy && !editor_mon_diff(&x, &y, what, sizeof what))
        continue;
      if (b == SE_PARTY)
        snprintf(where, sizeof where, "PARTY %d", s + 1);
      else
        snprintf(where, sizeof where, "BOX %d, %d", b + 1, s + 1);
      if (!hx || !hy)
        snprintf(what, sizeof what, hx ? "MOVED IN" : "MOVED OUT");
      out[n].kind = CH_MON, out[n].a = b, out[n].b = s;
      if (snprintf(out[n].text, sizeof out[0].text, "%s %.20s\t%.60s", where, hx ? x.nick : y.nick, what) < 0)
        out[n].text[0] = 0;
      n++;
    }
  }
  return n;
}
/* The change C put back from ORIG: false (with why) when it can't be on its own. */
static bool editor_undo_one(struct save_edit *e, struct save_edit *orig, const struct editor_change *c, char *err,
                            size_t n) {
  const struct se_ops *o = e->ops;
  switch (c->kind) {
  case CH_MONEY:
    return o->set_money(e, o->money(orig), err, n);
  case CH_COINS:
    return o->set_coins(e, o->coins(orig), err, n);
  case CH_TIME: {
    int h, m, s;
    return se_play_time(orig, &h, &m, &s) && se_set_play_time(e, h, m, s);
  }
  case CH_BADGES:
    return se_set_badges(e, se_badges(orig));
  case CH_DEX:
    snprintf(err, n, "A filled Pokedex goes back only with UNDO ALL.");
    return false;
  case CH_ITEM: {
    struct se_item was = {0};
    o->pocket_item(orig, c->a, c->b, &was);
    return o->set_item(e, c->a, c->b, was.id, was.id ? was.count : 0, err, n);
  }
  default: {
    struct se_mon was;
    if (!o->mon(orig, c->a, c->b, &was) || !o->mon(e, c->a, c->b, &(struct se_mon){0})) {
      snprintf(err, n, "A Pokemon moved can only go back with UNDO ALL.");
      return false;
    }
    if (!o->set_mon(e, c->a, c->b, &was, err, n))
      return false;
    struct se_mon now;
    char what[80];
    if (o->mon(e, c->a, c->b, &now) && editor_mon_diff(&now, &was, what, sizeof what)) {
      snprintf(err, n, "Part of it (such as healing) only goes back with UNDO ALL.");
      return false;
    }
    return true;
  }
  }
}
/* Before SAVE: every change listed. True to write them. */
static bool editor_changes(struct save_edit *e, const char *save, const char *rom) {
  char err[200] = "";
  struct save_edit *orig = se_open(save, rom, err, sizeof err);
  if (!orig)
    return true; // the save can't be read again to compare: se_save checks it anyway
  enum { CAP = 120 };
  static struct editor_change list[CAP];
  static const char *rows[CAP + 1];
  int at = 0;
  bool go = false;
  while (running) {
    int n = editor_diff(e, orig, list, CAP);
    rows[0] = "SAVE THESE CHANGES";
    for (int i = 0; i < n; i++)
      rows[i + 1] = list[i].text;
    char hint[96];
    if (!n)
      snprintf(hint, sizeof hint, "Nothing differs from the save now.");
    else
      snprintf(hint, sizeof hint, "%d change%s%s. A on one puts it back as it was.", n, n == 1 ? "" : "s",
               n >= CAP ? " (the first ones)" : "");
    at = menu_pick_hint("CHANGES", hint, rows, n + 1, at);
    if (at <= 0) {
      go = at == 0;
      break;
    }
    if (!editor_undo_one(e, orig, &list[at - 1], err, sizeof err))
      editor_say("CAN'T PUT IT BACK", err);
    else
      notice("Put back as it was");
    if (at > n - 1)
      at = n - 1;
  }
  se_close(orig);
  return go;
}
/* TRAINER (GBA saves): play time, the badges one by one, and the Pokedex filled in. */
static void editor_trainer(struct save_edit *e) {
  int at = 0;
  while (running) {
    int h = 0, mi = 0, s = 0, badges = se_badges(e), caught = se_dex_count(e, true), seen = se_dex_count(e, false);
    se_play_time(e, &h, &mi, &s);
    static char time_[48], badge[48], dex[64];
    snprintf(time_, sizeof time_, "PLAY TIME\t%d:%02d", h, mi);
    int count = 0;
    for (int i = 0; i < 8; i++)
      count += badges >= 0 && (badges >> i & 1);
    snprintf(badge, sizeof badge, "BADGES\t%d OF 8", count);
    snprintf(dex, sizeof dex, "POKEDEX\t%d SEEN, %d CAUGHT", seen, caught);
    const char *rows[3];
    int ids[3], n = 0;
    rows[n] = time_, ids[n++] = 0;
    if (badges >= 0)
      rows[n] = badge, ids[n++] = 1;
    if (caught >= 0)
      rows[n] = dex, ids[n++] = 2;
    at = menu_pick_hint("TRAINER", badges < 0 ? "This game's badges and Pokedex aren't known, so only the play time "
                                                "changes." : "Nothing is written until SAVE CHANGES.",
                        rows, n, at);
    if (at < 0)
      return;
    long v;
    if (ids[at] == 0) {
      if (editor_number("HOURS", h, 0, 999, &v)) {
        long m2;
        if (editor_number("MINUTES", mi, 0, 59, &m2))
          se_set_play_time(e, (int)v, (int)m2, 0);
      }
    } else if (ids[at] == 1) {
      int sel = 0;
      while (running) {
        int now = se_badges(e);
        static char names[8][32];
        const char *b8[10];
        for (int i = 0; i < 8; i++) {
          snprintf(names[i], sizeof names[i], "BADGE %d\t%s", i + 1, now >> i & 1 ? "GOT IT" : "NOT YET");
          b8[i] = names[i];
        }
        b8[8] = "ALL EIGHT";
        b8[9] = "NONE";
        sel = menu_pick_hint("BADGES", "A switches a badge. A badge also lets its field moves work outside battle.",
                             b8, 10, sel);
        if (sel < 0)
          break;
        se_set_badges(e, sel == 8 ? 0xFF : sel == 9 ? 0 : now ^ (1 << sel));
      }
    } else {
      bool national = se_dex_national(e);
      const char *fill[2] = {"FILL THE REGIONAL POKEDEX", "FILL THE NATIONAL POKEDEX"};
      int k = menu_pick_hint("POKEDEX", national ? "Each one is marked seen and caught." :
                             "The national Pokedex isn't on in this game yet: only the regional one fills.",
                             fill, national ? 2 : 1, 0);
      if (k >= 0) {
        int added = se_dex_fill(e, k == 1);
        char line[96];
        snprintf(line, sizeof line, added > 0 ? "%d more caught: SAVE CHANGES writes it" : "That Pokedex is full already",
                 added);
        notice(line);
      }
    }
  }
}
static void save_editor(const char *rom, const char *gameName) {
  char save[1024], err[200] = "";
  editor_save_path(rom, save, sizeof save);
  struct save_edit *e = se_open(save, rom, err, sizeof err);
  if (!e) {
    editor_say("EDIT SAVE", err);
    return;
  }
  int selected = 0;
  while (running) {
    enum { E_MONEY, E_COINS, E_TRAINER, E_BAG, E_PARTY, E_BOXES, E_PRESETS, E_HEAL, E_UNDO, E_SAVE };
    char money[48], coins[48];
    const char *rows[11];
    int ids[11], n = 0;
    snprintf(money, sizeof money, "MONEY: %ld", e->ops->money(e));
    rows[n] = money, ids[n++] = E_MONEY;
    if (e->ops->coins(e) >= 0) {
      snprintf(coins, sizeof coins, "COINS: %ld", e->ops->coins(e));
      rows[n] = coins, ids[n++] = E_COINS;
    }
    int hours, minutes, seconds;
    if (se_play_time(e, &hours, &minutes, &seconds))
      rows[n] = "TRAINER\tPLAY TIME, BADGES, POKEDEX", ids[n++] = E_TRAINER;
    rows[n] = "BAG", ids[n++] = E_BAG;
    rows[n] = "PARTY", ids[n++] = E_PARTY;
    rows[n] = "PC BOXES", ids[n++] = E_BOXES;
    rows[n] = "PRESETS", ids[n++] = E_PRESETS;
    rows[n] = "HEAL THE PARTY", ids[n++] = E_HEAL;
    if (e->dirty)
      rows[n] = "UNDO ALL\tDROP THE CHANGES NOT SAVED", ids[n++] = E_UNDO;
    rows[n] = "SAVE CHANGES", ids[n++] = E_SAVE;
    char title[160];
    snprintf(title, sizeof title, "EDIT SAVE: %s", gameName);
    int pick = menu_pick_hint(title, e->dirty ? "Changed: SAVE CHANGES writes it (the old save is kept as a backup)." : NULL,
                              rows, n, selected);
    if (pick < 0) {
      if (!e->dirty)
        break;
      static const char *const leave[] = {"KEEP EDITING", "LEAVE WITHOUT SAVING"};
      if (menu_pick("THE CHANGES AREN'T SAVED", leave, 2, 0) == 1)
        break;
      continue;
    }
    selected = pick;
    long v;
    err[0] = 0;
    switch (ids[pick]) {
    case E_MONEY:
      if (editor_number("MONEY", e->ops->money(e), 0, e->ops->money_max(e), &v) &&
          !e->ops->set_money(e, v, err, sizeof err))
        editor_say("MONEY", err);
      break;
    case E_COINS:
      if (editor_number("COINS", e->ops->coins(e), 0, 9999, &v) && !e->ops->set_coins(e, v, err, sizeof err))
        editor_say("COINS", err);
      break;
    case E_TRAINER:
      editor_trainer(e);
      break;
    case E_BAG: {
      const char *pockets[SE_POCKETS_MAX + 1];
      int count = e->ops->pockets(e), rows = count, p = 0;
      for (int i = 0; i < count; i++)
        pockets[i] = e->ops->pocket_name(e, i);
      // ADD ANY ITEM: the game's whole list, each item put in its own pocket.
      if (e->ops->item_pocket)
        pockets[rows++] = "ADD ANY ITEM\tINTO ITS OWN POCKET";
      while (running && (p = menu_pick("BAG", pockets, rows, p)) >= 0) {
        if (p < count) {
          editor_pocket(e, p);
          continue;
        }
        int id = editor_pick_item(e, -1, "ADD WHICH ITEM?");
        int into = id > 0 ? e->ops->item_pocket(e, id) : -1;
        if (into >= 0)
          editor_add_item(e, into, id);
      }
      break;
    }
    case E_PARTY:
      editor_box(e, SE_PARTY, "PARTY");
      break;
    case E_BOXES: {
      static char names[32][16];
      const char *boxes[32];
      int count = e->ops->boxes(e), b = 0;
      for (int i = 0; i < count && i < 32; i++) {
        snprintf(names[i], sizeof names[i], "BOX %d", i + 1);
        boxes[i] = names[i];
      }
      while (running && (b = menu_pick("PC BOXES", boxes, count, b)) >= 0)
        editor_box(e, b, boxes[b]);
      break;
    }
    case E_PRESETS: {
      int at = 0;
      while (running) {
        struct preset_list *all = presets_all(false);
        int gen = se_generation(e), here = 0;
        for (int i = 0; i < all->count; i++)
          here += all->v[i].gen == gen;
        char info[96];
        snprintf(info, sizeof info, "SETS\t%d, %d FOR GEN %d", all->count, here, gen);
        const char *prow[] = {"GET SETS\tSMOGON, GENERATIONS 1 TO 9", "IMPORT SETS\t.TXT .CSV .JSON .ZIP .RAR", info};
        at = menu_pick_hint("PRESETS",
                            "A Pokemon's PRESETS row fills a set in; PASTE SET there takes a copied one. GET SETS "
                            "downloads Smogon's sets again.",
                            prow, 3, at);
        if (at < 0)
          break;
        if (at == 0) {
          presets_get(false);
          continue;
        }
        if (at != 1)
          continue;
        char path[1024], dir[1100], msg[160];
        if (!browse_path(path, sizeof path, ".txt|.csv|.json|.zip|.rar", "PICK A FILE OF SETS"))
          continue;
        presets_folder(dir, sizeof dir);
        presets_import(path, dir, msg, sizeof msg);
        presets_all(true);
        editor_say("IMPORT SETS", msg);
      }
      break;
    }
    case E_HEAL: {
      int healed = e->ops->heal(e);
      notice(healed ? "Healed: SAVE CHANGES writes it" : "The party is already healthy");
      break;
    }
    case E_UNDO: { // the save read again, as it is on disk
      static const char *const undo[] = {"KEEP THEM", "UNDO ALL"};
      if (menu_pick("DROP EVERY CHANGE NOT SAVED?", undo, 2, 0) != 1)
        break;
      struct save_edit *again = se_open(save, rom, err, sizeof err);
      if (!again) {
        editor_say("UNDO ALL", err);
        break;
      }
      se_close(e);
      e = again;
      selected = 0;
      notice("Undone: the save is as it was");
      break;
    }
    case E_SAVE: {
      if (!e->dirty) {
        notice("Nothing to save");
        break;
      }
      if (!editor_changes(e, save, rom))
        break;
      char backup[1100];
      if (se_save(e, backup, sizeof backup, err, sizeof err)) {
        int aside = editor_set_aside(rom);
        char done[1400];
        snprintf(done, sizeof done,
                 aside < 0 ? "Saved. The old save is kept as %s. Start the game from its save (not CONTINUE) to see "
                             "the changes."
                           : "Saved. The old save is kept as %s. The game starts from the edited save next time.",
                 backup);
        editor_say("SAVED", done);
      } else
        editor_say("NOT SAVED", err);
      break;
    }
    }
  }
  se_close(e);
}
/* ---- TRADE SAVES (save_edit.h's se_trade): two GBA games' saves, a PC box Pokemon each way, the games' own rules ---- */
static bool trade_is_gba(const char *path) {
  const char *ext = strrchr(path, '.');
  return ext && !strcasecmp(ext, ".gba");
}
/* A GBA game from the list (not NOT): its path and name, false for B. */
static bool trade_pick_game(const char *title, const char *not, char *path, size_t pathSize, char *name,
                            size_t nameSize) {
  struct rom_list l = {0};
  rom_list_load(&l, root, false);
  const char **rows = calloc(l.count ? l.count : 1, sizeof *rows);
  int *ids = calloc(l.count ? l.count : 1, sizeof *ids), n = 0;
  for (int i = 0; rows && ids && i < l.count; i++)
    if (trade_is_gba(l.paths[i]) && !(not && same_path(not, l.paths[i])))
      rows[n] = l.names[i], ids[n++] = i;
  bool ok = false;
  if (!n)
    editor_say(title, "TRADE SAVES needs a second GBA game in your list.");
  else {
    int pick = menu_pick(title, rows, n, 0);
    if (pick >= 0) {
      snprintf(path, pathSize, "%s", l.paths[ids[pick]]);
      snprintf(name, nameSize, "%s", l.names[ids[pick]]);
      ok = true;
    }
  }
  free(rows);
  free(ids);
  rom_list_free(&l);
  return ok;
}
/* A PC box slot of E's (a Pokemon or an empty one): false for B. */
static bool trade_pick_slot(struct save_edit *e, const char *who, int *box, int *slot) {
  static char names[32][16];
  const char *boxes[32];
  int count = e->ops->boxes(e), b = *box >= 0 ? *box : 0;
  if (count > 32)
    count = 32;
  for (int i = 0; i < count; i++) {
    snprintf(names[i], sizeof names[i], "BOX %d", i + 1);
    boxes[i] = names[i];
  }
  while (running && (b = menu_pick(who, boxes, count, b)) >= 0) {
    static char text[32][80];
    const char *rows[32];
    int slots = e->ops->box_slots(e);
    if (slots > 32)
      slots = 32;
    for (int s = 0; s < slots; s++) {
      struct se_mon m;
      int k = se_trade_slot(e, b, s);
      if (k == 0)
        snprintf(text[s], 80, "%d  (EMPTY)", s + 1);
      else if (k < 0 || !e->ops->mon(e, b, s, &m))
        snprintf(text[s], 80, "%d  BAD EGG", s + 1);
      else if (m.egg)
        snprintf(text[s], 80, "%d  EGG", s + 1);
      else
        snprintf(text[s], 80, "%d  %s  Lv%d", s + 1, m.nick, m.level);
      rows[s] = text[s];
    }
    int s = menu_pick(boxes[b], rows, slots, *box == b && *slot >= 0 ? *slot : 0);
    if (s >= 0) {
      *box = b, *slot = s;
      return true;
    }
  }
  return false;
}
static void trade_slot_text(struct save_edit *e, int box, int slot, char *out, size_t size) {
  struct se_mon m;
  if (slot < 0)
    snprintf(out, size, "PICK A SLOT");
  else if (se_trade_slot(e, box, slot) == 0)
    snprintf(out, size, "BOX %d #%d (EMPTY)", box + 1, slot + 1);
  else if (se_trade_slot(e, box, slot) < 0 || !e->ops->mon(e, box, slot, &m))
    snprintf(out, size, "BOX %d #%d BAD EGG", box + 1, slot + 1);
  else
    snprintf(out, size, "BOX %d #%d %.40s", box + 1, slot + 1, m.egg ? "EGG" : m.nick);
}
static struct save_edit *trade_open(const char *rom, char *save, size_t size) {
  char err[200] = "";
  editor_save_path(rom, save, size);
  struct save_edit *e = se_open(save, rom, err, sizeof err);
  if (!e)
    editor_say("TRADE SAVES", err);
  else if (!se_trade_ready(e)) {
    editor_say("TRADE SAVES", "TRADE SAVES works with FireRed, LeafGreen, Ruby, Sapphire, Emerald and the hacks that "
                              "keep their Pokemon the same way. This game's save isn't one of them.");
    se_close(e);
    e = NULL;
  }
  return e;
}
/* FIRST (NULL: picked here) and a second GBA game picked here. Nothing is written until SAVE BOTH. */
static void trade_saves(const char *firstRom, const char *firstName) {
  char romA[1024], nameA[256], romB[1024], nameB[256], saveA[1024], saveB[1024];
  if (firstRom) {
    snprintf(romA, sizeof romA, "%s", firstRom);
    snprintf(nameA, sizeof nameA, "%s", firstName);
  } else if (!trade_pick_game("TRADE FROM WHICH GAME?", NULL, romA, sizeof romA, nameA, sizeof nameA))
    return;
  if (!trade_pick_game("TRADE WITH WHICH GAME?", romA, romB, sizeof romB, nameB, sizeof nameB))
    return;
  struct save_edit *a = trade_open(romA, saveA, sizeof saveA);
  if (!a)
    return;
  struct save_edit *b = trade_open(romB, saveB, sizeof saveB);
  if (!b) {
    se_close(a);
    return;
  }
  if (same_path(saveA, saveB)) {
    editor_say("TRADE SAVES", "Both games use the same save file. Pick two games with saves of their own.");
    se_close(a);
    se_close(b);
    return;
  }
  int boxA = -1, slotA = -1, boxB = -1, slotB = -1, selected = 0;
  while (running) {
    enum { T_A, T_B, T_TRADE, T_UNDO, T_SAVE };
    static char text[3][200];
    char a1[80], b1[80];
    trade_slot_text(a, boxA, slotA, a1, sizeof a1);
    trade_slot_text(b, boxB, slotB, b1, sizeof b1);
    snprintf(text[0], sizeof text[0], "%.60s\t%s", nameA, a1);
    snprintf(text[1], sizeof text[1], "%.60s\t%s", nameB, b1);
    bool fullA = slotA >= 0 && se_trade_slot(a, boxA, slotA) != 0, fullB = slotB >= 0 && se_trade_slot(b, boxB, slotB) != 0;
    snprintf(text[2], sizeof text[2], "%s", fullA && fullB ? "TRADE" : "SEND");
    const char *rows[5] = {text[0], text[1], text[2], "UNDO ALL", "SAVE BOTH"};
    static char why[200];
    const char *hint = se_trade_link_blocker(a, b, why, sizeof why);
    if (!hint && slotA >= 0 && slotB >= 0) {
      if (!fullA && !fullB)
        hint = "Pick a Pokemon on one side or both.";
      else if (fullA && (hint = se_trade_blocker(a, boxA, slotA, b, why, sizeof why)))
        ;
      else if (fullB)
        hint = se_trade_blocker(b, boxB, slotB, a, why, sizeof why);
    }
    if (!hint)
      hint = a->dirty || b->dirty ? "Traded: SAVE BOTH writes both saves (each old save is kept as a backup)."
                                  : "Pick a PC box slot in each game, then trade. An empty slot sends one over.";
    char title[160];
    snprintf(title, sizeof title, "TRADE SAVES");
    int pick = menu_pick_hint(title, hint, rows, 5, selected);
    if (pick < 0) {
      if (!a->dirty && !b->dirty)
        break;
      static const char *const leave[] = {"KEEP TRADING", "LEAVE WITHOUT SAVING"};
      if (menu_pick("THE TRADES AREN'T SAVED", leave, 2, 0) == 1)
        break;
      continue;
    }
    selected = pick;
    char err[200] = "";
    switch (pick) {
    case T_A:
      trade_pick_slot(a, nameA, &boxA, &slotA);
      break;
    case T_B:
      trade_pick_slot(b, nameB, &boxB, &slotB);
      break;
    case T_TRADE: {
      if (slotA < 0 || slotB < 0) {
        editor_say("TRADE SAVES", "Pick a slot in each game first.");
        break;
      }
      bool evolve = false;
      int evoA = fullA ? se_trade_evolution(a, boxA, slotA, b) : 0, evoB = fullB ? se_trade_evolution(b, boxB, slotB, a) : 0;
      if (evoA || evoB) {
        char line[200];
        const char *to = evoA ? b->ops->species_name(b, evoA) : a->ops->species_name(a, evoB);
        snprintf(line, sizeof line, "It evolves into %.60s when it arrives, as a link trade would.", to ? to : "?");
        static const char *const ask[] = {"EVOLVE", "DON'T EVOLVE"};
        int k = menu_pick_hint("EVOLVING!", line, ask, 2, 0);
        if (k < 0)
          break;
        evolve = k == 0;
      }
      if (se_trade(a, boxA, slotA, b, boxB, slotB, evolve, err, sizeof err))
        notice(fullA && fullB ? "Traded: SAVE BOTH writes it" : "Sent: SAVE BOTH writes it");
      else
        editor_say("CAN'T TRADE", err);
      break;
    }
    case T_UNDO: {
      if (!a->dirty && !b->dirty) {
        notice("Nothing to undo");
        break;
      }
      char e2[200] = "";
      struct save_edit *na = se_open(saveA, romA, e2, sizeof e2), *nb = na ? se_open(saveB, romB, e2, sizeof e2) : NULL;
      if (!na || !nb) {
        if (na)
          se_close(na);
        editor_say("CAN'T UNDO", e2);
        break;
      }
      se_close(a);
      se_close(b);
      a = na, b = nb;
      notice("Back to the saves as they are");
      break;
    }
    case T_SAVE: {
      if (!a->dirty && !b->dirty) {
        notice("Nothing to save");
        break;
      }
      if (!se_save_both(a, b, err, sizeof err)) {
        editor_say("NOT SAVED", err);
        break;
      }
      int asideA = editor_set_aside(romA), asideB = editor_set_aside(romB);
      editor_say("SAVED", asideA < 0 || asideB < 0
                              ? "Saved both, old saves kept as backups. Start each from its save, not CONTINUE."
                              : "Saved both, old saves kept as backups. Each game starts from its new save.");
      break;
    }
    }
  }
  se_close(a);
  se_close(b);
}
/* CHOOSE BOX ART: every cover for a game as a grid, like SteamGridDB's own picker: the game's own pictures in its
 * art folder first (its box art, then its alternates), libretro's cover and the base game's, then each grid
 * SteamGridDB has for the game (box shaped ones only until SHAPE shows them all), a page at a time as you scroll. A
 * picks one: an alternate swaps names with the box art, anything else is saved in the folder as the box art (the old
 * one becomes an alternate). SEARCH AGAIN types another name and picks SteamGridDB's game from a list; FROM URL and
 * FROM FILE take a link or a file. Returns true when the game's art changed. */
enum { PICK_BAR_SEARCH, PICK_BAR_SHAPE, PICK_BAR_URL, PICK_BAR_FILE };
static bool boxart_picker(struct library *l, int g) {
  const char *stem = l->stems[g];
  struct boxart_game game = l->art[g];
  snprintf(game.title, sizeof game.title, "%s", l->games.names[g]);
  boxart_pick_open(&game);
  static struct game_grid grid;
  static struct game_grid_item items[400];
  static int itemOf[400];          // a tile's candidate
  static char stems[400][16], *stemPtr[400];
  static char sizes[400][24];
  static const char *const BAR_BOX[] = {"SEARCH AGAIN", "SHAPE: BOX", "FROM URL", "FROM FILE"};
  static const char *const BAR_ALL[] = {"SEARCH AGAIN", "SHAPE: ALL", "FROM URL", "FROM FILE"};
  static char labels[400][32];
  bool allShapes = false, changed = false, done = false;
  char title[300], hint[400], entry[128];
  memset(&grid, 0, sizeof grid);
  game_grid_init(&grid, items, 0);
  grid.bar = BAR_BOX;
  grid.barCount = 4;
  grid.title = title;
  grid.hint = hint;
  char **savedStems = gridStems;
  struct game_grid *savedOverlay = gridOverlay;
  int seenGen = -1, seenCount = -1;
  while (running && !done) {
    events();
    repeat_directions();
    menu_script();
    // The tiles: the candidates the shape lets through, rebuilt as more arrive.
    int count = boxart_pick_count();
    if (count != seenCount || boxart_generation() != seenGen) {
      seenCount = count;
      seenGen = boxart_generation();
      int keepCand = grid.count && grid.selected < grid.count ? itemOf[grid.selected] : -1;
      int n = 0;
      for (int i = 0; i < count && n < 400; i++) {
        struct boxart_pick_item it;
        if (!boxart_pick_item(i, &it) || it.failed)
          continue;
        bool box = it.source != BOXART_PICK_STEAMGRIDDB || !it.w || it.h * 10 >= it.w * 12; // taller than wide
        if (!allShapes && !box)
          continue;
        itemOf[n] = i;
        boxart_pick_stem(i, stems[n], sizeof stems[n]);
        stemPtr[n] = stems[n];
        if (it.w && it.h)
          snprintf(sizes[n], sizeof sizes[n], "%d x %d", it.w, it.h);
        else
          sizes[n][0] = 0;
        static const char *const SOURCE[] = {"STEAMGRIDDB", "LIBRETRO", "BASE GAME", "IN FOLDER"};
        static const uint32_t SOURCE_COLOR[] = {0x3A6EA5, 0x2E9E6A, 0xB7791F, 0x8E44AD};
        if (it.source == BOXART_PICK_LOCAL)
          snprintf(labels[n], sizeof labels[n], it.local ? "Alternate %d" : "Its box art now", it.local);
        else
          snprintf(labels[n], sizeof labels[n], "%s", it.source == BOXART_PICK_BASE      ? "The base game's box"
                                                      : it.source == BOXART_PICK_LIBRETRO ? "libretro's cover"
                                                                                          : "SteamGridDB");
        items[n] = (struct game_grid_item){labels[n], sizes[n], l->art[g].system, false, false, 0,
                                           SOURCE[it.source & 3], SOURCE_COLOR[it.source & 3]};
        n++;
      }
      grid.items = items;
      grid.count = n;
      if (keepCand >= 0)
        for (int k = 0; k < n; k++)
          if (itemOf[k] == keepCand)
            grid.selected = k;
      if (grid.selected >= n)
        grid.selected = n ? n - 1 : 0;
    }
    int state = boxart_pick_state(entry, sizeof entry);
    snprintf(title, sizeof title, "CHOOSE BOX ART: %s", l->games.names[g]);
    snprintf(hint, sizeof hint, "%s%s%s%s",
             boxart_name_shared(stem) ? "Another game has this name: its pictures go in its console's folder. " : "",
             state == BOXART_PICK_NO_KEY ? "Add a SteamGridDB key in SETTINGS > LIBRARY > STEAMGRIDDB KEY for more covers. "
             : state == BOXART_PICK_LOADING ? "Looking... "
                                            : "",
             *entry ? "SteamGridDB: " : "", entry);
    if (!*hint)
      snprintf(hint, sizeof hint, "%s", grid.count ? "A USE  X SHAPE  Y SEARCH AGAIN  B BACK" : "No covers found: SEARCH AGAIN tries another name");
    grid.bar = allShapes ? BAR_ALL : BAR_BOX;
    int use = -1, bar = -1;
    if (down(B) || down(MENU))
      break;
    if (down(LEFT))
      game_grid_move(&grid, -1, 0);
    if (down(RIGHT))
      game_grid_move(&grid, 1, 0);
    if (down(UP))
      game_grid_move(&grid, 0, -1);
    if (down(DOWN))
      game_grid_move(&grid, 0, 1);
    if (down(A)) {
      if (grid.focus == GAME_GRID_BAR)
        bar = grid.barSelected;
      else if (grid.count)
        use = grid.selected;
    }
    if (down(X))
      bar = PICK_BAR_SHAPE;
    if (down(Y))
      bar = PICK_BAR_SEARCH;
    if (wheelSteps) {
      game_grid_scroll(&grid, -wheelSteps * (grid.tileH + grid.gap) / 2);
      wheelSteps = 0;
    }
    for (int i = 0; i < tapCount; i++) {
      int b;
      int t = game_grid_touch(&grid, taps[i].x, taps[i].y, taps[i].down, &b);
      if (t >= 0)
        use = t;
      if (b >= 0)
        bar = b;
    }
    tapCount = 0;
    if (grid.count && grid.selected < grid.count)
      boxart_pick_focus(itemOf[grid.selected]);
    if (use >= 0 && use < grid.count) {
      notice("Saving the cover...");
      companion_canvas();
      game_grid_draw(&grid, &canvas);
      gridStems = stemPtr;
      gridOverlay = &grid;
      menu_frame();
      char err[120];
      if (boxart_pick_choose(stem, itemOf[use], err, sizeof err)) {
        notice("Box art set: the one it had is now an alternate");
        changed = done = true;
      } else
        notice(err);
    } else if (bar == PICK_BAR_SHAPE) {
      allShapes = !allShapes;
      seenCount = -1;
    } else if (bar == PICK_BAR_SEARCH) {
      char term[128];
      snprintf(term, sizeof term, "%s", l->games.names[g]);
      gridStems = savedStems;
      gridOverlay = NULL;
      if (text_entry("SEARCH STEAMGRIDDB", term, sizeof term) && *term) {
        boxart_pick_search(term);
        char names[20][128];
        int n = -1;
        while (running && (n = boxart_pick_names(names, 20)) < 0) { // a moment while it asks
          events();
          if (down(B) || down(MENU))
            break;
          companion_canvas();
          static const char *const wait[] = {"LOOKING..."};
          struct pd_menu m;
          pd_menu_init(&m, "SEARCH STEAMGRIDDB", wait, 1);
          pd_menu_draw(&m, &canvas);
          if (menu_frame())
            break;
        }
        if (n == 0)
          notice(boxart_pick_state(NULL, 0) == BOXART_PICK_NO_KEY
                     ? "Add a SteamGridDB key in SETTINGS > LIBRARY > STEAMGRIDDB KEY first"
                     : "SteamGridDB has no game by that name");
        else if (n > 0) {
          const char *rows[20];
          for (int i = 0; i < n; i++)
            rows[i] = names[i];
          int pick = menu_pick("WHICH GAME?", rows, n, 0);
          if (pick >= 0) {
            boxart_pick_use_entry(pick);
            grid.selected = 0;
            grid.scrollY = 0;
            grid.focus = GAME_GRID_TILES;
            seenCount = -1;
          }
        }
      }
    } else if (bar == PICK_BAR_URL) {
      static const char *const from[] = {"USE THE COPIED LINK", "TYPE A LINK"};
      gridStems = savedStems;
      gridOverlay = NULL;
      int f = menu_pick_hint("FROM URL",
                             "Copy an https link to a PNG or JPG picture, then pick USE THE COPIED LINK. It's saved in "
                             "the game's box art folder as its box art; the one it had becomes an alternate.",
                             from, 2, 0);
      char link[1024] = "https://";
      int gen = boxart_generation();
      if (f == 0) {
        char *clip = SDL_HasClipboardText() ? SDL_GetClipboardText() : NULL;
        grid_set_art(stem, clip ? clip : "");
        if (clip)
          SDL_free(clip);
      } else if (f == 1 && text_entry("FROM URL", link, sizeof link))
        grid_set_art(stem, link);
      changed = done = boxart_generation() != gen;
    } else if (bar == PICK_BAR_FILE) {
      char file[POCKET_FOLDER_MAX];
      gridStems = savedStems;
      gridOverlay = NULL;
      int gen = boxart_generation();
      if (browse_path(file, sizeof file, ".png|.jpg|.jpeg", "PICK A PNG OR JPG"))
        grid_set_art(stem, file);
      changed = done = boxart_generation() != gen;
    }
    if (done || !running)
      break;
    companion_canvas();
    game_grid_draw(&grid, &canvas);
    gridStems = stemPtr;
    gridOverlay = &grid;
    bool quit = menu_frame();
    gridOverlay = savedOverlay;
    gridStems = savedStems;
    if (quit)
      break;
  }
  gridOverlay = savedOverlay;
  gridStems = savedStems;
  boxart_pick_close();
  return changed;
}
/* ---- LINK PLAY (link_play.h, the app's pk_link.c): two to four GBA games joined by mGBA's link cable ---- */
static struct pocket_save_status linkSaveStatus[LINK_PLAY_MAX];
static struct VFile *link_open_save(int game, const char *path) {
  if (game < 0 || game >= LINK_PLAY_MAX)
    return NULL;
  return pocket_save_wrap(VFileOpen(path, O_CREAT | O_RDWR), &linkSaveStatus[game]);
}
static bool link_same_rom(const char *a, const char *b) { return same_path(a, b); }
/* The games run in step on this thread: the one with the focus gets the buttons and is heard, the others run dimmed.
 * L2 / R2, a stick click or X move the focus, a click picks a game, MENU asks to leave. maxFrames / shot: a headless
 * look (POCKET_LINK). */
static void link_run(int n, char roms[][1024], int maxFrames, const char *shot) {
  char saves[LINK_PLAY_MAX][1024];
  const char *romp[LINK_PLAY_MAX], *savep[LINK_PLAY_MAX];
  if (n < 2 || n > LINK_PLAY_MAX)
    return;
  for (int i = 0; i < n; i++)
    romp[i] = roms[i];
  for (int i = 0; i < n; i++) {
    char own[1024];
    editor_save_path(roms[i], own, sizeof own);
    link_copy_save(own, link_copy_index(romp, i, link_same_rom), saves[i], sizeof saves[i]);
    savep[i] = saves[i];
    if (!pocket_save_backup(saves[i], time(NULL))) {
      notice("Save backup failed; the link didn't start");
      return;
    }
  }
  pk_link_open_save = link_open_save;
  pk_link_after_reset = pk_heal_rtc;
  if (!pk_link_open(n, romp, savep, 32768)) {
    notice("A game or its save didn't open; the link didn't start");
    return;
  }
  for (int i = 0; i < n; i++)
    fprintf(stderr, "link play: game %d %s, save %s\n", i + 1, roms[i], saves[i]);
  int gw = (int)pk_link_video_w(0), gh = (int)pk_link_video_h(0);
  SDL_Texture *tex[LINK_PLAY_MAX] = {0};
  for (int i = 0; i < n; i++)
    tex[i] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_STREAMING,
                               (int)pk_link_video_w(i), (int)pk_link_video_h(i));
  struct mCore *c0 = pk_link_core(0);
  double hz = (double)c0->frequency(c0) / c0->frameCycles(c0);
  uint64_t freq = SDL_GetPerformanceFrequency(), due = SDL_GetPerformanceCounter();
  int focus = 0, frame = 0;
  bool ff = false, leave = false, faulted = false;
  struct link_rect at[LINK_PLAY_MAX];
  static int16_t sound[4096 * 2];
  if (audio)
    SDL_ClearQueuedAudio(audio);
  held = pressed = 0;
  while (running && !leave) {
    events();
    audio_check();
    if (host_action(HA_FF)) {
      ff = !ff;
      if (audio)
        SDL_ClearQueuedAudio(audio);
    }
    if (down(L2) || down(L3))
      focus = (focus + n - 1) % n;
    if (down(R2) || down(R3) || down(X))
      focus = (focus + 1) % n;
    if (wingClicked) {
      int hit = link_hit(at, n, (int)wingClickX, (int)wingClickY);
      if (hit >= 0)
        focus = hit;
    }
    if (down(MENU) || faulted) {
      static const char *const rows[] = {"KEEP PLAYING", "LEAVE\tSAVE IN EACH GAME FIRST"};
      for (int i = 0; i < n; i++)
        pk_link_set_keys(i, 0);
      if (faulted) {
        notice("The link stopped answering, so it closed: start it again");
        leave = true;
      } else if (menu_pick("LINK PLAY", rows, 2, 0) == 1)
        leave = true;
      held = pressed = 0;
      due = SDL_GetPerformanceCounter();
      if (audio)
        SDL_ClearQueuedAudio(audio);
      continue;
    }
    for (int i = 0; i < n; i++)
      pk_link_set_keys(i, i == focus ? held & 1023 : 0);
    // The heard game's sound, its rate nudged so the queue stays near 60 ms (the timer paces the games).
    if (audio && !ff) {
      double queued = SDL_GetQueuedAudioSize(audio) / 4.0, target = 32768 * 0.06, d = (target - queued) / target;
      d = d > 1 ? 1 : d < -1 ? -1 : d;
      pk_link_set_audio_rate(32768 * (1 + 0.005 * d));
    }
    int count = ff ? 4 : 1;
    for (int k = 0; k < count && !faulted; k++) {
      if (!pk_link_run_frame()) {
        char status[512];
        pk_link_status(status, sizeof status);
        fprintf(stderr, "link play: lockstep fault: %s\n", status);
        faulted = true;
      }
      frame++;
      for (int i = 0; i < n; i++) {
        int got = pk_link_read_audio(i, sound, 4096);
        if (i == focus && !ff && got > 0 && audio && SDL_GetQueuedAudioSize(audio) < 32768 * 4 / 4)
          SDL_QueueAudio(audio, sound, (Uint32)got * 4);
      }
    }
    int ow = 1, oh = 1;
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
    SDL_GetRendererOutputSize(renderer, &ow, &oh);
    link_layout(n, ow, oh, gw, gh, at);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 18, 18, 18, 255);
    SDL_RenderClear(renderer);
    for (int i = 0; i < n; i++) {
      SDL_Rect dst = {at[i].x, at[i].y, at[i].w, at[i].h};
      if (tex[i]) {
        SDL_UpdateTexture(tex[i], NULL, pk_link_video(i), (int)pk_link_video_w(i) * 4);
        SDL_RenderCopy(renderer, tex[i], NULL, &dst);
      }
      if (i == focus) {
        // The game with the buttons: an orange frame (the app's).
        SDL_SetRenderDrawColor(renderer, 255, 140, 26, 255);
        for (int b = 1; b <= 6; b++) {
          SDL_Rect r = {dst.x - b, dst.y - b, dst.w + 2 * b, dst.h + 2 * b};
          SDL_RenderDrawRect(renderer, &r);
        }
      } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 110);
        SDL_RenderFillRect(renderer, &dst);
      }
    }
    if (ff) {
      // FF: four frames a refresh, every game, silent; a bar under the focused game says so.
      SDL_SetRenderDrawColor(renderer, 255, 140, 26, 255);
      SDL_Rect bar = {at[focus].x, at[focus].y + at[focus].h + 8, at[focus].w / 6, 4};
      SDL_RenderFillRect(renderer, &bar);
    }
    if (maxFrames && frame >= maxFrames) {
      if (shot && !capture_frame(shot, NULL))
        fprintf(stderr, "link play: capture failed\n");
      running = 0;
      break;
    }
    host_present();
    if (maxFrames)
      continue; // headless: as fast as it goes
    due += (uint64_t)(freq / hz);
    uint64_t now = SDL_GetPerformanceCounter();
    if (now < due)
      SDL_Delay((Uint32)((due - now) * 1000 / freq));
    else if (now - due > freq / 5)
      due = now;
  }
  // A game in the middle of saving finishes first (up to ten seconds of frames), then each closes and syncs.
  pk_link_close(600);
  for (int i = 0; i < n; i++) {
    if (tex[i])
      SDL_DestroyTexture(tex[i]);
    if (linkSaveStatus[i].failed) {
      char line[100];
      snprintf(line, sizeof line, "GAME %d's save couldn't be written; its backup is in DATA/backups", i + 1);
      notice(line);
      fprintf(stderr, "link play: %s\n", line);
    }
  }
  if (audio)
    SDL_ClearQueuedAudio(audio);
  held = pressed = 0;
}
/* Any GBA game from the list (the same one again too): its path and name, false for B. */
static bool link_pick_game(const char *title, char *path, size_t pathSize, char *name, size_t nameSize) {
  struct rom_list l = {0};
  rom_list_load(&l, root, false);
  const char **rows = calloc(l.count ? l.count : 1, sizeof *rows);
  int *ids = calloc(l.count ? l.count : 1, sizeof *ids), n = 0;
  for (int i = 0; rows && ids && i < l.count; i++)
    if (trade_is_gba(l.paths[i]))
      rows[n] = l.names[i], ids[n++] = i;
  bool ok = false;
  if (!n)
    editor_say(title, "LINK PLAY needs a GBA game in your list.");
  else {
    int pick = menu_pick(title, rows, n, 0);
    if (pick >= 0) {
      snprintf(path, pathSize, "%s", l.paths[ids[pick]]);
      snprintf(name, nameSize, "%s", l.names[ids[pick]]);
      ok = true;
    }
  }
  free(rows);
  free(ids);
  rom_list_free(&l);
  return ok;
}
/* LINK PLAY's setup, as the app's: GAME 1 (FIRST, when given), ADD A GAME up to four, a game's row swaps or removes it,
 * START LINK once there are two. */
static void link_setup(const char *firstRom, const char *firstName) {
  static char roms[LINK_PLAY_MAX][1024], names[LINK_PLAY_MAX][256];
  int n = 0, sel = 0;
  if (firstRom) {
    snprintf(roms[0], sizeof roms[0], "%s", firstRom);
    snprintf(names[0], sizeof names[0], "%s", firstName ? firstName : firstRom);
    n = 1;
    sel = 1;
  }
  enum { LINK_ADD = 10, LINK_START };
  while (running) {
    char text[LINK_PLAY_MAX][300];
    const char *rows[LINK_PLAY_MAX + 2];
    int ids[LINK_PLAY_MAX + 2], m = 0;
    for (int i = 0; i < n; i++) {
      snprintf(text[i], sizeof text[i], "GAME %d\t%.250s", i + 1, names[i]);
      rows[m] = text[i];
      ids[m++] = i;
    }
    if (n < LINK_PLAY_MAX)
      rows[m] = "ADD A GAME", ids[m++] = LINK_ADD;
    if (n >= 2)
      rows[m] = "START LINK", ids[m++] = LINK_START;
    int pick = menu_pick_hint("LINK PLAY", "THE OTHER GAMES WAIT FOR GAME 1", rows, m, sel);
    if (pick < 0)
      return;
    sel = pick;
    int id = ids[pick];
    if (id == LINK_ADD) {
      char title[32];
      snprintf(title, sizeof title, "GAME %d", n + 1);
      if (link_pick_game(title, roms[n], sizeof roms[n], names[n], sizeof names[n])) {
        n++;
        sel = n >= 2 ? n + (n < LINK_PLAY_MAX) : n; // on START LINK once there is one
      }
    } else if (id == LINK_START) {
      link_run(n, roms, 0, NULL);
      return;
    } else {
      static const char *const what[] = {"SWAP\tPICK ANOTHER GAME", "REMOVE"};
      int k = menu_pick(rows[pick], what, 2, 0);
      char path[1024], name[256];
      if (k == 0 && link_pick_game(rows[pick], path, sizeof path, name, sizeof name)) {
        snprintf(roms[id], sizeof roms[id], "%s", path);
        snprintf(names[id], sizeof names[id], "%s", name);
      } else if (k == 1) {
        for (int i = id; i < n - 1; i++) {
          memcpy(roms[i], roms[i + 1], sizeof roms[i]);
          memcpy(names[i], names[i + 1], sizeof names[i]);
        }
        n--;
        sel = 0;
      }
    }
  }
}
/* A game's options. Returns the game to play, else -1; *reload when the list changed; *finding when a search started. */
static int library_options(struct library *l, int g, bool *reload, bool *finding) {
  enum { OPT_PLAY, OPT_ART, OPT_RENAME, OPT_RESET, OPT_EDIT, OPT_TRADE, OPT_LINK, OPT_SUPPORT };
  (void)finding; // CHOOSE BOX ART no longer starts a search of its own
  const char *label = l->games.labels[g];
  char custom[256];
  bool hasName = game_custom_name(root, label, custom, sizeof custom);
  const char *rows[10];
  int ids[10], n = 0;
  char supportDetail[96], supportRow[140];
  int support = game_support_get(g, supportDetail, sizeof supportDetail);
#define OPT(id, text)                                                                                                  \
  do {                                                                                                                 \
    rows[n] = text;                                                                                                    \
    ids[n++] = id;                                                                                                     \
  } while (0)
  OPT(OPT_PLAY, "PLAY");
  OPT(OPT_ART, "CHOOSE BOX ART");
  OPT(OPT_RENAME, "RENAME");
  if (hasName)
    OPT(OPT_RESET, "USE THE FILE'S NAME");
  OPT(OPT_EDIT, "EDIT SAVE");
  if (trade_is_gba(l->games.paths[g])) {
    OPT(OPT_TRADE, "TRADE SAVES");
    OPT(OPT_LINK, "LINK PLAY");
  }
  if (game_support_word(support)) { // the companion's support, and what it lacks (A shows it whole)
    snprintf(supportRow, sizeof supportRow, "COMPANION\t%s", game_support_word(support));
    OPT(OPT_SUPPORT, supportRow);
  }
#undef OPT
  int pick = menu_pick(l->games.names[g], rows, n, 0);
  int id = pick >= 0 ? ids[pick] : -1;
  switch (id) {
  case OPT_PLAY:
    return g;
  case OPT_SUPPORT: {
    char line[128];
    snprintf(line, sizeof line, "%s: %s", game_support_word(support), supportDetail);
    notice(line);
    break;
  }
  case OPT_ART:
    boxart_picker(l, g);
    break;
  case OPT_RENAME: {
    char name[256];
    snprintf(name, sizeof name, "%s", l->games.names[g]);
    if (text_entry("RENAME", name, sizeof name)) {
      if (!*name)
        notice("A name needs a letter or two");
      else if (game_custom_name_set(root, label, name))
        *reload = true;
      else
        notice("Couldn't save the name");
    }
    break;
  }
  case OPT_RESET:
    if (game_custom_name_set(root, label, NULL))
      *reload = true;
    break;
  case OPT_EDIT:
    save_editor(l->games.paths[g], l->games.names[g]);
    break;
  case OPT_TRADE:
    trade_saves(l->games.paths[g], l->games.names[g]);
    break;
  case OPT_LINK:
    link_setup(l->games.paths[g], l->games.names[g]);
    break;
  default:
    break;
  }
  return -1;
}
static bool library_frame(struct library *l) {
  gridStems = l->stems;
  gridOverlay = &l->grid;
  bool quit = menu_frame();
  gridOverlay = NULL;
  return quit;
}
static void library_screen(void) {
  static struct library lib; // static: its grid is big (the art's places)
  memset(&lib, 0, sizeof lib);
  boxart_init(root);
  library_load(&lib);
  // POCKET_SETUP=1 shows GET STARTED whatever DATA holds (a headless look at it).
  if ((!lib.count && !setup_done()) || (getenv("POCKET_SETUP") && !strcmp(getenv("POCKET_SETUP"), "1"))) {
    if (get_started())
      library_load(&lib);
  }
  // POCKET_DROP=<file>: as if the file were dropped on the window (a headless look at PATCH A ROM, a box art).
  if (getenv("POCKET_DROP") && !droppedFile)
    droppedFile = strdup(getenv("POCKET_DROP"));
  struct game_grid *grid = &lib.grid;
  uint32_t touchAt = 0;
  bool wasTouching = false, finding = false, syncing = false;
  static char syncHint[96];
  while (running) {
    events();
    repeat_directions();
    menu_script();
    companion_canvas();
    game_grid_draw(grid, &canvas);
    if (update_prompt()) {
      gridModal = true; // the dialog over a dimmed grid, not the box art over the dialog
      bool quit = library_frame(&lib);
      gridModal = false;
      if (quit)
        break;
      continue;
    }
    if (down(B) || down(MENU))
      break;
    int play = -1, bar = -1, options = -1;
    if (down(LEFT))
      game_grid_move(grid, -1, 0);
    if (down(RIGHT))
      game_grid_move(grid, 1, 0);
    if (down(UP))
      game_grid_move(grid, 0, -1);
    if (down(DOWN))
      game_grid_move(grid, 0, 1);
    if (down(A)) {
      if (grid->focus == GAME_GRID_BAR)
        bar = grid->barSelected;
      else if (lib.count)
        play = grid->selected;
    }
    if (down(X) && lib.count && grid->focus == GAME_GRID_TILES)
      options = grid->selected;
    if (down(Y))
      bar = GRID_BAR_SETTINGS;
    if (wheelSteps) {
      game_grid_scroll(grid, -wheelSteps * (grid->tileH + grid->gap) / 2);
      wheelSteps = 0;
    }
    for (int i = 0; i < tapCount; i++) {
      int b;
      int t = game_grid_touch(grid, taps[i].x, taps[i].y, taps[i].down, &b);
      if (t >= 0)
        play = t;
      if (b >= 0)
        bar = b;
    }
    tapCount = 0;
    // A long press: the game's options.
    if (grid->touching && !wasTouching)
      touchAt = SDL_GetTicks();
    wasTouching = grid->touching;
    if (grid->touching && !grid->dragged && grid->pressedTile >= 0 && SDL_GetTicks() - touchAt > 550) {
      options = grid->pressedTile;
      grid->selected = options;
      grid->touching = wasTouching = false;
      tapHeld = false;
    }
    if (rightClicked) {
      rightClicked = false;
      int t = game_grid_tile_at(grid, rightClickX, rightClickY);
      if (t >= 0) {
        grid->selected = t;
        grid->focus = GAME_GRID_TILES;
        options = t;
      }
    }
    if (droppedFile) {
      size_t dl = strlen(droppedFile);
      char made[POCKET_FOLDER_MAX + 320];
      if (dl > 4 && !strcasecmp(droppedFile + dl - 4, ".bps")) {
        // A .bps dropped on the window: PATCH A ROM, and the new game picked in the list.
        if (patch_rom(droppedFile, made, sizeof made)) {
          library_load(&lib);
          for (int i = 0; i < lib.games.count; i++)
            if (same_path(lib.games.paths[i], made))
              grid->selected = i;
        }
      }
      // An image dropped on the window: the selected game's box art.
      else if (lib.count && grid->selected >= 0 && grid->selected < lib.count)
        grid_set_art(lib.stems[grid->selected], droppedFile);
      free(droppedFile);
      droppedFile = NULL;
    }
    static int supportSeen = -1;
    if (supportSeen != game_support_generation()) {
      supportSeen = game_support_generation();
      for (int i = 0; i < lib.count; i++)
        lib.items[i].support = game_support_get(i, NULL, 0);
    }
    static int artSeen = -1;
    if (artSeen != boxart_generation()) { // new art: a hack may now show its base game's box, with a banner
      artSeen = boxart_generation();
      for (int i = 0; i < lib.count; i++)
        lib.items[i].banner = boxart_is_base(lib.stems[i]);
    }
    if (syncing) {
      // SYNC BOX ART's progress along the bottom; the banners (a hack's base box) come with the reload at the end.
      int done = 0, total = 0;
      if (boxart_progress(&done, &total)) {
        snprintf(syncHint, sizeof syncHint, "BOX ART: %d OF %d GAMES", done, total);
        grid->hint = syncHint;
      } else {
        syncing = false;
        library_load(&lib);
        notice("Box art up to date");
      }
    }
    if (finding) {
      int r = boxart_find_result();
      if (r >= 0) {
        finding = false;
        char key[80];
        boxart_key(key, sizeof key);
        notice(r == 1 ? "Box art found"
               : *key ? "No box art found: BOX ART in its options picks one"
                      : "No box art found: a SteamGridDB key in SETTINGS finds more");
      }
    }
    if (bar == GRID_BAR_SYNC && lib.count) {
      static const char *const rows[] = {"MISSING ONLY\tGAMES WITH NO BOX ART YET",
                                         "ALL AGAIN\tEVERY GAME, YOUR OWN ART STAYS"};
      int pick = menu_pick("SYNC BOX ART", rows, 2, 0);
      if (pick >= 0) {
        boxart_sync(lib.art, lib.count, pick == 1);
        syncing = true;
        notice("Looking for box art...");
      }
    } else if (bar == GRID_BAR_THEME) {
      // LIGHT / DARK: DARK MODE itself (the companion's SETTINGS has the same switch), saved at once.
      char ini[1100];
      snprintf(ini, sizeof ini, "%s/settings.ini", root);
      struct pd_settings st;
      pd_settings_load(&st, ini);
      st.darkMode = !st.darkMode;
      pd_settings_save(&st, ini);
      pd_look_apply(NULL, &st);
      LIBRARY_BAR[GRID_BAR_THEME] = st.darkMode ? GAME_GRID_ICON_MOON : GAME_GRID_ICON_SUN;
      notice(st.darkMode ? "Dark mode" : "Light mode");
    } else if (bar == GRID_BAR_FOLDERS) {
      if (folders_screen())
        library_load(&lib);
    } else if (bar == GRID_BAR_SETTINGS) {
      bool changed = false;
      int next = settings_page(&changed);
      if (changed)
        library_load(&lib);
      if (next < 0)
        break;
    } else if (options >= 0 && options < lib.count) {
      bool reload = false;
      int p = library_options(&lib, options, &reload, &finding);
      if (reload)
        library_load(&lib);
      if (p >= 0 && p < lib.count)
        play = p;
    }
    if (play >= 0 && play < lib.count) {
      grid->selected = play;
      char rom[1400];
      snprintf(rom, sizeof rom, "%s", lib.games.paths[play]);
      run_game(rom, 0, NULL);
    }
    if (!running)
      break;
    companion_canvas();
    game_grid_draw(grid, &canvas);
    if (library_frame(&lib))
      break;
  }
  gridStems = NULL;
  boxart_quit();
  boxart_textures_free();
  game_support_stop();
  library_free(&lib);
}

int main(int argc, char **argv) {
  pd_settings_expand_default = true; // ASPECT EXPAND unless the player picked another (pd_settings.h)
  pd_settings_dark_default = true;   // DARK MODE on unless the player turned it off
  const char *rom = NULL, *shot = NULL;
  int frames = 0;
#ifdef POCKET_DS
  se_gen4_opener = se_open_gen4; // SAVE EDITOR's HeartGold / SoulSilver part lives with the DS backend
#endif
#ifdef _WIN32
  // SDL_MAIN_HANDLED: this main is the program's own (no SDL2main).
  SDL_SetMainReady();
  // The portable zip keeps its data beside the program: DATA next to PokeDaisy.exe,
  // wherever it was started from (a frontend's working folder is its own).
  static char exeData[1024];
  char *base = SDL_GetBasePath();
  if (base) {
    snprintf(exeData, sizeof exeData, "%sDATA", base);
    SDL_free(base);
    root = exeData;
  }
#endif
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--version")) {
      printf("PokeDaisy %s\n", POCKET_VERSION);
      return 0;
    }
    if (!strcmp(argv[i], "--data") && i + 1 < argc)
      root = argv[++i];
    else if (!strcmp(argv[i], "--save") && i + 1 < argc)
      saveFile = argv[++i];
    else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
      frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--shot") && i + 1 < argc)
      shot = argv[++i];
    else
      rom = argv[i];
  }
  mLogSetDefaultLogger(&quietLogger);
  signal(SIGINT, stop);
  signal(SIGTERM, stop);
  pocket_mkdir(root);
#ifndef POCKET_TEST
  {
    char log[1100];
    snprintf(log, sizeof log, "%s/pokedaisy.log", root);
    pocket_attach_log(log);
  }
#endif
  fprintf(stderr, "PokeDaisy %s\n", POCKET_VERSION);
#ifndef POCKET_TEST
  updater_own_name(); // pokedaisy.exe from an older version becomes PokeDaisy.exe
#endif
  char path[1024];
  static char backups[1024];
  if (saveFile) {
    snprintf(backups, sizeof backups, "%s/backups", root);
    pocket_backup_dir = backups;
  }
  const char *dirs[] = {"roms", "saves", "states"};
  for (int i = 0; i < 3; i++) {
    snprintf(path, sizeof path, "%s/%s", root, dirs[i]);
    pocket_mkdir(path);
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER)) {
    fprintf(stderr, "SDL: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  // A click or tap that brings the window forward also presses what it lands on (a HUD button, a wing).
  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
  // Every pad's face buttons by position (SDL would otherwise swap a Nintendo pad's by its labels): BUTTON LAYOUT
  // says which one is A.
  SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
  {
    /* A first start (no settings.ini yet) takes the XBOX layout: A is the bottom button, B the right one, as on most
     * pads. A settings.ini already there keeps the BUTTON LAYOUT it has, so nobody's choice changes. */
    snprintf(path, sizeof path, "%s/settings.ini", root);
    FILE *had = fopen(path, "r");
    if (had)
      fclose(had);
    else {
      struct pd_settings st;
      pd_settings_load(&st, path);
      st.buttonLayout = PD_LAYOUT_XBOX;
      pd_settings_save(&st, path);
    }
  }
  controls_load();
  SDL_DisplayMode display = {0};
  bool fullscreen = getenv("POCKET_FULLSCREEN") != NULL;
  if (!fullscreen && !frames) {
    // FULLSCREEN as last chosen; the first start picks it for a handheld's screen and keeps that.
    snprintf(path, sizeof path, "%s/settings.ini", root);
    struct pd_settings st;
    pd_settings_load(&st, path);
    if (st.fullscreen < 0) {
      st.fullscreen = handheld_screen() ? 1 : 0;
      pd_settings_save(&st, path);
    }
    fullscreen = st.fullscreen == 1;
  }
  if(fullscreen && SDL_GetCurrentDisplayMode(0,&display))fullscreen=false;
  // POCKET_WINDOW=WxH: a window the size of a device's panel, for trying a
  // layout on a desktop (1280x720 Smart Pro, 640x480, 720x720 ...).
  int winW = W, winH = H;
  const char *size = getenv("POCKET_WINDOW");
  if (size && (sscanf(size, "%dx%d", &winW, &winH) != 2 || winW < 160 ||
               winH < 144 || winW > 8192 || winH > 8192))
    winW = W, winH = H;
  window = SDL_CreateWindow(POCKET_HOST_NAME, SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, fullscreen?display.w:winW, fullscreen?display.h:winH,
      SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
  fullscreenOn = fullscreen && window;
  // Presents wait for the refresh (the pacing, above); a driver without it gets the timer.
  renderer = window ? make_renderer(window, getenv("POCKET_NO_VSYNC") ? 0 : SDL_RENDERER_PRESENTVSYNC) : NULL;
  if (!renderer && window)
    renderer = make_renderer(window, 0);
  if (!renderer) {
    fprintf(stderr, "Video: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  if (orientation != PD_ORIENT_LANDSCAPE)
    rotation_ready(); // ORIENTATION read before there was a renderer
  SDL_RendererInfo rendererInfo;
  if (!SDL_GetRendererInfo(renderer, &rendererInfo))
    fprintf(stderr, "Video backend: %s; opaque RGB888 canvas %dx%d\n",
            rendererInfo.name, canvasW, canvasH);
  SDL_ShowCursor(SDL_DISABLE);
  companion_canvas();
  gameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888,
                                  SDL_TEXTUREACCESS_STREAMING, 256, 256);
  wideTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888,
                                  SDL_TEXTUREACCESS_STREAMING, PK_WIDE_MAX_VIEW_W,
                                  PK_WIDE_MAX_VIEW_H);
  if (!texture || !gameTexture || !wideTexture) {
    fprintf(stderr, "Texture: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetTextureBlendMode(gameTexture, SDL_BLENDMODE_NONE);
  SDL_SetTextureBlendMode(wideTexture, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  audio_open();
  rawTrimui=getenv("POCKET_RAW_TRIMUI") && !strcmp(getenv("POCKET_RAW_TRIMUI"),"1");
  snprintf(path, sizeof path, "%s/gamecontrollerdb.txt", root);
  if (pocket_readable(path)) {
    int added = SDL_GameControllerAddMappingsFromFile(path);
    fprintf(stderr, "Controller mappings from %s: %d\n", path, added);
  }
  for (int i = 0; i < SDL_NumJoysticks(); i++)
    if (SDL_IsGameController(i))
      controller_added(i);
  if (!padCount)
    fprintf(stderr, "No controller yet (one switched on later is picked up)\n");
  SDL_Joystick *js = !controller && rawTrimui && SDL_NumJoysticks() ? SDL_JoystickOpen(0) : NULL;
  {
    // Once per launch, in the background; never in a headless run (--frames) or a local build ("dev").
    // POCKET_UPDATE_AS=x.y.z checks as that version (to try the prompt).
    snprintf(path, sizeof path, "%s/settings.ini", root);
    struct pd_settings st;
    pd_settings_load(&st, path);
    const char *as = getenv("POCKET_UPDATE_AS");
    updater_start(root, as ? as : POCKET_VERSION, st.updateCheck && (!frames || as) && !getenv("POCKET_NO_UPDATE_CHECK"));
    // RetroAchievements: a saved sign-in signs in again (never in a headless run).
    if (!frames)
      ra_init(root, POCKET_VERSION, notice);
  }
  if (getenv("POCKET_LINK")) {
    // POCKET_LINK="romA|romB[|...]": LINK PLAY straight away (a headless look; --frames / --shot as for a game).
    static char roms[LINK_PLAY_MAX][1024];
    int n = 0;
    for (const char *s = getenv("POCKET_LINK"); *s && n < LINK_PLAY_MAX;) {
      size_t len = strcspn(s, "|");
      snprintf(roms[n++], sizeof roms[0], "%.*s", (int)len, s);
      s += len;
      if (*s)
        s++;
    }
    link_run(n, roms, frames, shot);
  } else if (rom)
    run_game(rom, frames, shot);
  else {
    // The landing page: the games in DATA/roms and the GAME FOLDERS (rom_list.c) as a grid of box art.
    menuFrames = frames;
    menuShot = shot;
    menuKeys = getenv("POCKET_MENU_KEYS");
    library_screen();
  }
  if (js)
    SDL_JoystickClose(js);
  updater_finish();
  controllers_close();
  if (audio)
    SDL_CloseAudioDevice(audio);
  // NULLed: the runtime test runs the app again in a new window.
  SDL_DestroyTexture(texture);
  texture = NULL;
  SDL_DestroyTexture(gameTexture);
  SDL_DestroyTexture(wideTexture);
  SDL_DestroyTexture(lcdTexture);
  lcdTexture = NULL;
  if (filterTexture)
    SDL_DestroyTexture(filterTexture);
  filterTexture = NULL;
  if (menuTexture)
    SDL_DestroyTexture(menuTexture);
  menuTexture = NULL;
  if (hudTexture)
    SDL_DestroyTexture(hudTexture);
  hudTexture = NULL;
  pocket_shader_free();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return appError;
}
