---
title: Desktop and Android compared
summary: What the Windows and Linux version has from the Android app, what it does its own way, and what it doesn't have yet.
section: help
order: 92
keywords: [android, desktop, windows, linux, parity, compare, comparison, differences, missing, not yet, same as android, feature list, what works]
---

The Windows and Linux version runs the same companion as the Android app, so
most of what you know from a phone or handheld is here. This page lists what
carries over, what works its own way on a PC, and what isn't in the desktop
version yet. It describes the latest desktop release; the list of changes is in
the fork's README.

## The same as on Android

- **The games**: Game Boy, Game Boy Color, GBA and Nintendo DS, the retail
  games and the ROM hacks the companion knows.
- **The companion's tabs**: `PARTY`, `BAG`, `MAP`, `GUIDE`, the Pokedex,
  `BATTLE` (with `INFO`, `SUGGESTIONS`, `ONE-TAP MOVES`, `FOE IVS`' `STATS`, `BATTLE INFO`, `BATTLE VIEW` and `BATTLE FOCUS`), the `TRAINER CARD`,
  `BOXES`, `STATES` and `SETTINGS`, with `TAB BAR` and `MORE`. A GBA game's
  Pokedex page shows each Pokemon's `EGG GROUP` beside its `ABILITY`, and on a
  big enough window its `HIDDEN` ability, `GENDER` ratio, `CATCH RATE` and stat
  `TOTAL`, as on Android.
- **The field**: `ASPECT` `EXPAND` with zoom, `AUTO ZOOM IN` and `LOOK AROUND`, the `HUD`, and
  the `QUICK MENU` with PC, FLY, the keep-away spray, `BIKE`, `WILD ENCOUNTERS`,
  states, fast forward and the companion.
- **Speed**: `FF`, `FF SPEED`, `GAME SPEED`, `REWIND`, `FF MODE`, and `SCENE SPEEDS`
  in Game Boy and GBA games.
- **Gameplay**: `WILD ENCOUNTERS`, `AUTO-RUN`, `RUN INDOORS`, `INSTANT TEXT`,
  `REUSABLE TMS`, `TRADE EVOS`, `SHINY ODDS`, `NUZLOCKE` (with its tab), `CHEATS`.
- **Look**: `THEME` and `DARK MODE`.
- **Saves**: a backup of your save each time a game starts, `RESUME GAMES`, and
  states that never hold the save.

## Done the desktop's way

- **One screen, many views**: instead of a second screen, `COMPANION VIEW`
  places the companion round the game (`BATTLE WINGS`, `WINDOW`, `SIDE PANEL`,
  `WINGS`, `GLASS DRAWER`, `STEP BACK`), and `DS LAYOUT` places a DS game's two
  screens.
- **Controls**: any controller, several at once, a keyboard and the mouse.
  `BUTTON LAYOUT` and `HOTKEYS` take the place of the touch controls, and the
  back paddles and extra buttons can each get a job.
- **Shaders**: `SHARP`, `xBRZ`, the `HANDHELD` colours (Android's `GBA COLORS`)
  and `OLED`, `FILTER` (`LCD`, `LCD PAPER`, `SCANLINES`, `CRT`, plus the
  desktop's `SUBPIXEL`) with `GRID`, `BRIGHTNESS`,
  and black backgrounds made for OLED screens.
- **Fast forward music**: `FF MUSIC`'s `STEADY` works in GBA games. Each song is
  recorded the first time the game plays it, rather than all at once in the
  background, and Game Boy and DS games play `SPED-UP` instead.
- **Save editing**: `EDIT SAVE` in a game's options changes money, the bag and
  your Pokemon in the GBA games and English HeartGold, SoulSilver and Platinum, with a
  backup first. Moving Pokemon between the party and the boxes and trading
  between two saves (TRADE SAVES) work too, in the GBA games.
- **Updates**: the program updates itself from this version's releases.

## Only on the desktop

`LEVEL CAP`, `RUMBLE`, `THIS GAME ONLY` (settings per game), `BATTERY SAVER`,
`EDIT SAVE`'s `PRESETS` and `PASTE SET` (battle sets filled into a Pokemon),
ten state slots with pictures, `ORIENTATION` for a screen mounted another way,
the DS wings (`TOUCH IN WING`, `WINGS`, `TOUCH PIECES`) with the DS time of day
and Berry Pots, and the `DEX` tab in Modern Emerald.

## Not in the desktop version yet

- RetroAchievements in DS games, the `ACHIEVEMENTS` tab and the leaderboards.
  Game Boy and Game Boy Advance games earn achievements, and the game menu
  lists them (see [Windows](windows.md#retroachievements)).
- The `NUZLOCKE` tab.
- The rest of the `MAKEOVER` group: `PLAYER LOOK`, `RIVAL LOOK`, `TEXT LOOK` and `FOLLOWER`. `DAY AND NIGHT` is in.
- `BATTLE BUTTONS` over the game (the desktop's `BATTLE WINGS` keep the battle
  beside it instead), `OVERLAY` frames and touch control skins.
- Feature tips and the app's other languages: the desktop version is in English.

If something you use on Android matters to you on a PC, ask for it on the
fork's issues page.
