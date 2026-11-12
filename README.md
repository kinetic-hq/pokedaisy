# PokeDaisy for Windows and Linux

> [!IMPORTANT]
> **This is a fork of [PokeDaisy](https://github.com/lidor30/pokedaisy), made by Lidor Itzhari ([lidor30](https://github.com/lidor30)).**
> PokeDaisy, its companion and its design are Lidor's work. This fork only adds the Windows and Linux desktop /
> handheld version. For the Android app use [the original project](https://github.com/lidor30/pokedaisy), and if you
> enjoy PokeDaisy, support Lidor on [Patreon](https://www.patreon.com/cw/pokedaisy),
> [Buy me a coffee](https://buymeacoffee.com/lidor30g) or [Ko-fi](https://ko-fi.com/pokedaisy). PokeDaisy stays under
> the GNU GPL v3 with Lidor's copyright and credit notices kept as they are ([LICENSE](LICENSE), [NOTICE](NOTICE)).

One portable program for Windows handhelds (ROG Ally, Legion Go, OneXPlayer, ...), PCs and Linux that plays the Game
Boy, Game Boy Color, GBA and Nintendo DS Pokemon games you own, with PokeDaisy's live companion beside the game on one
screen: your party, bag, map, battle matchups, Pokedex and a spoiler-free guide.

<p>
  <img src="docs/images/desktop/game-list.png" alt="The game list: a grid of games with their system and whether each is an original or a ROM hack" width="49%">
  <img src="docs/images/desktop/companion-view.png" alt="COMPANION VIEW: six ways to place the companion around the game, each drawn as a small picture" width="49%">
  <img src="docs/images/desktop/ds-layout.png" alt="DS LAYOUT: six ways to place a DS game's two screens" width="49%">
  <img src="docs/images/desktop/settings.png" alt="SETTINGS, grouped like the Android app's" width="49%">
</p>

## Install and update

1. Download `pokedaisy-windows-x86_64.zip` (or `pokedaisy-linux-x86_64.zip`) from the top `desktop-v` release on the
   [Releases](https://github.com/kinetic-hq/pokedaisy/releases) page. `SHA256SUMS.txt` has the checksums.
2. Unzip it somewhere you can write to (Documents, not `Program Files`). Keep `pokedaisy.exe` and `SDL2.dll` together.
3. Put your `.gb`, `.gbc`, `.gba` and `.nds` files in `DATA\roms` (or add your own folders with `GAME FOLDERS`) and
   open `pokedaisy.exe`. The first start walks you through a few choices.

Updates come by themselves: each start looks for a newer release and offers `UPDATE NOW`, `LATER` or `SKIP THIS
VERSION`, and an update only replaces the program, never your `DATA` folder (settings, saves, states). Desktop builds
are a beta, so every release is marked pre-release. Turn the check off with `SETTINGS` > `CHECK FOR UPDATES`.

The full guide is in [docs/manual/windows.md](docs/manual/windows.md) (and [linux.md](docs/manual/linux.md)), and
[android-parity.md](docs/manual/android-parity.md) compares it with the Android app. When
something goes wrong, `DATA\pokedaisy.log` is what to attach to a report.

## OLED handhelds (OneXPlayer 3, Legion Go 2)

An OLED screen doesn't light black pixels at all, so PokeDaisy can keep everything around the game dark:

- `SETTINGS` > `SCREEN` > `DISPLAY OPTIONS` > `COLORS` > `OLED`: a little less glare, and the panels round the game go
  true black. With `OLED` (and in every DS game) `BACKGROUND` starts on `BLACK`.
- The companion's wings become near black cards with a faint edge; `DIM COMPANION` darkens them further.
- Against burn in, wings that haven't changed for a minute dim, and they move by a pixel every few minutes.
- `BRIGHTNESS` (80% to 150%) brightens the game itself if the darker look is too dim in daylight.

## How this fork differs from PokeDaisy

| Area | What the fork adds |
| --- | --- |
| Platforms | A Windows build of the desktop host (one zip for GBA, Game Boy and DS), Linux zips, self-updates and automatic `desktop-vX.Y.Z` pre-releases |
| Nintendo DS | melonDS, `DS LAYOUT` (one screen, picture in picture, stacked, side by side, large + small, wings, touch pieces), touch by mouse, finger or controller, rewind, time of day and Berry Pots |
| Companion on one screen | `COMPANION VIEW`: `BATTLE WINGS`, `WINDOW`, `SIDE PANEL`, `WINGS`, `GLASS DRAWER`, `STEP BACK`, with redesigned wing cards |
| Screen | `SHARP` / `xBRZ`, `HANDHELD` / `OLED` colours, `BRIGHTNESS`, black backgrounds and bezel bars, full screen, `ORIENTATION` |
| Controls | Hot plugged controllers (several at once), `NINTENDO` / `XBOX` layouts, hotkeys for paddles and extra buttons, touch and mouse, rumble |
| Games and saves | A box art game grid, your own game folders with saves beside the games, ten state slots, resume, per-game settings, `EDIT SAVE` |
| Play | Level cap, low PP warning, battery saver |

Every change, one line each (a pull request that changes what the fork adds updates this list):

<details>
<summary>The full list</summary>

- **Windows build of the desktop host**: one portable zip that plays GBA, Game Boy and DS games, with an icon,
  a UTF-8 manifest and safe save / state writes on Windows.
- **Nintendo DS on melonDS** in the desktop host, with `DS LAYOUT` (six ways to place the two screens) and touch by
  mouse, finger or controller.
- **COMPANION VIEW** on the desktop host: `BATTLE WINGS`, `WINDOW`, `SIDE PANEL`, `WINGS`, `GLASS DRAWER` and
  `STEP BACK`.
- **Controllers**: hot plugging, several at once (a handheld's own controls and an Xbox pad together).
- **Shaders and colours** on the desktop host: `SHARP` and `xBRZ` scaling, `HANDHELD` and `OLED` colours,
  `BRIGHTNESS` (80% to 150%, 110% by default), `DIM COMPANION`, and `BACKGROUND` (`BLACK` by default with
  `OLED` and in every DS game, `DARK` with `DARK MODE`): true black round the game, the wings as near black cards with a faint edge (dark mode and themes
  reach them too), a thin grey gutter at the game's border, still wings dimmed after a minute and moved a pixel
  every few minutes against burn in. The wings fill the screen's height: the party as tall cards, the map round
  the player with badges, play time and money under it. Bezel bars above and below the game where there's room
  (`BEZEL STYLE` `BLACK` / `GLOW` / `THEME`, each of six places picked in `BEZEL BARS`, the foe and matchups in a
  battle).
- **More in the bezel bars** on the desktop host: `NEXT BOSS AND CAP`, `CAUGHT HERE`, `LEAD POKEMON` (HP and EXP to
  go) and `LAST SAVED` (a warning after 30 minutes), the bottom bar starting on lead / next boss / money; an empty
  place borrows something neither bar shows, larger text, and the bars now show on 16:9 screens with the wings.
- **Redesigned wing cards** on the desktop host: rounded cards with a soft gradient and a type colour stripe,
  shiny star and held item marks, pixel headings (Silkscreen) and clear numbers (Atkinson Hyperlegible), HP
  bars that glide and cards that slide in when the party changes (`REDUCE MOTION` turns both off).
- **Wings that move**: a gold glow on a card that levels up, a popping new badge, money that counts to its new sum, bars that fade between words, and short notes over the right wing for a level up, a new area, a save or the spray wearing off (`REDUCE MOTION` stops the motion)
- **Battle wing**: the matchup wing lists the types the foe is weak to and, at its foot, the party Pokemon to switch to
- **UI SIZE, WING ROOM and STRETCH** on the desktop host: the menus, HUD, wings and companion are drawn a size
  smaller by default (`UI SIZE` `MEDIUM`; `LARGE` is the old size, `SMALL` smaller still), `WING ROOM` shares the
  width between the game and the wings (`AUTO` by default: `WINGS FIRST` while the game still fills 80% of the height, else
  `BALANCED`; or `GAME FIRST`, `BALANCED`, `WINGS FIRST`), `STRETCH` (off) lets
  the game lose up to 5% of its shape to fill the screen, and the zoom buttons turn `EXPAND` on themselves. The DS
  layouts with wings and the DS companion follow `WING ROOM` and `UI SIZE` too
- **Tap the HUD and zoom in WINGS** on the desktop host: the HUD's party slots, minimap, balls, `PC`, `BAG`, `WILD`
  and `COMPANION` answer a tap or click, the first click on the window counts, a tap on the party wing in a battle
  opens `PARTY`, and LT / RT zoom the game's area in `WINGS`
- **Wing look and clicks** on the desktop host: `WING LOOK` (`COMPACT`, `COMFY`, `BIG`) sizes the wings, `BACKGROUND` `BLUR` puts a blurred copy of the game behind the cards, a click on a wing opens the companion on its page, the map wing shows this area's wild kinds as icons (the ones not caught yet faded), and a small card at the bottom of the left wing shows the buttons when no bar does
- **Fuller wings** on the desktop host: an EXP bar under each HP bar, an outlined card with a ball for each free
  party place (seen on dark wings too), long names given the level's room before they're cut, badges as eight
  dots, the next boss and this area's catches under the map, nothing the bars already show said again, and bar
  text a size larger (a step smaller when three places wouldn't fit).
- **Game folders and saves beside games** on the desktop host: folders picked in the app (`GAME FOLDERS`,
  a folder browser that works with a controller) are listed with one level inside them, and their GBA / Game
  Boy saves sit next to the game (`SAVES BESIDE GAMES`, on), so an existing emulation folder works as it is.
- **Full screen** on the desktop host: `FULLSCREEN` (borderless) switched any time by F11, Alt+Enter, L3 + R3, the
  game menu or `SETTINGS`, remembered, and on by default the first time on a handheld's screen.
- **Button layout and hotkeys** on the desktop host: `BUTTON LAYOUT` `NINTENDO` / `XBOX` picks which face button is
  A; LT / RT zoom out / in and L3 toggles fast forward by default (L2 / R2 stay the DS controls in a DS game);
  a tap on R3 switches WINGS / BATTLE WINGS (in a DS game it steps the DS layouts, skipping any that look the same
  on your screen), a hold picks a view; the triggers, sticks and extra
  buttons (GUIDE, SHARE, back paddles) can each be given a job in `HOTKEYS`: game menu, companion, fast forward,
  states, screenshot, full screen, DS layout, swap screens, zoom in / out. EXPAND's camera eases in for text,
  menus and battles and back out after, like the Android app's.
- **Touch and orientation** on the desktop host: touch and the mouse on the game list, menus and companion,
  `GAME TOUCH` (`DS SCREEN ONLY` / `OFF`) for the game itself, and `ORIENTATION` turning the whole picture and touch
  for a screen mounted upright or upside down.
- **Android's settings menus** on the desktop host: the companion's `SETTINGS` in the app's groups and order, and
  a picker for `COMPANION VIEW` and `DS LAYOUT` that draws every choice, from the companion or the game list.
- **Game grid with box art** on the desktop host: the game list is a grid of box art (libretro thumbnails, then
  SteamGridDB with your own key; your own picture wins), each game with its system, `ORIGINAL` / `ROM HACK` and its
  version; `GAME FOLDERS` and `SETTINGS` in a slim top bar; `FIND BOX ART`, `SET BOX ART` (a copied path or link, or
  an image dropped on the window) and `RENAME` from `X`, a right click or a long press; D-pad, mouse, wheel and touch.
  Tiles are box shaped (3:4) with the art filling them, long names wrap to up to three lines, `SYNC BOX ART` fetches
  every game's art with progress (`MISSING ONLY` or `ALL AGAIN`), an original under any file name gets its box from
  its header's game code, and a hack with no art of its own gets its base game's box with a name banner.
- **DS wings** on the desktop host: `TOUCH IN WING` puts the touch screen in one wing and the companion's party in
  the other; `WINGS` is `ONE SCREEN` with party and trainer panels in both wings and the touch screen floating in;
  `TOUCH PIECES` takes the touch screen apart into its buttons, each on a card in the wings, balanced between them
  (HeartGold / SoulSilver's and Platinum's battle menu, moves, bag and party; other screens and games get the screen's
  two halves).
- **Low PP warning** on the desktop host: a move's PP turns orange at a quarter left (2 where the full PP isn't known)
  and red at 0, in the summary, the party's move list and the battle's moves.
- **Level cap** on the desktop host: SETTINGS > GAMEPLAY > LEVEL CAP takes the next unbeaten leader's top level as the
  cap, named under GUIDE > NEXT BOSS, and party levels past it show orange.
- **Save state slots** on the desktop host (GBA and Game Boy): ten per game in the companion's STATES tab, each with a
  picture and the time it was saved; the save / load hotkeys follow the picked slot, and slot 0 is the old single state.
- **Resume games** on the desktop host (GBA and Game Boy, like DS's AUTO-RESUME): leaving a game keeps a state, and
  the next start asks CONTINUE or START FROM SAVE (SETTINGS > APP > RESUME GAMES, on); the save file is never touched.
- **Rewind on the desktop host, DS too**: a `REWIND HOLD` hotkey (any HOTKEYS button, or R on a keyboard) rewinds
  while held; DS games keep a state a second for the last 20 s, never taking back the battery save.
- **Rumble** on the desktop host (SETTINGS > CONTROLS > RUMBLE, off): the controller buzzes as a battle starts,
  harder for a shiny foe, and with a rumble cartridge.
- **Per-game settings** on the desktop host (SETTINGS > APP > THIS GAME ONLY): a game keeps its own settings in
  `DATA/game-settings/`, while buttons, hotkeys, the window and the like stay program-wide.
- **Battery saver** on the desktop host (SETTINGS > APP > BATTERY SAVER, AUTO = on battery): fast forward capped at
  3x, xBRZ drawn as SHARP, the companion and game list redrawn half as often.
- **DS time of day and Berry Pots** on the desktop host: the DS facts wing shows MORNING / DAY / (EVENING) / NIGHT,
  and HeartGold / SoulSilver get their party and facts wings with each Berry Pot's berry, stage, time left and DRY.
- **DS buttons as a GBA game's** on the desktop host: `HOTKEYS` now apply in DS games too, so by default LT / RT
  zoom (the game against the wings), L3 fast forwards (new on DS: three frames a turn, muted), R3 steps / picks the
  DS layout, `GUIDE / HOME` opens the quick menu and SELECT + R3 swaps the screens; `USUAL USE` on LT / RT gives
  back the DS's own L2 quick menu and R2 swap.
- **DS Platinum by its header** on the desktop host: any `CPUE` revision 1 cart gets the Platinum companion, as on
  Android (a trimmed or re-dumped copy, Renegade Platinum), and another DS game's companion names the cart it found
  (game, region, revision, code) and the one it needs.
- **EDIT SAVE** on the desktop host: a game's menu changes its save file (money, coins, the bag, party and PC
  Pokemon: species, nickname, level, nature, ability, item, moves, IVs, EVs, shiny; heal) for the GBA games with
  the retail save layout and English HeartGold / SoulSilver / Platinum, only to values the game allows, with a backup first.
- **Updates**: the program checks this fork's releases and updates itself on Windows.
- **Releases**: a desktop release (`desktop-vX.Y.Z`) is cut automatically when `main` gets a `feat`, `fix` or `perf`
  change. Desktop builds are a beta: every one goes up as a GitHub pre-release until one is called stable, and the
  update check counts pre-releases.
- **Upstream mirroring**: a daily job opens a pull request bringing PokeDaisy's own changes in.

</details>

## Building

```bash
git submodule update --init third_party/mgba third_party/melonds
cmake -S linux -B linux/build -DCMAKE_BUILD_TYPE=Release -DPOCKET_MELONDS=ON
cmake --build linux/build
ctest --test-dir linux/build
```

On Windows build in MSYS2's UCRT64 shell (gcc, cmake, ninja, SDL2) and package with
`python linux/windows/package.py --melonds linux/build/pokedaisy.exe OUT`. More in [linux/README.md](linux/README.md).

## The original PokeDaisy

Everything else, the Android app and the companion itself, is Lidor's PokeDaisy: its own README is kept in
[docs/pokedaisy-readme.md](docs/pokedaisy-readme.md), and its home is [pokedaisy.com](https://pokedaisy.com) and
[lidor30/pokedaisy](https://github.com/lidor30/pokedaisy).
