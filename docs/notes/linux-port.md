# Linux port

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 211-256). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## Linux port (`linux/`)

An SDL2 host for Linux handhelds (Keegan Whitacre's, from his `keeganwhitacre/pokedaisy-brick` fork, whose `pocket/`
has much more: Polished Crystal, the summary pages, NextUI power handling): the pinned mGBA core plus the 3DS port's
C companion (`3ds/core/`), so a companion change for 3DS shows here too and the other way round. Its canvas follows
the screen (`companion_size`: the largest whole scale keeping 320x240, so 426x240 at 3x on 1280x720); PARTY is
Android's `PartySlot` ported (`pd_ui_party.c`), and the host's D-pad focus reaches the core as `pd_ui.focusId` so a
slot shows its own selected look instead of the focus ring. Controllers hotplug (`controller_added` / `_removed`, mappings
from `DATA/gamecontrollerdb.txt` or `SDL_GAMECONTROLLERCONFIG`); `package.py --portmaster` + `linux/portmaster/PokeDaisy.sh`
make a PortMaster port (ROCKNIX & co., untried on a device; `test_portmaster.py` runs it against a stand-in PortMaster). Y opens
the QUICK MENU (`linux/quick_menu.c`: QuickMenu.kt's RING / GLASS look, drawn into an ARGB canvas over the running game):
WILD ENCOUNTERS (the game's own switch, found by `linux/wild.c` - a C port of `WildEncounters.find`'s agbcc / CFRU
shapes - and held by pk_wide.c's `pk_field_state`; `settings.wildOff`), SAVE / LOAD STATE, FF, COMPANION, and PC /
FLY / REPEL / BIKE (`linux/fly.c`, `linux/field_items.c`: Fly.kt, FieldItems.kt's useFieldItem, Pc.kt and KeyItems.kt
in C over pd_config's `fly*` / `field*` / `pc*` fields from the `FIELD_ITEMS_*` records - the target's `records = {
items = ... }` - and the game's own functions through `pk_call`, moved out of pokedaisy_jni.c into
`app/src/main/cpp/pk_call.c` for both; the party / bag from the snapshot, "on the field" from pk_wide's scene, which the
host reads in ORIGINAL too; BIKE presses SELECT for the game). `linux-runtime-test` uses each on FireRed, LeafGreen
and Emerald and checks the result (a Repel / FLY taught to the save through the game's AddBagItem / SetMonData where it
lacks one). The HUD (`linux/hud.c`, SETTINGS > HUD, `settings.hud` / `hudHidden`) is FieldHud's CORNERS / GLASS with only
the parts a player reads (PARTY, MINIMAP, POKé BALLS, REPEL STEPS: the buttons are the quick menu's without touch),
redrawn only when `hud_key` changes. TAB BAR (`settings.tabBar`, at most 5, the rest under a MORE chip; CARD also
from SETTINGS) and the scrolling SETTINGS list are the core's, so the 3DS has them too. It adds no game data of its own (addresses come from `3ds/core/`). Build and test in a Linux container or
VM: `cmake -S linux -B linux/build && cmake --build linux/build && ctest --test-dir linux/build`; `linux-runtime-test
ROM DATA OUT` boots a real ROM + save (DATA/saves/<rom file name>.sav) to the field and screenshots PARTY / the
summary. Saves are backed up into `saves/pokedaisy-backups/` like Android's (`pocket_save_backup`, newest 10
distinct); states carry no save data, so loading one never writes the save. Keep a copy of the ROM's bytes rather
than mGBA's ROM block pointer: a GPIO write (FireRed) swaps the mapping. CI: `.github/workflows/linux.yml` (x86_64
native, AArch64 under QEMU with the checksum-pinned TrimUI toolchain).
**DS on Linux** (opt-in, `-DPOCKET_DSPERATE=ON`; Keegan's, 2026-10-28; `linux/dsperate/README.md`): `.nds` games on the
DSperate core (`third_party/dsperate`, unmodified), its own event loop (`frontend.cpp`), a read-only Platinum USA rev 1
companion (`companion.cpp`, Gen4Reader.kt's rules), the shared quick menu and party HUD. It builds the app's
`pk_nds_field.cpp` as is - keep that file RAM-only (melonDS reads go in `pk_nds_field_gpu.cpp`). `data_gen.h` is scraped
from the Kotlin (Gen4Reader / Gen4TablesPlatinum / Gen4TownMapPlatinum / NdsArt / Gen4Dex ...) by `gen_data.py`: after
changing those, run it (`--check` fails in `.github/workflows/linux-ds-experiment.yml`). DSperate states carry the save;
loading one keeps the battery save in memory (`load_state_keeping_save`), like the GBA host.
**DS on melonDS** (`-DPOCKET_MELONDS=ON`, Windows and Linux; 2026-11-11): the same frontend over `ds_core.h`'s `DsCore`
(`linux/dsperate/core_dsperate.cpp` or `linux/melonds/core_melonds.cpp`), melonDS built as the app builds it with the app's
`pk_nds_platform.cpp` (software renderer, FreeBIOS, direct boot). Its battery save sits beside the ROM as `<stem>.sav`
(melonDS's raw format, so synced folders keep working), backups in DATA/backups, states `<file>.melonds.ss`. Mouse and SDL
fingers press the touch screen while it shows (`ds_touch_point`; tests push taps with `POCKET_DS_HOST_TAPS`). CI: Windows'
`x86_64` job (the one Windows zip plays GBA and DS), Linux DS's `melonds` matrix entry; packages `--melonds` (melonDS's notices; Linux's zip `-ds`).
Its DS LAYOUT (`ds_layout.h`, pure, `test_layout.cpp`; `layout=N` in ds-settings.ini, append only): ONE SCREEN / PICTURE IN PICTURE /
TOUCH IN WING / STACKED / SIDE BY SIDE / LARGE + SMALL, SELECT + R2 / L2 step it (R2 alone still swaps, L2 the quick menu); ONE SCREEN floats
the touch screen on `PK_NDS_FIELD_TOUCH` and is PIP for a game without field rules.
**DS EXPAND on the desktop (2026-11-12)**: melonDS's `DsCore` runs the app's `pk_nds_wide.cpp` (built through
`linux/melonds/pk_nds_wide_host.cpp`: on Windows melonDS's headers pull in `<windows.h>`, whose `near` / `far` macros and
wingdi's `Polygon()` break that file, so the wrapper reads the headers first, drops the macros, and the file gets `NOGDI`).
The frontend sets the margins (`wideState`, the same overworld / battle rules as `pk_nds.cpp`), the window (`wideWindow`)
and draws `wideView` through `wide_camera.h`, the GBA's camera; `pd_ds_expand` / `pd_ds_auto_zoom` come from the host's
ASPECT and AUTO ZOOM IN. A crash that only shows outside gdb on Windows is often the NT debug heap hiding it: run gdb with
`_NO_DEBUG_HEAP=1`.

**Where a click landed.** Read a mouse event's own `x` / `y` through `mouse_event_at()` (it undoes the renderer's
logical size), never `SDL_GetMouseState()`: that is where the mouse is when the event is read, after any motion queued
behind the click, and a wing button 22 pixels tall missed it and opened the companion. Per-frame flags that a hit test
reads (`rightWingMap`) are set every frame the wing is shown, not only when its texture is drawn again.

**A move's kind and accuracy** (`pd_move_kind` / `pd_move_accuracy`, the battle wing's PHY / SPE / STA and ACC): a
FireRed or Emerald kind ROM's own 12-byte move table is found by its first three moves, and byte 10 is read as the
split only when POUND, EMBER, GROWL and SWORDS DANCE read 0, 1, 2, 2 there (CFRU hacks). Otherwise Gen 3's rule by type
applies, and only when the type numbering is vanilla. DS games show no kind: their move data isn't read yet.

## RetroAchievements (linux/ra.c)

- rc_client as on Android (softcore). Its server calls run on SDL threads through `http_post` (WinHTTP, or curl reading the body from a 0600 temp file so a password never shows in `ps`); the answers wait in a queue that `ra_pump` delivers from `events()`, so they always land on the main thread, between frames.
- The GB / GBA hash is the MD5 of the ROM *file* (`romdata` in run_game), never the core's ROM: mGBA writes the GPIO registers into it.
- The token lives in DATA/retroachievements.ini; a token the server turns down deletes it. Headless runs (`--frames`) never sign in.
- Progress is written beside each state as `<state>.ra` inside `state()`. DS games don't load a set yet (the DS loop is dsperate's).

**LINK PLAY** (2026-11-13, a GBA game's options and SETTINGS > LIBRARY; `main.c`'s link_setup / link_run, `link_play.c`): the app's `pk_link.c` compiled into the host as it is, with one hook added there, `pk_link_open_save`, so each game's save goes through `pocket_save_wrap` (NULL back = that game isn't opened) after `pocket_save_backup`. `pk_link_after_reset` = `pk_heal_rtc`. The host loop is timer-paced at game 0's rate; only the focused game's audio is queued (its rate nudged toward a 60 ms queue), the others are drained and dropped. `POCKET_LINK="romA|romB"` with `--frames` / `--shot` opens it headless. A second copy of a ROM plays `<save stem> (link N).<ext>` as LinkActivity names it.

**EDIT SAVE's move legality and PRESETS** (2026-11-13, `save_edit.c`, `presets.c`): the retail move lists come from the app's `SaveTablesGen.kt` through `scripts/gen_save_tables_c.py` into `linux/save_tables_gen.h` (the `save-tables-gen` ctest fails when they drift). A ROM counts as retail only when it has no names of its own, isn't randomized and every species' ROM level-up list matches the table; only then are new moves refused. Egg moves come from the ROM's own gEggMoves, found by Bulbasaur's run (20001) followed by Charmander's (20004). PRESETS read Showdown text (both the `EVs: 252 HP / 4 Def` and the `EVs: hp: 252; def: 4` spellings, `=== [Gen N TIER] Species [Set] ===` headers), CSV and two JSON shapes (a flat list, and Smogon's `{species: {tier: {set: {...}}}}`, its generation from the file name). Sets dedupe on generation, species, tier without its `Gen N`, set name and moves. Archives unpack through `%SystemRoot%\System32\tar.exe` on Windows (libarchive reads .zip and .rar), else bsdtar, unrar / 7z, unzip or `python3 -m zipfile`. `POCKET_PASTE` stands in for the clipboard in headless looks. The user's Smogon files are never committed.

**TOUCH WINDOW (DS, `DsPopout` in `dsperate/frontend.cpp`)**: the touch screen in a second SDL window with its own renderer (no vsync, so the game's present still paces the loop). Gotchas: with two windows open SDL sends no `SDL_QUIT` when the game's window closes, so the loop pushes one itself; a click in the second window makes the game's window lose focus, so FOCUS_LOST only pauses when, a frame later, neither window has the keyboard; events from the second window are taken before `pd_ds_host_event` so the host never sees its clicks. `POCKET_DS_TAPS_TOUCH_WINDOW=1` sends `POCKET_DS_HOST_TAPS` to it, and `--shot x.png` also writes `x-touch.png`. Don't save prefs after the loop: `POCKET_DS_RESUME=0` would be written back.

**TOUCH IN WING's pop out** (`popTarget` / `popAnim` in `dsperate/frontend.cpp`): it rewrites `place.touch` to the big box and sets `touchFloats` *after* EXPAND's room is worked out, so the game's picture doesn't change under it; `touchFloats` also empties the wing boxes, so no tap opens the companion. `pointerAt` returns whether it took a press, and callers only try the wing when it didn't. `ds_winged_top` steps down to 3x (8x and 7x leave 2560 x 1600 no wings), but WINGS FIRST's wider wings only take one scale off. `POCKET_DS_TAPS_FINGER=1` sends `POCKET_DS_HOST_TAPS` as finger events.

**EDIT SAVE's help bar and letter jump** (2026-11-14, `main.c`, `pd_menu`'s `keys`): `editorDepth` is above 0 while `save_editor` runs, and every pick-list then draws `pd_menu.keys` along the bottom. `menu_pick_keys` screens set their own bar through `pickBar`, used once. LB/RB jump by first letter only in lists a caller marks with `pickLetters` (species, moves, items), since a Pokemon's own page also has 20+ rows. The shoulders are named by `shoulders_name()` from BUTTON LAYOUT. `POCKET_MENU_KEYS` takes `[` and `]` for the shoulders.

**ART STYLE** (2026-11-14, `boxart.c`, `game_grid.c`, settings.ini `artstyle`, 0 TALL / 1 BOX): the art drawn for a style is `art_styled_path`: the box art when its shape suits the style or `<stem>.mainstyle` in DATA/boxart says it was picked under it, else the highest-numbered alternate of that shape (the newest pushed aside), else the box art. Only the player's picks (the picker, FROM URL / FILE, an alternate swap) write `.mainstyle`; a find clears it. TALL with a key: `tried()` is false until the game has tall art or a `.none-tall` mark, and `look()` asks SteamGridDB for 600x900 / 342x482 / 660x930 first, saving it as the box art (the old box becomes an alternate). The grid trims art within about 1/7 of the tile's shape to fill it, and blurs anything else. WIDE (920x430) was tried and dropped: hardly any of these games have it, so the tiles were mostly blur.

- **Library art keys and covers**: box art is filed under the automatic display name (`rom_list.autoNames`), never a RENAME, so renames keep their art. A PC fan game's `Graphics/Titles` picture (`boxart_game.pcArt`) is never saved as art; `boxart_games` deletes a main art that is byte-identical to it unless a `.mainstyle` mark says the player picked it. A tile with no art draws a title card in `game_grid.c`.

- **mkxp-z and the controller**: mkxp-z opens only SDL joystick 0 (`SDL_GameControllerOpen(0)` at start, hot plug only
  for `which == 0`), so on a handheld whose first listed device isn't its pad the game never sees the pad, and its
  own SDL's mapping can disagree with PokeDaisy's. PokeDaisy therefore hides every controller from an mkxp-z player
  (`SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0xFFFF/0xFFFF`, set only around `CreateProcess`) and presses mkxp-z's
  default keys (`pc_pad_mkxpz`: ENTER, ESC, SHIFT, A, S, D, Q, W, the arrows) through the PC_PAD_KEYS path. The
  player's F1 settings window is forced off in the shadow mkxp.json. Keys only reach a game whose window is in front,
  so a check on a desktop someone is using needs keys posted to the game's window instead.
- **PAD FEED** (2026-11-16): keys failed on a handheld (Indigo, "doesn't work with controllers") wherever the game's
  window didn't get the foreground (Windows refuses `SetForegroundWindow` from behind; a desktop in use showed it:
  stuck on Indigo's CONTROLS screen) or the player's own `keybindings.mkxp1` (in `%APPDATA%/<Game.ini Title>`, next to
  the saves, so it can't be redirected) no longer ties ENTER / ESC to C / B. Now `pc_start` sets `POKEDAISY_PC_PAD`
  (`<pc dir>/pad.txt`), the companion wraps `Input.update` (at `pad_setup`, after the game's scripts redefined it) and
  answers `press?` / `trigger?` / `repeat?` / `release?` / `dir4` / `dir8` from the mask too, and says `pad_feed`;
  from then main.c writes `m<mask>;` (`pc_pad_mask`, bits of `enum pc_pad_button`) on each change and presses no key.
  Since 2026-11-17 every fan game with the companion gets the feed (keys mode too: the win32 Input's `press?` and
  friends are wrapped the same way) and no key is pressed before it loads; only games without a companion still get
  keys. The feed no longer checks the foreground (`pc_input_ours` is gone): a window taking it on the user's desktop
  cut the pad off mid game. The stuck-at-connect filter uses only SDL's state as the pad opens: a 700 ms window after
  it also ate real presses (`test_controller_hotplug`, which failed the release). The user also saw "everything is auto run": run is Input::A, pressed as SHIFT, which
  Sticky Keys (five SHIFT presses) or a key-up sent while another window was in front leaves held. Checked with a ViGEm X360 pad (a test helper of its own, ViGEmBus's IOCTLs) and an
  observer preload logging `Input.press?` per frame. Note that `SDL_GetPrefPath` ignores an `APPDATA` override, so a
  check run makes `%APPDATA%/<title>` even with one (delete it if it stays empty).
- **STUCK AT CONNECT** (2026-11-16, main.c `pad_unstuck`): "everything is automated in terms of selection" on a
  handheld. Buttons and axis sides (beyond 16000) already on when a pad opens, or turning on in its first 700 ms, are
  dropped until they let go (an axis until it's back inside 8000). The guess is a second HID device beside the pad
  whose report leaves something on for good; not reproduced here. Logged as "Controller X: button N on as it connected".
- Indigo was hidden (a DROPPED list by Game.ini Title) on 2026-11-16 at the user's ask and brought back on 2026-11-17
  once no key was pressed for the controller in any game.
- **Feed rewrap** (2026-11-17): Awakening defines its own keyboard Input (PSystem_Controls) after its language choice,
  replacing the wrappers made at frame 10, so the pad died there. `feed_check` (every tick) wraps any of Input's methods
  that isn't ours again and rereads the button numbers.
  `test_companion_feed.rb` (ctest companion-feed when ruby is found) covers it.
- **Feed edges per update** (2026-11-17): reading the pad once per `Graphics.frame_count` left `trigger?` true for
  every `Input.update` until the count moved, and Indigo (its own 40 fps cap) runs more updates than that, so one
  D-pad press moved its menus two or three rows (seen in the user's video, 0.2 s, far under the repeat's 0.4 s).
  `feed_read` now reads at each outermost `Input.update` (`feed_depth` keeps chained wrappers to one read), as RGSS.
- **Pad file kept open** (2026-11-17): "tons of lag after each button press" in Indigo. `pc_pad_feed` used to
  fopen / write / fclose `pad.txt` at each change and the companion opened it at each read; Defender scans a file
  after a write is closed and holds the next open meanwhile, so the game stalled after every press. Now PokeDaisy
  keeps one handle for the play (8 bytes, `m%05u;\n`, written in place and flushed; `pc_pad_feed(NULL, 0)` closes
  it after the play) and the companion keeps one too (`@padIO`, seek 0 and read).
- **Wings paced** (2026-11-17): "massive lag" in every companion game (Indigo, Reborn, Rejuvenation, Reminiscencia)
  even with the pad file kept open. Since #223 / #224 every mkxp-z game is PC_PAD_KEYS, so `pc_play`'s loop ran every
  8 ms, and each pass software-drew both wings and the bars, uploaded them and presented the whole full-screen
  window with vsync, behind the game's window: the handheld's iGPU and DWM's composition were shared with the game at
  the screen's refresh rate (compare the vsync stall above). Now a game PokeDaisy doesn't draw (not `capture`) gets a
  repaint only when `wingsDirty` (the companion's JSON, a window change, zoom / ff / instant / WIDE, a button, a
  restart) or `PC_WINGS_MS` (66) has passed; input and the pad feed still run every 8 ms. Capture games still draw
  every pass (PokeDaisy draws their picture).
- **Name entry on the pad** (2026-11-18): a OneXPlayer player (no keyboard) was stuck at Reborn / Rejuvenation's
  "Your name?" screen. Rejuvenation 13.5's `TextEntry.rb` (Reborn's is the same code) has no `PokemonEntryScene2`:
  `pbEnterText` always builds `PokemonEntryScene`, whose `USEKEYBOARD = true` constant picks `Window_TextEntry_Keyboard`
  (`Input.gets`, ENTER / ESC through `triggerex?`) over its old `Window_CharacterEntry` grid (`pbEntry2`: arrows, C
  picks, B deletes via `Window_TextEntry#update`, an OK cell confirms). Essentials v17 to v21 instead choose
  `PokemonEntryScene2` (the cursor grid) or the keyboard scene from `$PokemonSystem.textinput` (older:
  `USEKEYBOARDTEXTENTRY`). Every one of them goes through `PokemonEntry#pbStartScreen`, so the companion wraps that
  (`hook_entry`, retried on ticks until the class exists): once the pad feed has seen any button (`@padUsed`, set in
  `feed_read`), a `PokemonEntryScene` there is swapped for a fresh `PokemonEntryScene2` when the game has one, else
  the scene's own `USEKEYBOARD` is `remove_const` / `const_set` to false for that call, and both are put back in an
  `ensure` (a reset mid-entry included). Nothing is written to `$PokemonSystem`. Not covered: `pbFreeText` /
  `pbMessageFreeText` (a typed box inside a message, used for codes and passwords), which has no grid to fall back
  on. Test: `test_companion_entry.rb` (ctest `companion-entry`).
- **XInput, not raw input** (2026-11-18): the real cause of the fan game lag and the 3 to 6 rows a press on the
  OneXPlayer. SDL 2.32 on Windows reads an XInput pad through its RAWINPUT (and WGI) drivers, whose reports reach
  PokeDaisy reliably only while its own window has the foreground; behind the game's window the pad feed logged a
  100 ms tap as `400` held 365 to 510 ms, so repeat fired and the press came late. The game itself ran at 57 to 60
  fps (companion stats), and the keyboard and the game's own `Game.exe` were smooth. `SDL_JOYSTICK_RAWINPUT=0` and
  `SDL_JOYSTICK_WGI=0` in the environment made it smooth, so main.c now sets both hints before `SDL_Init` on
  Windows (the environment still overrides them). The pad's log line then shows no `HID#...IG_` path. Earlier
  guesses (#230 edges, the pad file, #233 wings, #234's input paths) didn't touch this.
- **RESTART** (2026-11-17): Indigo's VSync option asks "Restart now?" and launches `Game.exe` itself, which failed
  under PokeDaisy ("Windows cannot find 'Game.exe'") and would have run outside it anyway. The companion wraps
  `system` / `spawn` / `exec`, `Process.spawn` and mkxp-z's `System.launch`: a command naming `game(.exe)` or the
  player's exe (`POKEDAISY_PC_EXE`) isn't run, `POKEDAISY_PC_RESTART` (`<pc dir>/restart.txt`) is written instead,
  and `pc_play` starts the game again once it closes (`pc_restart_asked`, a note under a minute old, ten a play). The
  VSync choice itself changes nothing: the shadow mkxp.json keeps vsync off.
- **Pad log** (2026-11-17): pokedaisy.log now always names each controller (vendor:product, SDL path, what it held as
  it connected) and the first 600 pad feed changes of a play with their time, so a report of a game moving by itself
  shows whether PokeDaisy fed a held button (a device's stuck report) or the game read input of its own.
- **Input redesign for four games** (2026-11-17): Indigo / Añil, Reminiscencia, Reborn and Rejuvenation still
  lagged and skipped rows after #230 / #233 while ~15 other fan games played smoothly (all four are 40 fps,
  Essentials v16/v17-era games). Only these four change (`pc_lean_game`: a whole word of Game.ini's Title or the
  folder name, Añil in UTF-8 / Latin-1 / "anil"); every other game keeps the feed as it was.
  - Measured first, in `linux/bench_companion_input.rb` (ctest `companion-bench`; `--full` for more timing runs): a
    mock RGSS loop with mkxp-z's Input (native) and old Essentials' keyboard Input (keys: update asks `getstate` for
    256 keys, answers through `buttonToKey` in Ruby), 1 to 3 `Input.update` a frame, up to 2000 extra `press?` calls
    a frame. Findings: a wrapped call costs about 0.2 to 0.3 us, so 2000 calls a frame add about 0.5 ms (hypothesis 1,
    per-call overhead, can't make "major latency" unless a game makes tens of thousands of calls a frame). But the
    feed's `repeat?` counts updates: at 3 updates a frame a 150 ms tap moved 2 rows and a 1 s hold 27 rows (7 at one
    update a frame). That is the skip whenever a game runs `Input.update` more than once a frame (hypothesis 2,
    confirmed in the model).
  - **INPUT PATH** (`pc_input_path`, `POKEDAISY_PC_INPUT` for the companion): `feed` (every other game, unchanged),
    `source` (the four), `native`. `source` on a keys-mode game (`Input.getstate` and `buttonToKey` exist) wraps only
    `update` (reads the pad file first, outermost call only) and `getstate` (true for the VK the game's own
    `buttonToKey` ties to each fed button, through globals `$pokedaisy_vk`, so no call into the companion per key):
    the game's own update, trigger, repeat and dir4 run on it as on a keyboard (the bench: identical rows to the
    keyboard at 1 to 3 updates a frame). If 40 updates in a row never ask `getstate` (a game whose update reads
    Windows directly) it falls back to the lean answers and logs it. `source` on mkxp-z's own Input (no getstate):
    the wrappers stay (there's no Ruby way into mkxp-z's C key state) but each is one bit test of masks worked out
    once per outermost update (`lean_edges`), and `repeat?` goes by the monotonic clock (0.4 s, then every 0.1 s; a
    stall never fires a burst): 1 row a tap and 7 a second at any update rate.
  - `native` (only via `POCKET_PC_INPUT=native`, direction A, for bisecting on the device): the mkxp-z player is
    shown the first pad PokeDaisy has, alone (`SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0xVVVV/0xPPPP`, so it is
    joystick 0), with the face-swap mappings for NINTENDO and `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1`; the
    companion feeds nothing (a keys-mode game, which can't read a pad, still gets `source`). Not the default: the
    reasons for hiding the pad (a second HID device with the same IDs, the player's own F1 bindings in
    keybindings.mkxp1) still stand, and it's untested on the OXP3. `POCKET_PC_INPUT=feed` gives the four the old
    feed back.
  - LEAN extras (`POKEDAISY_PC_LEAN=1`, the four only): ZOOM / WIDE VIEW go back to the game's size at once but
    return only after a second of `quiet_map?` (a busy map's events flipped it and rebuilt the sprites each time);
    `pad_roles` (every script inflated, up to 3000 files read) runs again only when the button numbers change;
    `map_name` once a map; `icon_of` asks the game once per species / form / egg / shiny / gender.
  - DIAGNOSTICS, every fan game with the companion (`POKEDAISY_PC_LOG` is now always `<pc dir>/companion.log`,
    cleared by `pc_prepare`, copied into pokedaisy.log as "PC companion: ..." by `pc_companion_log` after the play):
    "stats:" every 30 s (every 5 minutes after the first 10 lines) with the frame median / p95 / max and frames over
    50 ms (from `Graphics.update` to `Graphics.update`), `Input.update` calls a frame (and the most in one frame),
    wrapped calls a frame, the input mode and path, and screen resizes; "slow: <step> N ms" for any companion step
    over 20 ms (300 at most). main.c logs "PC game: input path ..." at start and "the companion says input keys /
    native, pad feed on" when it changes. Ruby 1.8 players have no monotonic clock: `now` checks once (`MONO`)
    instead of raising at each call.
  - What's left unproven: whether the four really run more than one update a frame, and where the latency comes
    from. The first play's stats lines answer both: Input.update per frame over 1 means the skip above; a median
    frame well over 25 ms or many frames over 50 ms means the game itself is slow (then "slow:" lines say whether
    it's the companion).
- **Phoenix Rising's left stick** (2026-11-17): the D-pad worked, the stick did nothing. The game runs on its own
  RGSS player with no companion, and PokeDaisy feeds it nothing: its own scripts read the pad through XInput
  (presumably `XInputGetState` through Win32API, inferred: the game isn't in the repo) and, from the report, look
  at the D-pad bits of `wButtons` only, never `sThumbLX` / `sThumbLY`. Two ways were
  weighed. (b) Feeding the stick in as the D-pad at the XInput level needs a proxy `xinput1_x.dll` that the game
  loads, which means a file beside its exe in the user's own game folder (the RGSS player runs from there, unlike
  mkxp-z's copy in `<pc dir>`; the ground rules forbid writing there) or injecting a DLL into the game (fragile,
  antivirus bait, and a 32-bit build to ship); a virtual pad (ViGEmBus) needs a driver the user would have to
  install. Ruled out. (a) The stick alone presses the arrow keys through `pc_keys_send`, which every RGSS game
  reads (the player's own Input and Essentials' `GetAsyncKeyState` alike); the D-pad and buttons are left to the
  game as before, so nothing is read twice. Taken, with the stuck-key history in mind: arrows aren't SHIFT (no
  Sticky Keys), and a pressed arrow can't outlive what holds it. `pc_keys_send` presses only while the game's
  process owns the foreground and lets every key go the moment it doesn't (from then on, and its key-up goes
  through `SendInput`, which clears `GetAsyncKeyState`'s state wherever the focus is); the stick centring, GAME
  MENU and the end of the play (`pc_keys_send(NULL)`) let go too. `pc_stick_dir` gives one direction at a time
  (dominant axis), on past 16000 and off inside 10000 (hysteresis at the rim), and keeps a held direction until the
  other axis is 1.25x past it (hysteresis at the diagonal), so a stick near 45 degrees doesn't flicker between
  two keys; `test_pc_game` covers it. Only `info.player == PC_PLAYER_RGSS && !info.companion` games get it, and
  `held`'s UP / DOWN / LEFT / RIGHT aren't used (they merge the D-pad with the stick); the axes are read from SDL
  directly, a side still on from STUCK AT CONNECT read as at rest. The log says "left stick up / centred" (first
  600 changes); `POCKET_PC_STICK="x,y"` holds the stick for checks. A game reading the stick itself as well would
  just see the same direction twice, which a held key and a held stick combine into. Not checked on the real game
  here (it isn't in the repo).
- **32-bit players** (Bushido's mkxp-z) get LARGE_ADDRESS_AWARE set on PokeDaisy's copy (`pc_large_address`): without
  it a big window on a handheld's driver left too little of 2 GB and `load_data` of the battle animations failed with
  NoMemoryError.
- **mkxp-z vsync**: the shadow mkxp.json forces `vsync` and `syncToRefreshrate` false. With them on (Indigo, Eon
  Guardians) the player's window over PokeDaisy's stalled at about 7 fps (264 to 577 ms frames) and a 60 ms key press
  was mostly missed; off, Indigo holds its own 40 fps and Eon Guardians 60. In keys mode (`PC_PAD_KEYS`) the loop
  runs every 8 ms and each press stays down at least 60 ms (`downUntil` in main.c), so a tap between two of
  PokeDaisy's polls or two of the game's frames still lands.
