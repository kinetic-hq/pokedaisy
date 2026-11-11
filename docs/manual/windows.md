---
title: Windows handhelds and PCs
summary: Run the Linux host's PokeDaisy on a Windows handheld or PC from a portable zip, with your own games and saves.
section: start
order: 91
keywords: [windows, pc, rog ally, legion go, onexplayer, steam deck, portable, zip, exe, sdl, controller, fullscreen]
---

The Windows version is the [Linux host](linux.md) built for Windows: the same
emulator, quick menu, HUD and companion, on a Windows handheld (an ROG Ally, a
Legion Go, a OneXPlayer) or any PC. It plays Game Boy, Game Boy Color and Game
Boy Advance games you own. It is early, like the Linux version, so keep a copy
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
the quick menu and Escape for the game menu.

To start a game straight away, for a frontend or a shortcut, give its file:
`pokedaisy.exe "D:\Games\My Game.gba"`. `--data FOLDER` uses another data
folder than the one beside the program.

## When something goes wrong

Started from a command prompt, the program prints what it's doing there.
Opened with a double click, it writes the same to `DATA\pokedaisy.log`. Look
there first, and include it if you report a problem.
