---
title: Windows handhelds and PCs
summary: Run the Linux host's PokeDaisy on a Windows handheld or PC from a portable zip, with your own games and saves.
section: start
order: 91
keywords: [windows, pc, ui size, too big, smaller, wing room, stretch, update, updates, check for updates, new version, rog ally, legion go, onexplayer, steam deck, portable, zip, exe, sdl, controller, fullscreen, full screen, f11, alt+enter, borderless, windowed, button layout, xbox, nintendo, a and b swapped, hotkeys, paddles, back buttons, guide, home button, share, screenshot, controls, touch, touch screen, mouse, tap, game touch, orientation, portrait, landscape, rotate, rotation, flipped, upside down, vertical screen, companion view, battle wings, side panel, glass drawer, step back, nds, ds, nintendo ds, melonds, touch screen, mouse, ds layout, one screen, picture in picture, touch in wing, stacked, side by side, large + small, shader, shaders, xbrz, sharp, colors, colours, handheld, oled, dim companion, black panels, brightness, bezel, bezel bars, bezel style, glow, zoom in, zoom out, remap, r3, view, game folders, roms folders, roms-folders.txt, other folders, emulation folder, retroarch, saves beside games, save next to game, sav, time of day, night, berries, plant pots, heartgold, soulsilver]
---

The Windows version is the [Linux host](linux.md) built for Windows: the same
emulator, quick menu, HUD and companion, on a Windows handheld (an ROG Ally, a
Legion Go, a OneXPlayer) or any PC. It plays Game Boy, Game Boy Color and Game
Boy Advance games you own, and Nintendo DS games (below), all from one download. It is early, like the Linux version, so keep a copy
of your saves. [Desktop and Android compared](android-parity.md) lists what it
shares with the Android app and what it doesn't have yet.

## Install it

There is no installer. Download `pokedaisy-windows-x86_64.zip` from the
[Releases page](https://github.com/kinetic-hq/pokedaisy/releases) (the top `desktop-v` one: desktop builds are a beta, marked pre-release), unzip it anywhere you can write to (your Documents folder is
fine, `Program Files` isn't), and you get:

- `pokedaisy.exe` and `SDL2.dll`, which must stay side by side
- a `DATA` folder with `roms`, `saves` and `states` in it
- the licenses and this program's source record

Put your `.gba`, `.gbc`, `.gb` and `.nds` files in `DATA\roms`, then open
`pokedaisy.exe`. It lists your games; pick one with the D-pad and press A.
Everything the program writes stays in that `DATA` folder, so you can move the
whole folder to another PC or a memory card.

The game list shows each game by its own name, the way the Android app's
library does: a file's region and dump tags, version numbers, underscores and
dashes are left out, the game's own spelling is used, and "Pokémon" goes first.
Your files keep their names.

## Get started

The first time you open PokeDaisy with no games yet, `GET STARTED` walks you
through the choices that matter most, one page each, like the app's first
time setup: `GAME FOLDERS` (add the folders that hold your games),
`BUTTON LAYOUT`, `FULLSCREEN`, `COMPANION VIEW`, `DS LAYOUT` and `COLORS`
(`OLED` keeps everything around the game black), then your game list. A picks
a row and moves on, B goes back a page, and each choice is saved as you make
it. `SKIP SETUP` on the welcome page leaves it for later. To see it again, open
`SETTINGS` > `APP` > `RUN SETUP`.

## Your game list

Your games open as a grid of box art. Each tile shows the whole box, never cut
off, over a blurred copy of it where the box is a different shape. Each game
shows its name (on up to three lines when it is long), the system (`GBA`, `GBC`,
`GB` or `NDS`), `ORIGINAL` or `ROMHACK`, its version when the file name has one,
and how much of the companion it gets:

- **`FULL`**: every companion tab, including the `DEX`, the `GUIDE` and the
  foe's team in `BATTLE`.
- **`PARTLY`**: the companion, with some of that missing. Press `X` on the game
  and pick `COMPANION` to see what.
- **`PLAY ONLY`**: the game runs, without a companion.

Move with
the D-pad, the mouse or a finger; `A` or a tap plays, the mouse wheel or a drag
scrolls, and `Y` opens `SETTINGS`. `SYNC BOX ART`, `GAME FOLDERS` and `SETTINGS`
sit in the bar along the top: press UP from the first row to reach them. The
bar's left end shows the PokeDaisy daisy and name; the window's title and
`SETTINGS` > `HOST` still say whether it's the Windows or the Linux build.

Box art is looked for by itself in the background (`SETTINGS` > `LIBRARY` >
`BOX ART DOWNLOAD`, on): first the libretro thumbnails, which have the original
games, by the file's name and then by the game the file itself says it is (so a
game under a name of your own still gets its box), then SteamGridDB when you add
your own key, which has many ROM hacks. SteamGridDB's art is only used when its
game has the same name as yours, and only in English, so a hack never gets the
box of a sequel or a spin off. Spaces and "and" don't count, so "Ash Gray" finds
"AshGray" and "Heart and Soul" finds "Heart & Soul". To add one, make a free key on
steamgriddb.com (Preferences, API), copy it, then pick `SETTINGS` > `LIBRARY` >
`STEAMGRIDDB KEY` > `PASTE THE KEY FROM THE CLIPBOARD`. A ROM hack nobody has art
for gets the box of the game it was made from, with the hack's name on an orange
banner across the bottom. Box art is kept in `DATA\boxart`.

`SYNC BOX ART` looks for every game at once, with its progress along the bottom
of the screen. `MISSING ONLY` tries the games that have no art yet, even those it
found nothing for before. `ALL AGAIN` looks again for every game and replaces the
art it found before (for a hack that got a new box on SteamGridDB, say). Your own
pictures always stay.

Press `X`, right click or hold a finger on a game for its options:

- **`COMPANION`** says how much of the companion the game gets and, for
  `PARTLY` and `PLAY ONLY`, what's missing.
- **`FIND BOX ART`** looks again now, even for a game it found nothing for before.
- **`SET BOX ART`** uses your own picture: copy a PNG or JPG file's path, or an
  https link to one, then pick `USE THE COPIED PATH OR LINK`. You can also drop
  an image on the window to set the selected game's art. Your own art always
  wins, and `REMOVE MY BOX ART` goes back to the downloaded one.
- **`RENAME`** gives the game a name of your own, typed on a real keyboard or
  picked key by key with a controller (`B` deletes, `Y` is a space, `START` is
  done). `USE THE FILE'S NAME` takes it back. Names are kept in `DATA\names.txt`;
  the files themselves are never renamed.
- **`EDIT SAVE`** changes the game's save file: see "Editing a save" below.

## Games in other folders

Your games can stay where they are, in a folder another emulator or a sync app
already uses. Pick `GAME FOLDERS` in the bar at the top of the game list (or
`SETTINGS` > `LIBRARY` > `GAME FOLDERS`), then `ADD A FOLDER`: you see your drives, `A` opens a folder, `B` goes back up, and
`USE THIS FOLDER` adds the one you're in, with a controller or the keyboard.
Pick a folder on the `GAME FOLDERS` list to take it off the list again; its
games and saves stay where they are.

PokeDaisy lists the games in each of those folders and in the folders right
inside them (so `D:\Emulation\roms` also finds `D:\Emulation\roms\gba` and
`D:\Emulation\roms\nds`, but not a folder deeper down). `DATA\roms` and the
folders inside it are always listed too, and folders whose name starts with a
dot are skipped. The list is kept in `DATA\roms-folders.txt`, one folder a
line, if you'd rather write it yourself.

## Updates

Each time it starts, PokeDaisy looks for a newer release in the background, so
your game never waits on it and nothing shows when you're offline. When there
is one, the game list asks what to do (in a game, a line at the top tells you
first):

- **`UPDATE NOW`** downloads it and checks it is the release's own file. It
  goes in when you close PokeDaisy, and the next start is the new version. Only
  the program's files change: the `DATA` folder (your settings, saves, states
  and games) is left as it is.
- **`LATER`** asks again the next time you start it.
- **`SKIP THIS VERSION`** doesn't ask about that release again, only a newer
  one.

On Linux `UPDATE NOW` reads `OPEN DOWNLOAD PAGE` and opens the release in your
browser. To stop looking for updates, turn `CHECK FOR UPDATES` off in the
companion's `SETTINGS`.

## Saves

Your in-game saves go to `DATA\saves`, named after the game's file
(`My Game.gba.sav`). To bring a save from another emulator, copy a backup of it
there under that name before you start the game.

A game from one of your own game folders (above) keeps its save next to it
instead, the way RetroArch and the Android app keep them: `My Game.gba` saves as
`My Game.sav` in the same folder. If that file isn't there yet but the game has
a save in `DATA\saves`, a copy of it is put beside the game first; the one in
`DATA\saves` is left as it was. To keep those saves in `DATA\saves` too, turn
off `SAVES BESIDE GAMES` in the companion's `SETTINGS`. DS games always keep
their save beside the game.

Each time a game starts, its save is first copied to a `pokedaisy-backups`
folder next to it (`DATA\saves\pokedaisy-backups` unless the save sits beside
the game), and the newest ten
different copies are kept. Save states go to `DATA\states`. A state is written
to a new file first and then put in place in one step, so a crash while saving a
state never leaves half a file behind.

In GBA and Game Boy games each game has ten state slots, in the companion's
`STATES` tab (on the tab bar, or under the tiles button). Each card shows a
small picture of the game from the moment you saved and when it was saved. Pick
a card, then `SAVE` or `LOAD` under it. The `SAVE STATE` and `LOAD STATE`
hotkeys use the card you picked last. Slot 0 is the one state file from earlier
versions, so a state you already had is still there.

`RESUME GAMES` (in `SETTINGS`, `APP`, on at first) keeps your place when you
leave a GBA or Game Boy game. The next time you open it you're asked
`Continue where you left off?`: `Continue` puts you back where you were,
`Start from save` (or B) starts from your save as usual. Your save file is never
changed by this. If the save was played somewhere else since, the question is
skipped and the game starts from the save.

`THIS GAME ONLY` (in the companion's `SETTINGS`, `APP`) gives the game you're
playing its own settings. Turn it on and every change you make while that game
runs (fast forward, `ASPECT`, the view, the `HUD`, `LEVEL CAP`, the shader and so
on) stays with that game; your other games keep your usual settings. Your
buttons, `HOTKEYS`, `FULLSCREEN`, `RUMBLE`, `BATTERY SAVER`, `RESUME GAMES` and
`CHECK FOR UPDATES` are the same for every game, so changing one of them here
still changes it everywhere. Turn `THIS GAME ONLY` off to put the game back on
your usual settings; what it had of its own is forgotten.

`BATTERY SAVER` (in `SETTINGS`, `APP`) helps a handheld PC last longer.
`AUTO`, the first choice, saves only while you're on battery and reads
`AUTO, SAVING` then; `ON` always saves, `OFF` never does. While it saves, fast
forward goes no faster than 3x, the `xBRZ` shader draws as `SHARP`, and the
companion and the game list are redrawn half as often (the game itself still
runs at its full speed).

`RUMBLE` (in `SETTINGS`, `CONTROLS`, off at first) makes your controller buzz
briefly when a battle starts in a GBA or Game Boy game, and harder when the foe
is a shiny one. A Game Boy cartridge with a rumble motor shakes the controller
too. Turning it on in the program's own settings gives you a short buzz, so you
know it works.

### Editing a save

`EDIT SAVE` in a game's options in the game list (press `X`, right click or hold
a finger on the game) changes the game's save file while the game
isn't running. It covers FireRed, LeafGreen, Emerald, Ruby and Sapphire in
English and the other Western languages, the hacks that kept those games' save
layout, and English HeartGold, SoulSilver and Platinum. Any other game, or a save that
doesn't check out, is refused and left alone.

- `MONEY` and `COINS`: type a new amount, up to what the game allows.
- `BAG`: change how many of an item you have, take one out, or `ADD AN ITEM`
  (GBA games). Key items and HMs stay as they are.
- `PARTY` and `PC BOXES`: pick a Pokemon to change its species, nickname,
  level, nature, ability, held item, moves, IVs, EVs and whether it's shiny.
  `ALL IVS TO 31` fills them in one go. Only values the game itself could have
  are taken: a move the game doesn't have, the same move twice, EVs over 510 or
  a level over 100 are refused with the reason.
- `HEAL THE PARTY` puts every party Pokemon back to full HP and PP with no
  status.
- `SAVE CHANGES` writes it all. Before anything is written, the old save is
  copied next to it as `<save>.before-edit-<date>-<time>.sav`, and the new file
  is read back and checked; if that fails, the old save is put back.

Leaving with changes you haven't saved asks you first. Saving also sets aside
the game's `Continue where you left off?` point from before the edit (it would
take you back to the old save), so the next start is from the edited save. That
point isn't deleted: it's kept in `DATA\states` with `.before-edit-` and the date
added to its name.

## Full screen and controllers

Press F11 or Alt+Enter to switch between full screen and a window, any time: in
the game list, in a GBA game or in a DS game. On a controller, press both
sticks down together (L3 and R3). The game's menu has `Toggle fullscreen` too,
and the companion's `SETTINGS` has a `FULLSCREEN` row. Full screen covers the
whole display at its own resolution, with no black flash, and your choice is
remembered for the next start.

The first time PokeDaisy starts it picks for you: full screen on a handheld's
screen (under about 10 inches), a window on a bigger one. A window can be
resized. `POCKET_FULLSCREEN=1` still starts it full screen whatever you chose,
for a frontend or a shortcut, and `POCKET_WINDOW=1920x1080` opens the window at
a size you choose, to try a handheld's layout on a desktop.

Built in handheld controls and any SDL controller work as they come. A
controller can be plugged in, switched on or pulled out while you play, and on a
handheld its own controls and an Xbox controller both work at once. For a controller it
doesn't know, put a line for it in `DATA\gamecontrollerdb.txt` (the community
file's format). On a keyboard: the arrow keys, Z for A, X for B, Enter for
START, Backspace for SELECT, Q and E for L and R, Tab for the companion, F for
the quick menu, V for the next companion view, K to lock the side panel and
Escape for the game menu.

### Button layout and hotkeys

`SETTINGS` > `CONTROLS` on the game list's rail sets up your controller.
`GAME BUTTONS` > `BUTTON LAYOUT` says which face button is the game's A. Pick the one printed on
your controller: `NINTENDO` (the default) makes the right button A and the
bottom one B, as on a Nintendo style controller or a handheld with the A on the
right; `XBOX` makes the button labelled A, the bottom one, the game's A. The
companion's `SETTINGS` has the same `BUTTON LAYOUT` row.

In `HOTKEYS`, the triggers and sticks come first (`L2 / LT`, `R2 / RT`, `L3`,
`R3`), then each extra button your controller may have: `GUIDE / HOME`, `SHARE`
and the four back `PADDLE` buttons. Out of the box LT zooms out, RT zooms in
(`ASPECT` starts on `EXPAND` here, so zoom works from your first game; if you picked `ORIGINAL`, the first press turns `EXPAND` back on, in a game that has it) and L3 turns fast forward on and off.
A DS game uses the same buttons: LT and RT make the game smaller or larger
against the wings (`WINGS FIRST`, `BALANCED`, `GAME FIRST`, for as long as you
play; in a layout without wings the game says so), L3 turns fast forward on and
off, R3 is the DS layout and `GUIDE / HOME` opens the quick menu. Give LT or RT
`USUAL USE` to have them back as the DS's own: L2 the quick menu, R2 swapping
the screens. Pick one and choose
`GAME MENU`, `COMPANION`, `FF TOGGLE`, `SAVE STATE`, `LOAD STATE`,
`SCREENSHOT`, `FULLSCREEN`, `DS LAYOUT`, `DS LAYOUT BACK`, `SWAP SCREENS`,
`ZOOM IN`, `ZOOM OUT` or `REWIND HOLD`, or `USUAL USE` to give it back.

`REWIND` (in `FF / SPEED`) keeps the last 20 seconds or so. While you hold the
button you gave `REWIND HOLD` (or R on a keyboard, with `REWIND` on), the game
runs backwards, muted, and stops at the oldest moment it kept. A button with
`REWIND HOLD` keeps the rewind running even with `REWIND` off. In a Game Boy
Advance or Game Boy game it steps back frame by frame; in a DS game it goes back
a second at a time. Rewinding never takes back a save the game wrote.

R3's usual use is the view. In a Game Boy Advance or Game Boy game a tap
switches between `WINGS` and `BATTLE WINGS`; in a DS game it steps through the
`DS LAYOUT`s in the picker's order, leaving out any that would look the same as
one before it on your screen. Holding R3 opens every view as a small picture to
pick from (A takes one, B leaves it as it was). The view's name shows for a
moment when it changes. A button with
a job no longer reaches the game. Screenshots go to `DATA\screenshots`, named
after the game. Fast forward has no effect in DS games, and the DS LAYOUT and
SWAP SCREENS jobs only in them. Everything is kept in `DATA\settings.ini`.

### Screen, touch and orientation

The game list's `SETTINGS` (in the bar at its top, or `Y`) is in the app's groups: `SCREEN`
(`COMPANION VIEW`, `DS LAYOUT`, `DS WING SIDE` and `DISPLAY OPTIONS`, which has
`SHADER`, `COLORS`, `DIM COMPANION`, `FULLSCREEN`, `ORIENTATION` and
`GAME TOUCH`), `LIBRARY` (`GAME FOLDERS`, `BOX ART DOWNLOAD`,
`STEAMGRIDDB KEY`), `CONTROLS` (`GAME BUTTONS`,
`HOTKEYS`) and `APP` (`CHECK FOR UPDATES`, `RUN SETUP`, `VERSION`). The companion's
`SETTINGS` has the same rows (see "Where the settings are" below).

`ORIENTATION` turns the whole picture for a screen that is mounted another way:
`LANDSCAPE` (the usual), `PORTRAIT` (a quarter turn clockwise),
`LANDSCAPE FLIPPED` (upside down) or `PORTRAIT FLIPPED` (a quarter turn the
other way). The game list, the games and the companion all lay themselves out
for the turned screen, and touch follows the turn.

Touch and the mouse work on the game list, its screens, the game menu and the
companion: tap a row or a tab as you would press A on it. `GAME TOUCH` says what
touch does on a game itself. `DS SCREEN ONLY` (the default) presses a DS game's
touch screen wherever it is drawn and leaves the rest alone, so a stray finger
never moves your character; `OFF` leaves the touch screen to the D-pad
(SELECT + D-pad + A). A DS game's own companion and menus are still worked with
the controller.

To start a game straight away, for a frontend or a shortcut, give its file:
`pokedaisy.exe "D:\Games\My Game.gba"`. `--data FOLDER` uses another data
folder than the one beside the program.

## Companion views

On a wide screen the companion doesn't have to cover the game. Pick how it
shares the screen in the companion's `SETTINGS` > `SCREEN` > `COMPANION VIEW`,
in the game list's `SETTINGS`, or press the right stick (V on a
keyboard) to step to the next one as you play. The picker draws every view as a
small picture of the screen, so you can see where the game and the companion go
before you choose; the new one takes over at once. A filled red mark is a view
the app has on its phones and handhelds too, a hollow one a view added for wide
screens. The game
stays in the middle of the screen in every view but a locked side panel.

- **`BATTLE WINGS`** (the first choice): on the field the game fills the screen
  with the HUD, and the companion opens as a window over the dimmed game. When a
  battle starts, the game shrinks to the largest whole size that leaves room on
  both sides: your party on the left, the matchup on the right (the foe, its
  types, the types it's weak to, how each of your moves does against it and,
  at the foot, the Pokemon in your party that would do better against it). On a 16:10 screen a strip
  above the game shows where you are, the time and your badges, and one below
  it the buttons and the steps left on the spray.
- **`WINDOW`**: the companion as a window in the middle, over the dimmed game.
- **`SIDE PANEL`**: the companion slides in from the right. Press the left stick
  (K on a keyboard) while it's open to lock it: the game moves into the left
  half and the panel keeps the right half.
- **`WINGS`**: the battle layout all the time, with the map on the right when
  you're not in a battle. LT and RT zoom the game's own area beside the wings.

Tap or click the HUD's buttons to use them: a party slot opens its summary, the
minimap opens `MAP`, the balls and `BAG` open the bag, `PC` turns on the game's
PC, `WILD` opens `GUIDE`, and `COMPANION` opens the companion. A tap on a wing
opens the companion on that wing's page (the party wing on `PARTY`, even in a
battle).
- **`GLASS DRAWER`**: a see-through sheet over the bottom of the screen.
- **`STEP BACK`**: the game steps back two sizes and the companion sits around
  it.

Click a wing (or touch it) to open the companion on that wing's page: the
party's wing opens `PARTY`, the other `MAP`, or `BATTLE` during a battle. Under
the map, `CAUGHT HERE` shows this area's wild kinds as small icons, the ones
you haven't caught yet faded. When no strip runs above or below the game, a
small card at the bottom of the left wing shows the buttons for a few seconds
as a game starts, and again when you've been idle a while.

The game moves into its new place smoothly when a view changes.

A move running low on PP shows its PP in orange (a quarter of its PP left, or 2
in games where the full amount isn't known) and in red once it's out, in a
Pokemon's summary and in the battle's move list, so you know to heal before the
next fight.

`LEVEL CAP` (in `SETTINGS`, `GAMEPLAY`, off at first) is for runs with a level
rule. It takes the top level of the next leader you haven't beaten as your cap:
`NEXT BOSS` in the `GUIDE` names it, and a party slot whose level is past it
shows that level in orange. It follows the guide, so games without a `NEXT BOSS`
page have no cap.

## Where the settings are

The companion's `SETTINGS` has the same groups in the same order as the app on
a phone or handheld, so a page you know there is where you expect it here. Rows
only this version has sit in the group they fit best, and rows that only make
sense on a phone (touch controls, the second screen, sign ins) are left out.

| Group | Rows | Added for Windows and Linux |
| --- | --- | --- |
| `FF / SPEED` | `FF`, `FF SPEED`, `GAME SPEED`, `REWIND`, `SPEED OPTIONS` (`FF MODE`, `FF MUSIC`) | |
| `SOUND` | `GAME VOLUME`, `MUTE GAME` | |
| `GAMEPLAY` | `WILD ENCOUNTERS`, `AUTO-RUN`, `RUN INDOORS`, `CHEATS` | `LEVEL CAP` |
| `SCREEN` | `ASPECT`, `AUTO ZOOM IN`, `HUD`, `DISPLAY OPTIONS`, `COMPANION VIEW`, `THEME`, `DARK MODE`, `FPS / CPU` | `COMPANION VIEW` adds `BATTLE WINGS`, `WINGS`, `GLASS DRAWER` and `STEP BACK` |
| `SCREEN` > `DISPLAY OPTIONS` | `SHADER`, `COLORS`, `DIM COMPANION`, `BACKGROUND`, `UI SIZE`, `WING ROOM`, `WING LOOK`, `STRETCH`, `REDUCE MOTION` | `FULLSCREEN`, `ORIENTATION`, `GAME TOUCH` |
| `COMPANION` | `BATTLE HINTS`, `ONE-TAP MOVES`, `TAB BAR` | |
| `CONTROLS` | `GAME BUTTONS`, `HOTKEYS`, `QUICK MENU` | `RUMBLE`; `GAME BUTTONS` > `BUTTON LAYOUT`; `HOTKEYS` sets which button opens the quick menu, the companion and the HUD cursor |
| `APP` | | `BATTERY SAVER`, `THIS GAME ONLY`, `RESUME GAMES`, `CHECK FOR UPDATES` |

Under the list are `CLOSE GAME` and `RESTART GAME`. Pages open with A and close
with B. A DS game's companion keeps its own `SETTINGS` (the gear tab), which has
`DS LAYOUT` and `DS WING SIDE`.

## Shaders and colours

Three rows in the companion's `SETTINGS` change how the game looks:

- **`SHADER`**: `OFF` draws the game's pixels as they are. `SHARP` keeps every
  pixel the same size and its edges crisp when the game doesn't fit the screen
  at a whole number scale. `xBRZ` smooths the edges of the pixel art, for a
  cleaner picture on a big, sharp screen.
- **`COLORS`**: `ORIGINAL` shows the game's colours as it sends them.
  `HANDHELD` gives the softer colours of the handheld's own screen. `OLED` takes
  a little glare off on an OLED screen, and the panels around the game go true
  black, which an OLED doesn't light at all.
- **`BRIGHTNESS`**: lifts the game's darker colours, 80% to 150%. `110%` is the
  default; `100%` is the game's own picture.
- **`UI SIZE`**: how large the menus, the HUD, the panels beside the game and
  the companion are drawn. `MEDIUM` is the default; `SMALL` fits more on the
  screen, and `LARGE` is the old, bigger size, easier to read from a distance.
  A DS game's companion follows it too.
- **`WING ROOM`**: how the width is shared between the game and the panels
  beside it. `AUTO` (the default) gives the panels the most room whenever the
  game still fills most of the screen's height (a Game Boy game, or a taller
  16:10 screen), and otherwise uses `BALANCED`. `BALANCED` keeps the game large
  with roomy panels; `GAME FIRST` makes the game as large as it goes, and `WINGS FIRST` gives the
  panels more room for a slightly smaller game. The game always keeps its shape.
  The DS layouts with panels beside the top screen (`WINGS`, `TOUCH IN WING`,
  `TOUCH PIECES`) follow it as well: `GAME FIRST` fills the height and
  `WINGS FIRST` takes a size off the top screen for wider panels.
- **`STRETCH`**: off by default. On, the game grows a little past its own shape
  (at most 5%) to fill the gaps round it.
- **`WING LOOK`**: how large the panels beside the game are drawn. `COMPACT`
  draws them a size smaller so more fits, `COMFY` (the default) as worked out
  for the window, and `BIG` a size larger, easier to read from the couch, where
  the window has room for it.
- **`DIM COMPANION`**: the companion and the panels beside the game are drawn
  a little darker, so the game stands out.
- **`BACKGROUND`**: `BLACK` makes everything round the game true black and
  draws the panels beside it as near black cards with a faint edge. `DARK` is
  the same a shade lighter. `THEME` keeps the theme's own colours. `BLUR`
  puts a soft, blurred copy of the game behind the panels, which turn a little
  see-through. With
  `COLORS` `OLED` it starts on `BLACK`, with `DARK MODE` on `DARK`, and in DS games it always starts
  on `BLACK`: most of the window is room round the screens there. Panels that stay still for a minute dim
  a little more, and move by a pixel every few minutes, so nothing burns in.
- **`REDUCE MOTION`**: the party cards beside the game slide in when your team
  changes, and their HP bars glide to the new value. A Pokemon that levels up
  gets a gold glow round its card, a new badge's dot pops, your money counts
  up or down to the new sum, and the bars fade from old words to new ones.
  Turn this on and all of it changes at once instead.
- **`BEZEL STYLE`** and **`BEZEL BARS`**: where the screen has room above and
  below the game (a 16:9 screen with the wings has just enough), a bar sits in
  each. Its text is sized to the bar, a step smaller when all three places wouldn't fit side by side. `BLACK` is near black with light text (the default with a `BLACK` or
  `DARK` background, so with `OLED` and `DARK MODE`), `GLOW` is a soft, faint
  glow of the game's own colours, `THEME` the theme's window. `BEZEL BARS`
  picks what each of the six places shows, or `NONE`: the place, the clock and
  battery, badges and play time, button hints (they fade when you leave the
  controls alone), fast forward, zoom and a `SAVED` mark, the keep-away spray's
  steps or your money, `NEXT BOSS AND CAP` (the next leader to beat and their
  top level), `CAUGHT HERE` (how many of this area's wild Pokemon you have),
  `LEAD POKEMON` (its HP and the EXP to its next level) and `LAST SAVED` (it
  warns you after half an hour without a save). The top bar starts on the
  place, the clock and badges, the bottom one on your lead, the next boss and
  your money. A place with nothing to say borrows something useful neither bar
  shows yet, so a bar is never half empty; `NONE` stays empty. In a battle the
  bottom bar shows the foe, how your moves do against it, and for a wild one
  how likely a ball is to work. Anything your game doesn't tell the app is left
  out.
- **Wings**: the party fills the left wing top to bottom, a card a Pokemon with
  its level, status, HP and a thin blue EXP bar to its next level (an outlined
  card with a ball for each place still free). A long name gets the room the
  level would take, so it's cut only when it's wider than the card. The right
  wing shows the map round you and, under it, your badges as eight dots, play
  time, the next boss, how many of this area's wild Pokemon you have, how many
  kinds you have caught and seen, and your money. Whatever the bars above and
  below the game already show is left out of the wing, so nothing is said
  twice. When something happens (a level up, a new area, a save, the spray
  wearing off), a short note shows at the top of the right wing for a couple
  of seconds and fades away.

DS games have the same rows in the companion's `SETTINGS` (the gear tab).

## Nintendo DS games

Nintendo DS games play on melonDS, in the same program and the same list as
your other games, so everything above still applies. Put your `.nds` files in
`DATA\roms` with the others, or start one straight away with
`pokedaisy.exe "D:\Games\My Game.nds"`. No BIOS or firmware files are needed.

A DS game's save sits beside the game's file, named after it without the
`.nds` (`My Game.sav`), the way most DS emulators keep it, so a folder you sync
between devices keeps working. The file is the cartridge's own save memory, so
a save from another DS emulator works if you copy it there under that name.
Each time a DS game starts its save is first copied to `DATA\backups`. DS save
states go to `DATA\states` and never hold the save.

`DS LAYOUT` picks where the two DS screens go in the window. Change it in the
DS companion's `SETTINGS`, in the game list's `SETTINGS`, or hold
SELECT (Backspace) and press RT (Space) for the next one or LT (Tab) for the one
before. A tap on R3 also steps to the next one. The picker shows each layout as a small picture of the window, marked
the same way as the companion views (`TOUCH IN WING`, `WINGS` and
`TOUCH PIECES` are the ones added here):

- **`ONE SCREEN`** (the first choice): the top screen fills the window. In
  Platinum (USA) the touch screen floats in over the dimmed game
  when the game wants it. Other DS games can't say when, so there you get
  `PICTURE IN PICTURE` instead.
- **`PICTURE IN PICTURE`**: the top screen fills the window and the touch
  screen sits small in its bottom right corner.
- **`TOUCH IN WING`**: the top screen in the middle at the largest whole size
  that leaves a wing on each side. The touch screen sits in one wing, level
  with the top screen's bottom, and the companion fills the other: your party
  down that wing, and your trainer's place, money, badges and dex above the
  touch screen. `DS WING SIDE` puts the touch screen on the `LEFT` or the
  `RIGHT` (the default). A window too narrow for that gets `PICTURE IN
  PICTURE`.
- **`STACKED`**: the top screen over the touch screen, like the DS itself.
- **`SIDE BY SIDE`**: the two screens next to each other. In a window taller
  than it is wide they go `STACKED`.
- **`LARGE + SMALL`**: a large top screen with a small touch screen beside it.
- **`WINGS`**: `ONE SCREEN` with the companion beside it. The top screen sits
  in the middle at a whole size, your party fills one wing and your trainer's
  facts the other (`DS WING SIDE` says which side the party is on). The touch
  screen floats in over the game when the game wants it, as in `ONE SCREEN`,
  or when you press R2. A window with no room for wings gets `ONE SCREEN`.
  The facts wing also shows `TIME`, the time of day the game goes by
  (`MORNING`, `DAY`, `NIGHT`, and `EVENING` in Platinum; the game's clock is
  your computer's). In HeartGold and SoulSilver the wings show your party and
  facts too, and once you carry the plant pots, `POT 1` to `POT 4` say what
  grows in each, how far along it is, the time left until its next step (or
  until ripe berries fall) and `DRY` when it needs water.
- **`TOUCH PIECES`**: the top screen in the middle and the touch screen taken
  apart, button by button, into the two wings. In HeartGold, SoulSilver and
  Platinum the battle's buttons each get a card of their own: FIGHT in one wing and the
  other three in the other; your moves, two to a wing; the bag's pockets; your
  party, three to a wing; with `BACK` or `CANCEL` as a button of its own under
  them. The buttons are drawn as large as both wings allow, so neither wing is
  crowded while the other sits empty. Tap a button as you would on the DS. A
  screen it doesn't know yet (and every other DS game) shows the touch screen
  cut down the middle instead, a half in each wing: a drag can cross from one
  half to the other, and `DS WING SIDE` set to `LEFT` swaps them. The
  companion's panels don't show here, since the wings hold the touch screen. A
  window too narrow for that gets `TOUCH IN WING`.

The companion's panels in the wings read Platinum (USA) and
Renegade Platinum for now; in other DS games they say so. While they show, the HUD leaves out its party row.

SELECT and R3 together (Space on a keyboard) swap the two screens: in `ONE SCREEN` you see the
touch screen alone, in `PICTURE IN PICTURE`, `LARGE + SMALL` and
`TOUCH IN WING` the touch screen becomes the large one, in `WINGS` it
floats in over the game, and in `TOUCH PIECES` it shows whole in the middle. `GUIDE / HOME` (Tab) opens the quick menu. To touch the touch
screen, click it with the mouse or tap it with your finger on a touch display,
wherever it shows; a drag works too. With a controller, hold SELECT, move the
pointer with the D-pad and press A to tap. On a keyboard the DS's X and Y are S
and A. Escape pauses the game.

With `ASPECT` on `EXPAND` (the default here), a DS game with a companion
(Platinum, HeartGold and SoulSilver, and the games built on them) shows the
world past the top screen's edges, so the whole window is game. The middle is
the game's own picture, untouched; the rest is the game's own 3D drawn again
around it. A battle's background reaches the edges too. LT and RT zoom out and
back in on the overworld, the same steps as a Game Boy Advance game, and the
view pulls back in for a battle, a menu and (with `AUTO ZOOM IN`) a text box.
With `ASPECT` on `ORIGINAL`, or in a DS game without a companion, LT and RT
make the game larger or smaller against the wings instead.

The DS companion fills the window and wears the same theme and `DARK MODE` as
the Game Boy Advance companion; `UI SIZE` sets how large its text is.

The DS companion has the same tabs as on a handheld: `PARTY` with each
member's summary, `BAG` with its pockets, `DEX` with each entry's page, `MAP`
with `PLACES`, `GUIDE`, and in a battle `BATTLE`, which shows the foe and how
well each of your moves does against it. L and R move between the tabs and the
gear at the end opens `SETTINGS`: your trainer facts, then the DS rows (`SCREEN`,
`DS LAYOUT` and the rest).

It reads Platinum (USA) in full. It goes by the game code inside the file
(`CPUE`, revision 0 or 1), so a trimmed or re-dumped copy works too, and so does
Renegade Platinum (read as the original game, so types, stats and the `GUIDE`
are the original game's). HeartGold and SoulSilver (USA) are read in full
too, with the party's icons in the HUD and the wings. For other DS games you get the game alone, and `SETTINGS` tells you
which game and revision it found, and which ones the companion reads.

## When something goes wrong

Started from a command prompt, the program prints what it's doing there.
Opened with a double click, it writes the same to `DATA\pokedaisy.log`. Look
there first, and include it if you report a problem.
