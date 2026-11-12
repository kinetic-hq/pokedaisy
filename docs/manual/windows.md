---
title: Windows handhelds and PCs
summary: Run the Linux host's PokeDaisy on a Windows handheld or PC from a portable zip, with your own games and saves.
section: start
order: 91
keywords: [windows, pc, update, updates, check for updates, new version, rog ally, legion go, onexplayer, steam deck, portable, zip, exe, sdl, controller, fullscreen, full screen, f11, alt+enter, borderless, windowed, button layout, xbox, nintendo, a and b swapped, hotkeys, paddles, back buttons, guide, home button, share, screenshot, controls, touch, touch screen, mouse, tap, game touch, orientation, portrait, landscape, rotate, rotation, flipped, upside down, vertical screen, companion view, battle wings, side panel, glass drawer, step back, nds, ds, nintendo ds, melonds, touch screen, mouse, ds layout, one screen, picture in picture, touch in wing, stacked, side by side, large + small, shader, shaders, xbrz, sharp, colors, colours, handheld, oled, dim companion, black panels, brightness, bezel, bezel bars, bezel style, glow, zoom in, zoom out, remap, r3, view, game folders, roms folders, roms-folders.txt, other folders, emulation folder, retroarch, saves beside games, save next to game, sav, time of day, night, berries, plant pots, heartgold, soulsilver]
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

Your games open as a grid of box art. Each game shows its name, the system
(`GBA`, `GBC`, `GB` or `NDS`), `ORIGINAL` or `ROM HACK`, and its version when
the file name has one. Move with the D-pad, the mouse or a finger; `A` or a tap
plays, the mouse wheel or a drag scrolls, and `Y` opens `SETTINGS`. `GAME FOLDERS`
and `SETTINGS` sit in the bar along the top: press UP from the first row to
reach them.

Box art is looked for by itself in the background (`SETTINGS` > `LIBRARY` >
`BOX ART DOWNLOAD`, on): first the libretro thumbnails, which have the original
games, then SteamGridDB when you add your own key, which has many ROM hacks. To
add one, make a free key on steamgriddb.com (Preferences, API), copy it, then
pick `SETTINGS` > `LIBRARY` > `STEAMGRIDDB KEY` > `PASTE THE KEY FROM THE CLIPBOARD`.
Box art is kept in `DATA\boxart`.

Press `X`, right click or hold a finger on a game for its options:

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
layout, and English HeartGold and SoulSilver. Any other game, or a save that
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
(with `ASPECT` `EXPAND`) and L3 turns fast forward on and off; in a DS game L2
and R2 stay the touch screen and the DS layout. Pick one and choose
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
  types and how each of your moves does against it). On a 16:10 screen a strip
  above the game shows where you are, the time and your badges, and one below
  it the buttons and the steps left on the spray.
- **`WINDOW`**: the companion as a window in the middle, over the dimmed game.
- **`SIDE PANEL`**: the companion slides in from the right. Press the left stick
  (K on a keyboard) while it's open to lock it: the game moves into the left
  half and the panel keeps the right half.
- **`WINGS`**: the battle layout all the time, with the map on the right when
  you're not in a battle.
- **`GLASS DRAWER`**: a see-through sheet over the bottom of the screen.
- **`STEP BACK`**: the game steps back two sizes and the companion sits around
  it.

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
| `SCREEN` > `DISPLAY OPTIONS` | `SHADER`, `COLORS`, `DIM COMPANION`, `BACKGROUND`, `REDUCE MOTION` | `FULLSCREEN`, `ORIENTATION`, `GAME TOUCH` |
| `COMPANION` | `BATTLE HINTS`, `ONE-TAP MOVES`, `TAB BAR` | |
| `CONTROLS` | `GAME BUTTONS`, `HOTKEYS`, `QUICK MENU` | `RUMBLE`; `GAME BUTTONS` > `BUTTON LAYOUT`; `HOTKEYS` sets which button opens the quick menu, the companion and the HUD cursor |
| `APP` | | `BATTERY SAVER`, `THIS GAME ONLY`, `RESUME GAMES`, `CHECK FOR UPDATES` |

Under the list are `CLOSE GAME` and `RESTART GAME`. Pages open with A and close
with B. DS games keep their own `SET` tab, which has `DS LAYOUT` and
`DS WING SIDE`.

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
- **`DIM COMPANION`**: the companion and the panels beside the game are drawn
  a little darker, so the game stands out.
- **`BACKGROUND`**: `BLACK` makes everything round the game true black and
  draws the panels beside it as near black cards with a faint edge. `DARK` is
  the same a shade lighter. `THEME` keeps the theme's own colours. With
  `COLORS` `OLED` it starts on `BLACK`, with `DARK MODE` on `DARK`, and in DS games it always starts
  on `BLACK`: most of the window is room round the screens there. Panels that stay still for a minute dim
  a little more, and move by a pixel every few minutes, so nothing burns in.
- **`REDUCE MOTION`**: the party cards beside the game slide in when your team
  changes, and their HP bars glide to the new value. Turn this on and they
  change at once instead.
- **`BEZEL STYLE`** and **`BEZEL BARS`**: where the screen has room above and
  below the game, a bar sits in each. `BLACK` is near black with light text (the
  default with a `BLACK` background), `GLOW` is a soft, faint glow of the game's
  own colours, `THEME` the theme's window. `BEZEL BARS` picks what each of the
  six places shows, or `NONE`: the place, the clock and battery, badges and play
  time, button hints (they fade when you leave the controls alone), fast
  forward, zoom and a `SAVED` mark, and the keep-away spray's steps or your
  money. In a battle the bottom bar shows the foe, how your moves do against
  it, and for a wild one how likely a ball is to work. Anything your game
  doesn't tell the app is left out.
- **Wings**: the party fills the left wing top to bottom, a card a Pokemon with
  its level, status and HP (an empty card for each place still free); the right wing shows the map round you and, under
  it, your badges, play time, how many kinds you have caught and seen, and your
  money.

DS games have the same rows in the companion's `SET` tab.

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
companion's `SET` tab, in the game list's `SETTINGS`, or hold
SELECT (Backspace) and press R2 (Space) for the next one or L2 (Tab) for the one
before. The picker shows each layout as a small picture of the window, marked
the same way as the companion views (`TOUCH IN WING`, `WINGS` and
`TOUCH PIECES` are the ones added here):

- **`ONE SCREEN`** (the first choice): the top screen fills the window. In
  Platinum (USA, revision 1) the touch screen floats in over the dimmed game
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
  apart, button by button, into the two wings. In HeartGold and SoulSilver the
  battle's buttons each get a card of their own: FIGHT in one wing and the
  other three in the other; your moves, two to a wing; the bag's pockets; your
  party, three to a wing; with `BACK` or `CANCEL` as a button of its own under
  them. The buttons are drawn as large as both wings allow, so neither wing is
  crowded while the other sits empty. Tap a button as you would on the DS. A
  screen it doesn't know yet (and every other DS game) shows the touch screen
  cut down the middle instead, a half in each wing: a drag can cross from one
  half to the other, and `DS WING SIDE` set to `LEFT` swaps them. The
  companion's panels don't show here, since the wings hold the touch screen. A
  window too narrow for that gets `TOUCH IN WING`.

The companion's panels in the wings read Platinum (USA, revision 1) for now; in
other DS games they say so. While they show, the HUD leaves out its party row.

R2 (Space on a keyboard) swaps the two screens: in `ONE SCREEN` you see the
touch screen alone, in `PICTURE IN PICTURE`, `LARGE + SMALL` and
`TOUCH IN WING` the touch screen becomes the large one, in `WINGS` it
floats in over the game, and in `TOUCH PIECES` it shows whole in the middle. L2 (Tab) opens the quick menu. To touch the touch
screen, click it with the mouse or tap it with your finger on a touch display,
wherever it shows; a drag works too. With a controller, hold SELECT, move the
pointer with the D-pad and press A to tap. On a keyboard the DS's X and Y are S
and A. Escape pauses the game.

The companion reads one DS game so far, Platinum (USA, revision 1): its party,
bag, dex and Town Map. For other DS games you get the game alone.

## When something goes wrong

Started from a command prompt, the program prints what it's doing there.
Opened with a double click, it writes the same to `DATA\pokedaisy.log`. Look
there first, and include it if you report a problem.
