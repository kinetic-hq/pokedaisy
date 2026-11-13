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

## Game support

The goal is for every supported game to be `FULL`. The games the maintainer plays come first, then every other game
listed on the site. Each tile in the game list carries a tag:

- `FULL`: every companion tab follows the game (PARTY, BAG, DEX, MAP, GUIDE and, in a battle, BATTLE).
- `PARTLY`: the game runs and the companion follows it, but a tab is missing or it's read as the game it's built on.
  `COMPANION` in the game's options says what's missing.
- `PLAY ONLY`: the game runs, but the companion doesn't follow it yet.

`PARTLY` and `PLAY ONLY` are steps on the way to `FULL`, not where a game stops.
[desktop-game-support.md](docs/manual/desktop-game-support.md) lists each game's state and what's left.

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
  version; `GAME FOLDERS` and `SETTINGS` in a slim top bar; `BOX ART` (a picker over every cover found, see below) and
  `RENAME` from `X`, a right click or a long press; D-pad, mouse, wheel and touch.
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
- **DS companion in the app's look** on the desktop host: the DS companion, pause screen and toasts use the theme's
  backdrop, windows and tab colours (and `DARK MODE`) like the GBA companion, and fill the window at `UI SIZE`'s
  scale instead of a small fixed box.
- **The full DS companion** on the desktop host: DS games get the handheld core's companion (`PARTY` summaries, `BAG`
  pockets, `DEX` pages, `MAP` with `PLACES`, `GUIDE`, and `BATTLE` with the foe and your moves' verdicts) instead of
  a text list, with the desktop's DS rows under its `SETTINGS`; HeartGold and SoulSilver (USA) get it all too, on
  the app's HeartGold field read (`BATTLE`, auto battles, the HUD and wings with the party's icons).
- **FireRed rev 0's tables**: the USA/Europe 1.0 cart gets the `DEX` pages, the `GUIDE`'s trainers and wild
  Pokemon, and the foe's team in `BATTLE`, on the desktop, the 3DS port and Android alike.
- **LeafGreen rev 0's foe team**: the USA 1.0 cart shows the trainer's whole team in `BATTLE` (FireRed rev 0's
  battle RAM, which it shares), on every platform.
- **Foe teams in six hacks**: Gaia, Odyssey, Unbound, Heart and Soul, Radical Red and Celia's hack show the trainer's whole team in `BATTLE`,
  on the desktop, the 3DS port and Android alike.
- **ASPECT starts on EXPAND** on the desktop host (Android starts on `ORIGINAL`), so `AUTO ZOOM IN` and the zoom hotkeys work
  from the first game; `ORIGINAL` stays a choice, and a pick you made is kept.
- **The game grid** on the desktop host: whole covers (never cropped), a `FULL` / `PARTLY` / `PLAY ONLY` tag from
  the companion's own detection (`COMPANION` in a game's options says what's missing), `ROMHACK` on every hack's tile,
  SteamGridDB art only for the same name and in English (its escaped "Pok\u00e9mon" read as the word it is, spaces and
  "and" not counting, and a hack left on its base game's box looked for again once), and dialogs drawn over the dimmed art.
- **BOX ART picker** on the desktop host: a game's `BOX ART` option shows every cover found as a grid, like
  SteamGridDB's own picker in Steam's Big Picture: libretro's cover and the base game's box, then every SteamGridDB grid for
  the game, a page at a time as you scroll, each tagged with its source and size. `SHAPE` shows box shaped covers or
  all of them, `SEARCH AGAIN` types another name and picks SteamGridDB's game from a list, `CHOOSE FILE` takes a path
  or link, `AUTOMATIC` drops the pick. A pick is the game's own art: `SYNC BOX ART` never replaces it.
- **Black, White, Black 2 and White 2 on the desktop host**: the USA / Europe carts (any dump with their header) get the
  whole DS companion (`PARTY`, `BAG`, `DEX`, their own Town Map in `MAP`, `GUIDE`, `BATTLE`), the party and facts wings,
  the HUD and EXPAND, on the app's Gen 5 reader and field reads.
- **Origin HeartGold's type chart on Android** is found inside the game's own code, so other dumps of the same build
  read the right matchups instead of stray bytes.
- **Renegade Platinum on the desktop host**: its own names, species types, moves, Fairy type, type chart, TMs and icon
  palettes come from the cart, so `PARTY`, `BATTLE` and `DEX` show the hack's data; its `GUIDE` tab is hidden (as on
  Android), and so is Origin HeartGold's.
- **Origin HeartGold's `GUIDE` on the desktop host** (beyond Android, which has none for it): `HERE` lists the wild
  creatures where you stand by time of day, read from the cart's own map headers and encounter tables, and the `DEX`
  page's where-to-find uses them too. `NEXT BOSS` shows the next gym leader or League member with the hack's own team,
  read from the cart's trainer data, in the order of their levels (Kanto's leaders first).
  `HERE` also lists the item balls and hidden items where you stand, from the cart's map events and scripts.
- **Renegade Platinum's `GUIDE` on the desktop host** (beyond Android, which has none for it): `HERE` lists the wild
  creatures where you stand by time of day, from the cart's own map headers and encounter tables, and `NEXT BOSS` shows
  the next gym leader or League member with the hack's own team, read from the cart's trainer data.
  `HERE` also lists the item balls and hidden items where you stand, from the cart's map events and scripts.
- **Origin HeartGold's `BATTLE` on the desktop host**: its own battle engine's objects are read (as on Android): your
  creature and the foe with their stats, types and moves, and a trainer's team.
- **HeartFelt Gold's `BATTLE` on the desktop host**: it keeps HeartGold's battle structures, so the panes read your
  creature and the foe (with the build's own species, moves and Fairy type) and a trainer's team. Auto battles and
  EXPAND stay off for it.
- **HeartFelt Gold's `DEX` on the desktop host**: its Pokédex (1534 species ids, caught and seen) read from the save,
  listed in National Dex order, with each page's front, stats, size, text, evolutions and level-up moves from the cart.
  Its TM01 to TM100 and HM01 to HM08 come from hg-engine's machine learnsets and moves in the cart.
- **HeartFelt Gold's `GUIDE` on the desktop host**: `HERE` lists the wild creatures where you stand by time of day,
  read from the cart's own map headers and encounter tables, with the build's own species. `NEXT BOSS` shows the next
  gym leader or League member with the hack's own team, read from the cart's hg-engine trainer data.
- **HeartFelt Gold's EXPAND and auto battles on the desktop host**: its code is retail HeartGold's where the field
  read looks (the same ARM9 and field overlay bytes), so it runs on HeartGold's field read: EXPAND, `BATTLES` `AUTO`
  and the touch screen floating in for menus.
- **HeartFelt Gold on the desktop host** (an hg-engine build of HeartGold, not read on Android): `PARTY`, `BAG` and
  `MAP` on its own save offsets (its bigger bag moves the rest of the save), with its species to 1534, moves, items,
  Fairy type, type chart and icon palettes read from the cart. No `DEX`, `GUIDE` or `BATTLE` yet. The game list's tag
  now reads Renegade Platinum as "every tab but GUIDE" and knows Origin HeartGold too.
- **Origin HeartGold on the desktop host**: `PARTY`, `BAG`, `MAP` and `DEX` (Pokédex pages, pictures, learnsets,
  machines, evolutions) on its own save offsets, with its species, moves, items, Fairy type and type chart read from
  the cart. No `BATTLE` panes yet.
- **DS EXPAND** on the desktop host: Platinum, HeartGold and SoulSilver (and the games built on them) draw their world past
  the top screen's edges, Android's EXPAND (`pk_nds_wide.cpp`) on the desktop's melonDS, with the GBA's zoom steps and
  AUTO ZOOM IN.
- **The game list's header** on the desktop host: the app's daisy, redrawn as 16 x 16 pixel art, beside the PokeDaisy
  name, instead of "PokeDaisy Windows" / "PokeDaisy Linux" (the window title and SETTINGS' HOST row keep the platform).
- **DEX for expansion hacks** on the desktop host and the 3DS port: Heart and Soul, Wishes of Tomorrow, Dreamstone
  Mysteries and Lazarus (and any pokeemerald-expansion game whose gSpeciesInfo the ROM is seen to hold) get the `DEX` tab, read
  from the game's own species data (Lazarus, which zeroes the species it leaves out, is checked on another one); the game grid's support tag asks the same rule.
- **Monomyth's DEX** on the desktop host, from its own tables (its pages, smol pictures and region list), and **metric
  units** on the `DEX` page of every GBA game whose cart uses them (Monomyth, the European and Japanese carts), as in the app.
- **Amethyst 1.4.3**: its own config (PARTY, BAG, BATTLE with the foe's team, DEX, GUIDE, the PC) instead of 1.4.1's
  guessed one, which read the wrong ability names and foe teams; on the desktop, the 3DS port and Android alike.
- **Petalburg Dynasty 1.12.2**: a new supported hack (pokeemerald-expansion 1.16.4, its sha1 only): PARTY, BAG, BATTLE
  with the foe's team and the battle buttons, MAP place names and the GUIDE's wild Pokemon on the desktop, the 3DS port
  and Android, and its DEX on all three.
- **R.O.W.E.'s GUIDE**: HERE lists the wild Pokémon of the place you're in, read from the game's own tables (a wider
  header than Emerald's, with day and night sets), on Android, the desktop host and the 3DS port. Its gyms come in any
  order, so there's no NEXT BOSS.
- **Petalburg Dynasty's Tera type**: BATTLE shows a Terastallized Pokémon's Tera type, and the verdicts and weaknesses
  that go with it, on Android, the desktop host and the 3DS port. A Stellar Tera keeps its own types.
- **FF MUSIC's STEADY on the desktop**: in GBA games, each song is recorded on its own on a second core the first time
  the game plays it, cut where the sound engine loops, and plays at its normal speed while the game races, as on
  Android. It's the default for new setups. Clips are WAVs in `ffmusic/` beside the saves, 1.5 GB at most; there's no
  background pass over every song.
- **SCENE SPEEDS on the desktop, and GAME SPEED that works there**: `SPEED OPTIONS` gets `BATTLE`, `TEXT`, `MENU`,
  `BIKE` and `SURF SPEED` in Game Boy and GBA games, as on Android (`MENU SPEED` where `EXPAND` reads the screen).
  `GAME SPEED` was shown on the desktop but never applied; it now sets the pace in between.
- **TRADE EVOS on the desktop**: in the English FireRed (rev 1), LeafGreen and Emerald, a party Pokémon that would
  evolve by trading gets `EVOLVE` on its summary, and the game plays its own evolution, as on Android.
- **NUZLOCKE on the desktop**: the Android app's tracker and its tab (under `MORE`): one encounter per area, fainted
  Pokémon out for good, fixes by hand, `DUPES CLAUSE`, `WARNINGS` and `NEW RUN`, in the same GBA games. Each game's run
  is kept in `nuzlocke/` beside the saves, in the app's own format.
- **BATTLE INFO and BATTLE VIEW on the desktop**: the two `COMPANION` settings from the Android app. `BATTLE INFO` off
  leaves the `BATTLE` tab with only the battle buttons (no `INFO`, `SUGGESTIONS` or move ratings), and the battle wing
  keeps its map. `BATTLE VIEW` set to `INFO` opens the tab on `INFO`.
- **FOE IVS on the desktop**: the `COMPANION` setting from the Android app. Battle `INFO` gets `STATS`, your Pokémon and
  the foe side by side with each stat's IV and EV and the nature, in wild and trainer battles.
- **SHINY ODDS on the desktop**: `GAMEPLAY` gets `SHINY ODDS` (`NORMAL` up to `1 IN 128`) in the same GBA games and
  hacks as on Android. A wild foe is made shiny as the game creates it, keeping its nature, gender and ability.
- **INSTANT TEXT and REUSABLE TMS on the desktop**: the two `GAMEPLAY` settings from the Android app, found in the
  ROM's own code the same way, so they fit the same GBA games and show `N/A` elsewhere.
- **DS double battles on the desktop**: Black, White, Black 2 and White 2 show the BATTLE tab in a double battle too,
  with the foe on the left and your Pokémon on the left, as a GBA double does. Before, the tab didn't appear at all.
- **DS triple and rotation battles on the desktop**: Black, White, Black 2 and White 2 show the BATTLE tab in these
  too. A triple shows the left pair as a double does; a rotation shows the Pokémon in front on each side, following
  every rotation. Before, the tab didn't appear in either.
- **Petalburg Dynasty's MAP picture** on the desktop host, the 3DS port and Android: the game keeps Emerald's own map,
  stored in the newer smol compression, and now shows it with each of its places where the game puts them. On the desktop
  and the 3DS port, Wishes of Tomorrow's map shows too: its pictures were too large for the smol reader before.
- **DS Platinum by its header** on the desktop host: any `CPUE` revision 0 or 1 cart gets the Platinum companion (rev 1 as on
  Android, rev 0 too here: its addresses are the same; a trimmed or re-dumped copy, Renegade Platinum), and another DS game's companion names the cart it found
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
