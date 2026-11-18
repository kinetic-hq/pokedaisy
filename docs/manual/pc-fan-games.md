---
title: PC fan games
summary: Play PC fan games made with RPG Maker inside PokeDaisy on Windows, full screen, with your party and the battle in the wings.
section: start
order: 92
keywords: [pc, fan game, left stick, analog stick, thumbstick, button layout, alt tab, control hints, keyboard, language, english, spanish, idioma, fan games, rpg maker, rpg maker xp, essentials, mkxp, mkxp-z, game.exe, game.ini, restart, vsync, skips rows, cursor jumps, lag, input lag, latency, reborn, tectonic, fire ash, insurgence, phoenix rising, rejuvenation, desolation, awakening, bushido, empire, eon guardians, wing room, wide view, wide, zoom, camera, pan, xbrz, play time, fast forward, instant text, controller, full screen, pc game, windows]
---

On Windows, PokeDaisy plays PC fan games made with RPG Maker (most of them are
built on Pokemon Essentials) as well as cartridge games. The game fills the
screen at a sharp, even scale, and on most of them the wings beside it show
your party, where you are and the battle you're in, the way they do for a Game
Boy Advance game.

You play the game you already have: PokeDaisy doesn't change its folder or
your saves, and the game saves exactly where it always does.

## Add your games

Pick `GAME FOLDERS` in the bar at the top of the game list, then `ADD A FOLDER`,
and pick the folder that holds your fan games (each game in a folder of its
own, the way it unzipped). A folder above it works too: PokeDaisy looks up to
three folders down for a game. A game's folder is one with a `Game.ini` and the
game's own program in it.

Each fan game gets a tile with `PC`, `FAN GAME` and a tag:

- `PARTLY`: the game plays with the wings beside it (your party, where you are,
  the battle). The `BAG`, `DEX` and `GUIDE` tabs aren't there for fan games.
- `PLAY ONLY`: the game plays full screen through PokeDaisy, with no wings.

When the game's folder or title has a version in it (for example `19.5`), the
tile shows it. The cover is the game's own title picture until you pick another
with `SYNC BOX ART` or the game menu.

## How a game starts

PokeDaisy makes a folder for the game in `DATA\pc` and starts the game from
there with its own player program, the one that came with the game, set up to
draw at the size PokeDaisy picked and to tell the wings what's happening. The
game still reads its maps and pictures from its own folder, and saves where it
always does (its folder, `Saved Games` or `AppData`, whatever the game uses).
Nothing in the game's folder is written by PokeDaisy.

Press the controller's Guide button (or hold `SELECT` and `START` for a second)
for the game menu: `BACK TO THE GAME`, `CLOSE GAME`, and on games with wings,
`FAST FORWARD`, `INSTANT TEXT` and `WIDE VIEW`. `CLOSE GAME` asks the game to close the way
its own close button would; if it hasn't closed after a few seconds, you're
asked whether to wait or end it (unsaved play is lost).

## The screen

The game is drawn in the middle of the screen at a whole or near whole multiple
of its size, so its pixels stay even, and `WING ROOM` (in `SETTINGS`) decides how
much room the wings get:

- `AUTO` (the first choice): wings of about 240 to 320 pixels each, and a bar
  above and below the game. On a 1920 x 1200 screen the game is 1408 x 1056; on a
  1920 x 1080 screen it's 1280 x 960.
- `GAME FIRST`: the game as tall as the screen, the wings in what's left.
- `BALANCED`: the same as `AUTO`.
- `WINGS FIRST`: a smaller game and wider wings.

The bar above the game shows where you are, the clock and the battery (`BATTERY`
and its percent), and your badges, play time and money. The bar below shows how
many you've seen and caught, the zoom and the speed, and what's wild where
you're standing. While `CONTROL HINTS` are on, the bar below shows the buttons
instead, and the zoom moves up to the bar above when there's room.

A `PLAY ONLY` game is as tall as the screen, with nothing beside it.

A game runs at its own speed, whatever its `mkxp.json` says about waiting for the
screen: PokeDaisy turns that waiting off, since it slowed some games (Eon
Guardians) to a crawl in the window over PokeDaisy's.

## The wings

The left wing is your party: each one's name, types, level and HP. The right
wing shows where you are, a quick look at your badges, money and play time, and
the types your party is weak to. In a battle the right wing turns into the foe's
card: its name, level, types and HP, and what it's weak to.

The wings and the bars use the same text size as a Game Boy Advance game's
wings, and they never cover the game.

## Zoom, fast forward and instant text

On games with wings:

- `LT` shows more of the map around you (the camera zooms out), and `RT` shows
  less (it zooms in). The steps are 67%, 80%, 100%, 133% and 200%: how big
  things look next to the game's own size.
- The zoom is used on the map. A message, a menu or a battle shows at the
  game's own size, and your zoom comes back a second after it ends. Walking past
  things that start a short event doesn't change the zoom; only an event that
  goes on for a couple of seconds, like a scene, takes it off.
- The bar below the game shows the zoom you're seeing right now, so it reads
  100% in a menu or a battle. For a few seconds after you press `LT` or `RT` it
  shows the zoom even while `CONTROL HINTS` are on.
- If a game's own events keep taking the zoom off, PokeDaisy turns zoom off for
  the rest of that play so the picture stays still, and the bar says
  `ZOOM OFF`. Your zoom is tried again the next time you start the game.
- `L3` turns fast forward on and off.
- `INSTANT TEXT` in the game menu shows each message whole instead of letter by
  letter.
- `WIDE VIEW` in the game menu widens the game on the map to fill the room
  beside it: the wings get narrower and you see more of the map left and right.
  Menus and battles keep the game's own shape, in the middle of that room. The
  bar below the game says `WIDE` while it's on.

Your zoom, `INSTANT TEXT` and `WIDE VIEW` are kept for each game for next time. Fast forward
starts off. Most fan games have their own running shoes or auto run option in
their own options menu; use that one.

## Controllers

The controller goes to the game through PokeDaisy, so the D-pad, the left
stick, `A`, `B` and the rest work in every game with wings, whichever
controller your handheld lists first. PokeDaisy hands your buttons straight
to the game, the way a game reads its own controller: it never presses a
keyboard key for you, so nothing gets stuck down and the game never moves or
picks by itself. Your buttons work even when Windows keeps another window in
front of the game or the game's own key settings were changed. Hold the run
button to run, the same as on a keyboard. `START` opens the game's menu, and `SELECT`
uses your registered item in games that have one. The `GUIDE` button, `L3`,
`LT` and `RT` stay PokeDaisy's. The keyboard works as the game sets it up.
One press of the D-pad moves a menu's cursor one row; hold it to keep moving.
Your presses reach the game right away even though the game's window is in front of
PokeDaisy's.

In Indigo, Añil, Reminiscencia, Reborn and Rejuvenation, PokeDaisy hands your
buttons to the game where it reads its own keys, so its menus move just as they
do on a keyboard. A quick press moves the cursor once, and holding the D-pad
keeps it moving at the game's usual pace, however busy the game is.

When a game asks you to type a name (yours, a creature's or a box's) and you
play with a controller, the name screen opens as the game's own letter grid
instead of waiting for a keyboard. Move with the D-pad, press the confirm
button to add the letter under the cursor, press the back button to rub out
the last one, and pick `OK` when you're done. In Reborn and Rejuvenation the grid has a cell that switches
between letters and symbols, and one that adds a space. If you haven't pressed
a controller button yet in this play, the game keeps its keyboard screen, so
you can still type on a keyboard. The game's own text entry option stays as
you set it.

Some games ask to restart after you change one of their options (a `VSync`
option, for example). Pick yes: the game closes and PokeDaisy starts it again
for you, with its wings and your controller.

The bar under the picture shows your controller's buttons and what each one
does in the game you're playing: confirm, back, the menu, running, and the
game's other keys. Turn `CONTROL HINTS` off in `SETTINGS`, `CONTROLS` to show
the dex and the wild ones there instead.

`BUTTON LAYOUT` in `SETTINGS` decides which face button confirms, the same as
in your cartridge games: on `XBOX` (the first choice) the bottom button
confirms and the right one goes back; on `NINTENDO` the right one confirms and
the bottom one goes back. The `F1` key binding window some games' players
have stays shut, since only a mouse and a keyboard could close it.

If you switch to another window and come back (`Alt` + `Tab`, the Windows key,
your handheld's side app, sleep), the game goes back where it was, full screen
with its wings, and gets the controller and keyboard back. If your screen's
resolution changed meanwhile, the game and the wings are laid out for the new
one.

## Languages

Some fan games come in more than one language, and a few list another
language before English, so a first start would pick that one. When a game has
English text, PokeDaisy starts it in English the first time: its language
choice, if it asks, starts on `English`, and a new adventure's options start on
English. You can still pick another language in the game, and your save keeps
what you picked. The game's files aren't changed.

## Games made for the original RPG Maker XP player

A few older games (Insurgence is one) were made for the original RPG Maker XP
player and come without the newer player the wings need. When another fan game
in the same folder has a newer player built for those older games, PokeDaisy
starts the older game with that player instead, and the older game gets the
wings too. The game's extras that only the original player can do are left out:
its special tilted maps are drawn flat, and its own animated picture and
screenshot extras are off. Its saves are the same files the original player
writes, so you can go back and forth.

## Games on their own player

A game with its own player (Phoenix Rising is one) plays `PLAY ONLY`. PokeDaisy
leaves its window the size the game makes it and draws the game's picture as
tall as the screen, whole, through your `SHADER` and `COLORS`.

- `RT` zooms in a step at a time (125%, 150%, 175%, 200%) and `LT` zooms back
  out; each step glides rather than jumps. While you're zoomed in, the right
  stick moves the view around the picture. Back at 100% the whole picture
  shows again.
- With `SHADER` on `xBRZ`, a game that draws its art at half size and doubles it
  (Phoenix Rising does) is smoothed from its real pixels, so edges come out
  round instead of blocky.
- The room beside the picture shows the clock and the battery on the left, and
  on the right the play time PokeDaisy has counted for the game and the zoom.
  The play time is kept for each game and goes on from where it was next time.

Phoenix Rising reads an Xbox style controller (most handheld PCs' built in
controls are one) by itself, so the D-pad and buttons go straight to the game,
by the buttons' places whatever `BUTTON LAYOUT` says. The game doesn't read the
left stick, so PokeDaisy turns the stick into the arrow keys for it: push the
stick and you walk or move through menus as with the D-pad, one direction at a
time. Let the stick go back to the middle and you stop. The game keeps
the keyboard and the mouse too. If the game stops with a script error while it's
starting, PokeDaisy starts it again for you.

## Tested games

| Game | Tag | Notes |
| --- | --- | --- |
| Reborn | `PARTLY` | Party, place, battle with the foe's card |
| Tectonic | `PARTLY` | Party, place, dex counts |
| Fire Ash | `PARTLY` | Party, place, battle, zoom |
| Eon Guardians | `PARTLY` | Party, place, battle |
| Awakening | `PARTLY` | Party, place; controller works |
| Bushido | `PARTLY` | Party, place |
| Desolation | `PARTLY` | Party, place |
| Empire | `PARTLY` | Party, place |
| Insurgence | `PARTLY` | Runs on another game's newer player (above); party, place, zoom, `WIDE VIEW` |
| Indigo | `PARTLY` | Party, place; starts in English |
| Opalo | `PARTLY` | Party, place; starts in English |
| Realidea | `PARTLY` | Party, place; starts in English |
| Africanvs | `PARTLY` | Party, place; starts in English |
| Reminiscencia | `PARTLY` | Party, place; starts in English |
| Rejuvenation | `PARTLY` | Party, place |
| Floral Tempest | `PARTLY` | Party, place |
| Itinerant | `PARTLY` | Party, place |
| Phoenix Rising | `PLAY ONLY` | Its own player, drawn whole full screen; xBRZ from its real pixels, the left stick moves you, zoom with the right stick, clock and play time beside it |

A game in a folder whose name has an accented letter in it plays too: PokeDaisy gives the game a plain path to its folder (a Windows junction
in `DATA\pc\links`) without moving anything.

## When something goes wrong

- **The game isn't in the list.** Check that its folder has a `Game.ini` and
  the game's program, and that it's no more than three folders below the folder
  you added.
- **The wings are empty.** The game hasn't started your adventure yet; they fill
  in once you're playing.
- **A game lags or its cursor skips rows with the controller.** Play it for a
  minute, close it, and send `pokedaisy.log` from PokeDaisy's folder. Its
  `PC companion: stats` lines say how fast the game ran and how it read your
  buttons.
- **The game asks to close and doesn't.** Pick `END IT` when PokeDaisy asks; you
  lose anything you haven't saved.
