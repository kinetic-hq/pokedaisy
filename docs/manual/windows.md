---
title: Windows handhelds and PCs
summary: Run the Linux host's PokeDaisy on a Windows handheld or PC from a portable zip, with your own games and saves.
section: start
order: 91
keywords: [windows, pc, rog ally, legion go, onexplayer, steam deck, portable, zip, exe, sdl, controller, fullscreen, companion view, battle wings, side panel, glass drawer, step back, nds, ds, nintendo ds, melonds, touch screen, mouse]
---

The Windows version is the [Linux host](linux.md) built for Windows: the same
emulator, quick menu, HUD and companion, on a Windows handheld (an ROG Ally, a
Legion Go, a OneXPlayer) or any PC. It plays Game Boy, Game Boy Color and Game
Boy Advance games you own, and Nintendo DS games in the DS build (below). It is early, like the Linux version, so keep a copy
of your saves.

## Install it

There is no installer. Download the `pokedaisy-windows-x86_64.zip` from a build
of the repository, unzip it anywhere you can write to (your Documents folder is
fine, `Program Files` isn't), and you get:

- `pokedaisy.exe` and `SDL2.dll`, which must stay side by side
- a `DATA` folder with `roms`, `saves` and `states` in it
- the licenses and this program's source record

Put your `.gba`, `.gbc` and `.gb` files in `DATA\roms`, then open
`pokedaisy.exe`. It lists your games; pick one with the D-pad and press A.
Everything the program writes stays in that `DATA` folder, so you can move the
whole folder to another PC or a memory card.

## Saves

Your in-game saves go to `DATA\saves`, named after the game's file
(`My Game.gba.sav`). To bring a save from another emulator, copy a backup of it
there under that name before you start the game. Each time a game starts, the
save is first copied to `DATA\saves\pokedaisy-backups`, and the newest ten
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
controller can be plugged in or pulled out while you play. For a controller it
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

## Nintendo DS games

The `pokedaisy-windows-x86_64-ds.zip` build also plays Nintendo DS games, on
melonDS. It is the same program with DS games added, so everything above still
applies. Put your `.nds` files in `DATA\roms` with the others, or start one
straight away with `pokedaisy.exe "D:\Games\My Game.nds"`. No BIOS or firmware
files are needed.

A DS game's save sits beside the game's file, named after it without the
`.nds` (`My Game.sav`), the way most DS emulators keep it, so a folder you sync
between devices keeps working. The file is the cartridge's own save memory, so
a save from another DS emulator works if you copy it there under that name.
Each time a DS game starts its save is first copied to `DATA\backups`. DS save
states go to `DATA\states` and never hold the save.

One DS screen shows at a time. R2 (Space on a keyboard) switches between the
top and the touch screen, and L2 (Tab) opens the quick menu. To touch the touch
screen, show it and then click it with the mouse or tap it with your finger on
a touch display; a drag works too. With a controller, hold SELECT, move the
pointer with the D-pad and press A to tap. On a keyboard the DS's X and Y are S
and A. Escape pauses the game.

The companion reads one DS game so far, Platinum (USA, revision 1): its party,
bag, dex and Town Map. For other DS games you get the game alone.

## When something goes wrong

Started from a command prompt, the program prints what it's doing there.
Opened with a double click, it writes the same to `DATA\pokedaisy.log`. Look
there first, and include it if you report a problem.
