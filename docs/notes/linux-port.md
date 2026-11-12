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
