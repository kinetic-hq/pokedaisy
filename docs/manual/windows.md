---
title: Windows handhelds and PCs
summary: Run the Linux host's PokeDaisy on a Windows handheld or PC from a portable zip, with your own games and saves.
section: start
order: 91
keywords: [windows, pc, update, updates, check for updates, new version, rog ally, legion go, onexplayer, steam deck, portable, zip, exe, sdl, controller, fullscreen, companion view, battle wings, side panel, glass drawer, step back, nds, ds, nintendo ds, melonds, touch screen, mouse, ds layout, one screen, picture in picture, touch in wing, stacked, side by side, large + small, shader, shaders, xbrz, sharp, colors, colours, handheld, oled, dim companion, black panels, game folders, roms folders, roms-folders.txt, other folders, emulation folder, retroarch, saves beside games, save next to game, sav]
---

The Windows version is the [Linux host](linux.md) built for Windows: the same
emulator, quick menu, HUD and companion, on a Windows handheld (an ROG Ally, a
Legion Go, a OneXPlayer) or any PC. It plays Game Boy, Game Boy Color and Game
Boy Advance games you own, and Nintendo DS games (below), all from one download. It is early, like the Linux version, so keep a copy
of your saves.

## Install it

There is no installer. Download `pokedaisy-windows-x86_64.zip` from the
[Releases page](https://github.com/kinetic-hq/pokedaisy/releases/latest), unzip it anywhere you can write to (your Documents folder is
fine, `Program Files` isn't), and you get:

- `pokedaisy.exe` and `SDL2.dll`, which must stay side by side
- a `DATA` folder with `roms`, `saves` and `states` in it
- the licenses and this program's source record

Put your `.gba`, `.gbc`, `.gb` and `.nds` files in `DATA\roms`, then open
`pokedaisy.exe`. It lists your games; pick one with the D-pad and press A.
Everything the program writes stays in that `DATA` folder, so you can move the
whole folder to another PC or a memory card.

## Games in other folders

Your games can stay where they are, in a folder another emulator or a sync app
already uses. Pick `GAME FOLDERS` at the bottom of the game list, then
`ADD A FOLDER`: you see your drives, `A` opens a folder, `B` goes back up, and
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

## Full screen and controllers

To fill the screen, start it with `POCKET_FULLSCREEN` set to `1`. From a command
prompt in the folder, type `set POCKET_FULLSCREEN=1`, then `pokedaisy.exe`.

A frontend or a shortcut can set it the same way. Without it, the program opens
a window you can resize. `POCKET_WINDOW=1920x1080` opens the window at a size
you choose, to try a handheld's layout on a desktop.

Built in handheld controls and Xbox style controllers work as they come. A
controller can be plugged in, switched on or pulled out while you play, and on a
handheld its own controls and an Xbox controller both work at once. For a controller it
doesn't know, put a line for it in `DATA\gamecontrollerdb.txt` (the community
file's format). On a keyboard: the arrow keys, Z for A, X for B, Enter for
START, Backspace for SELECT, Q and E for L and R, Tab for the companion, F for
the quick menu, V for the next companion view, K to lock the side panel and
Escape for the game menu.

To start a game straight away, for a frontend or a shortcut, give its file:
`pokedaisy.exe "D:\Games\My Game.gba"`. `--data FOLDER` uses another data
folder than the one beside the program.

## Companion views

On a wide screen the companion doesn't have to cover the game. Pick how it
shares the screen in the companion's `SETTINGS` > `COMPANION VIEW`, or press the
right stick (V on a keyboard) to step to the next one as you play. The game
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
- **`DIM COMPANION`**: the companion and the panels beside the game are drawn
  a little darker, so the game stands out.

DS games have the same three rows in the companion's `SET` tab.

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
companion's `SETTINGS`, or hold SELECT (Backspace) and press R2 (Space) for the
next one or L2 (Tab) for the one before:

- **`ONE SCREEN`** (the first choice): the top screen fills the window. In
  Platinum (USA, revision 1) the touch screen floats in over the dimmed game
  when the game wants it. Other DS games can't say when, so there you get
  `PICTURE IN PICTURE` instead.
- **`PICTURE IN PICTURE`**: the top screen fills the window and the touch
  screen sits small in its bottom right corner.
- **`TOUCH IN WING`**: the top screen as tall as the window, and the touch
  screen in the room beside it. A window too narrow for that gets `PICTURE IN
  PICTURE`.
- **`STACKED`**: the top screen over the touch screen, like the DS itself.
- **`SIDE BY SIDE`**: the two screens next to each other. In a window taller
  than it is wide they go `STACKED`.
- **`LARGE + SMALL`**: a large top screen with a small touch screen beside it.

R2 (Space on a keyboard) swaps the two screens: in `ONE SCREEN` you see the
touch screen alone, and in `PICTURE IN PICTURE` and `LARGE + SMALL` the touch
screen becomes the large one. L2 (Tab) opens the quick menu. To touch the touch
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
