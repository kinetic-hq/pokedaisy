---
title: Desktop game support
summary: Game by game, what the Windows and Linux companion reads for each game on the site's games table, and what it still misses next to the Android app.
section: help
order: 93
keywords: [desktop, windows, linux, pc, games table, support, supported, which games, compatibility, parity, missing, gap, not yet, party, bag, battle, map, dex, pokedex, guide, nds, ds, black, white, heartgold, soulsilver, platinum, renegade platinum, origin heartgold, modern emerald, dragonsden]
---

Every game on the site's games table plays on the Windows and Linux version. This
page says how much of the companion each one gets there: the six tabs the games
table lists, game by game, next to what the Android app does with the same game.
Use it to check your game before you move it to a PC. [Desktop and Android
compared](android-parity.md) covers the rest of the program (settings, controls,
link play), and [Supported games](supported-games.md) explains how the companion
recognises a ROM.

The aim is for every game here to reach `FULL`, so a game marked `PARTLY` or
`PLAY ONLY` is still being worked on. The games the maintainer plays come first.

## How to read it

Each row is one row of the games table, with the versions it covers. The desktop
knows a game by the same thing the app does: its game code and revision for the
original Game Boy Advance carts, and its SHA-1 for Game Boy games and ROM hacks
(the first eight characters are shown). A dump that isn't one of these plays,
but the companion shows a notice instead of your party.

- **works**: the tab is there and does what it does on Android, in every version
  in the row.
- **partly**: the tab is there with less in it, or only some versions in the row
  get it. The gap column says which.
- **no**: the tab isn't there on the desktop.

`BATTLE` reads `partly` on the desktop wherever it reads `partly` on Android: the
battle panes work but haven't been checked in every kind of battle. A missing
foe team is listed as a gap even then.

## The count

Of the 72 rows:

- **65** get the companion at the same level as on Android, every tab
  included. (Only 2 of them read `works` in all six columns; the rest have a
  `partly` the Android app has too.)
- **7** get a companion that falls short of Android's somewhere.
- **None** is left with no companion.

By tab:

| Area | works | partly | no |
| --- | --- | --- | --- |
| `PARTY` | 72 | 0 | 0 |
| `BAG` | 72 | 0 | 0 |
| `BATTLE` | 6 | 66 | 0 |
| `MAP` | 71 | 1 | 0 |
| `DEX` | 65 | 1 | 6 |
| `GUIDE` | 37 | 28 | 7 |

## Game by game

| Game | Versions / builds | PARTY | BAG | BATTLE | MAP | DEX | GUIDE | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Red / Blue (GB) | USA/Europe; SHA-1 ea9bcae6, d7037c83 | works | works | partly | works | works | works | None. Battle panes as on Android (not checked in every battle) |
| PureRed / PureBlue (GB) | 2.7.6; SHA-1 2e94d09c, d419fe24 | works | works | partly | works | works | no | None (no guide on Android either) |
| Yellow (GB) | USA/Europe; SHA-1 cc7d0326 | works | works | partly | works | works | works | None |
| Yellow (GB) | German, French, Italian, Spanish; SHA-1 42f3714e, 0aceec0e, 05bb8e99, 1dc24203 | works | works | partly | works | works | works | None |
| Yellow Legacy (GB) | 1.0.10; SHA-1 2ac87de4 | works | works | partly | works | works | works | None |
| Gold / Silver (GBC) | USA/Europe; SHA-1 d8b8a360, 49b163f7 | works | works | partly | works | works | no | None (no guide on Android either) |
| Crystal (GBC) | USA, USA/Europe rev 1, Australia; SHA-1 f4cd194b, f2f52230, a0fc810f | works | works | partly | works | works | works | None |
| Crystal Legacy (GBC) | 1.3.1; SHA-1 74a6727e | works | works | partly | works | works | works | None |
| Crystal Clear (GBC) | 2.5.11 and 2.6.0, each with a Color Filter build (4 builds) | works | works | partly | works | works | no | None (no guide on Android either) |
| Prism (GBC) | 0.95 build 254 Hotfix 5; SHA-1 75207669 | works | works | partly | works | works | no | None (no guide on Android either) |
| Polished Crystal (GBC) | 3.2.0 to 3.2.3, normal and Faithful (16 builds) | works | works | partly | works | works | works | None |
| FireRed | USA/Europe: BPRE rev 0 (SHA-1 41cb23d8) and rev 1 (dd5945db) | works | works | partly | works | works | works | None |
| FireRed | Spanish, German, French, Italian rev 0 (BPRS, BPRD, BPRF, BPRI); Japanese rev 0 and 1 (BPRJ) | works | works | partly | works | works | partly | No guide for the Japanese cart; no foe team in battle in the German, French, Italian and Japanese carts |
| LeafGreen | USA/Europe: BPGE rev 0 (SHA-1 574fa542) and rev 1 (7862c67b) | works | works | partly | works | works | works | None |
| LeafGreen | Spanish, German, French, Italian rev 0 (BPGS, BPGD, BPGF, BPGI); Japanese rev 0 and 1 (BPGJ) | works | works | partly | works | works | partly | No guide for the Japanese cart; no foe team in battle in the German, French, Italian and Japanese carts |
| Emerald | USA/Europe: BPEE (SHA-1 f3ae0881, and the common collection dump) | works | works | works | works | works | works | None |
| Emerald | Spanish, German, French, Italian, Japanese (BPES, BPED, BPEF, BPEI, BPEJ) | works | works | works | works | works | partly | No guide for the Japanese cart |
| FireRed / Emerald, randomized | Universal Pokemon Randomizer ZX 4.6.1 on USA/Europe FireRed or Emerald, any seed | works | works | works | works | works | partly | None (the guide is partly on Android too) |
| Ruby | USA: AXVE rev 0, 1 and 2 | works | works | partly | works | works | works | None |
| Ruby | Spanish, German, French, Italian, Japanese rev 0 and 1 (AXVS, AXVD, AXVF, AXVI, AXVJ) | works | works | partly | works | works | partly | No guide for the Japanese cart |
| Sapphire | USA: AXPE rev 0, 1 and 2 | works | works | partly | works | works | works | None |
| Sapphire | Spanish, German, French, Italian, Japanese rev 0 and 1 (AXPS, AXPD, AXPF, AXPI, AXPJ) | works | works | partly | works | works | partly | No guide for the Japanese cart |
| Platinum (DS) | USA rev 0 and 1 (CPUE; rev 1 SHA-1 0862ec35); French and Spanish rev 0 (CPUF, CPUS) | works | works | works | works | works | works | USA carts: the full companion, as on Android; French and Spanish carts get nothing |
| HeartGold (DS) | USA (IPKE rev 0, SHA-1 4fcded0e); Europe and every other language | works | works | partly | works | works | works | USA cart only: the full companion, as on Android |
| SoulSilver (DS) | USA (IPGE rev 0, SHA-1 f8dc38ea); Europe and every other language | works | works | partly | works | works | works | Same as HeartGold |
| Black (DS) | USA/Europe (IRBO, SHA-1 26ad0b99) and every language | works | works | partly | works | works | works | USA / Europe cart only: the full companion, as on Android |
| White (DS) | USA/Europe (IRAO, SHA-1 bc696a0d) and every language | works | works | partly | works | works | works | Same as Black |
| Black 2 (DS) | USA/Europe (IREO, SHA-1 e51e6dfb) and every language | works | works | partly | works | works | works | Same as Black |
| White 2 (DS) | USA/Europe (IRDO, SHA-1 b5d7490b) and every language | works | works | partly | works | works | works | Same as Black |
| Origin HeartGold (DS) | 4.0.3 English 1.0.0 (IPKJ rev 0) | works | works | partly | works | works | partly | Its guide has `HERE` (with its items) and `NEXT BOSS` only, read from its cart (no guide on Android) |
| Renegade Platinum (DS) | 1.2.0, 1.3.0 and 1.3.0 Classic (CPUE rev 1) | works | works | works | works | works | partly | None. Its guide has `HERE` (with its items) and `NEXT BOSS`, read from its cart (no guide on Android) |
| Unbound | 2.1.1.1; SHA-1 b4776b82 | works | works | partly | works | works | works | None |
| Unbound | 2.1.1.1 French; SHA-1 0ce2a880 | works | works | partly | works | works | works | None |
| Radical Red | 4.1; SHA-1 964f951a | works | works | partly | works | works | works | None |
| Gaia | 3.2; SHA-1 d5b1e779 | works | works | partly | works | works | works | None |
| Odyssey | 4.1.1; SHA-1 8745ddbd | works | works | partly | works | works | works | None |
| Heart and Soul | 2.0.6; SHA-1 79ee6df0 | works | works | partly | works | works | works | None |
| Amethyst | 1.3.0 (SHA-1 00e70c03), 1.4.1 (91291aad), 1.4.3 (411e9023) | works | works | partly | works | works | works | None |
| Celia's Stupid Romhack | 1.1.4; SHA-1 3cbd5a2e | works | works | partly | works | works | works | None |
| Orange Islands | Beta 5.7 and an older beta; SHA-1 8bac897d, d2e3800e | works | works | partly | works | works | works | None |
| Ultra Violet | 1.22; SHA-1 3b6c51d8 | works | works | partly | works | works | works | None |
| FireRed Rocket Edition | The one release; SHA-1 033d0537 | works | works | partly | works | works | works | None |
| Ash Gray | 4.5.3 (SHA-1 1ee1557d), Fork Bug Fix 1.0 (92ed8214) | works | works | partly | works | works | works | None |
| Ghost Grey | 1.0.1, 1.0.2, 1.0.4; SHA-1 20dfaea1, 74337272, 0dafc011 | works | works | partly | works | works | works | None |
| Let's Go | English, both versions, each with a Translation Only build (4 builds) | works | works | partly | works | works | works | None (its `DEX` reads the app's own tables for it; not yet run on a PC) |
| Sword and Shield Ultimate Plus | 1.2.1.2 and its 3 patches (4 builds) | works | works | partly | works | works | works | None |
| GS Chronicles | 2.7.6; SHA-1 25f8840e | works | works | partly | works | works | works | None |
| Recharged Yellow | 1.8.0, 1.9.0, 1.9.4, 1.9.7 | works | works | partly | works | works | works | None |
| Emerald Legacy | 1.1.4; SHA-1 cbd71af6 | works | works | partly | works | works | works | None |
| Lazarus | 2.0; SHA-1 7dcdc7e2 | works | works | partly | works | works | partly | None |
| Emerald Seaglass | 3.0; SHA-1 b9f4d332 | works | works | partly | works | works | partly | None |
| Too Many Types 2 | 1.5.2; SHA-1 3fa8e61e | works | works | partly | works | no | partly | No `DEX` tab |
| Glazed | 9.2.0; SHA-1 e10105d8 | works | works | partly | works | works | partly | None |
| Emerald Imperium | 1.3.1; SHA-1 1d20091c | works | works | partly | works | no | partly | No `DEX` tab |
| Quetzal | English Alpha 9 v0; SHA-1 d0658315 | works | works | partly | works | works | partly | None |
| Quetzal | Spanish Alpha 9 v0; SHA-1 fe346b5b | works | works | partly | works | works | partly | None |
| Quetzal | Alpha 9 v1, English and Spanish; SHA-1 c108cec7, ff008b70 | works | works | partly | works | works | partly | None |
| Quetzal | Alpha 0.8.4, English and Spanish; SHA-1 6806c714, b6542c7d | works | works | partly | works | works | partly | None |
| Moemon Quetzal | Alpha 0.8.4 Hotfix, English and Spanish; SHA-1 f1035f61, ae5d88bb | works | works | partly | works | works | partly | None |
| Super Mariomon | 1.2.1, 1.2.2, 1.3, 1.5.1, 1.5.2 | works | works | partly | works | works | partly | None |
| Recharged Emerald | 2.2.5, 2.2.8, Rebalanced 2.2.8 | works | works | partly | works | works | partly | None |
| Modern Emerald | 3.6 (BPEE, 32 MB, SHA-1 ab3bc012) | works | works | partly | works | no | no | None (no `DEX` or `GUIDE` on Android either). Its types follow the settings you picked when you started your game |
| Elite Redux | 2.65.3b (SHA-1 5b38a681), 2.65.4.1b (2f3e17e5) | works | works | partly | works | works | partly | None |
| Team Rocket Dragonsden | 1.5; SHA-1 40780347 | works | works | partly | partly | no | partly | None (no `DEX` on Android either) |
| Team Rocket Dragonsden | 2026 3.0.9 Spanish (BPEE, 32 MB, SHA-1 0d8bdaee) | works | works | partly | works | works | partly | None (read from the app's own data for this build; not yet run on a PC) |
| Wishes of Tomorrow | 1.2.6; SHA-1 60182b93 | works | works | partly | works | works | partly | None |
| Petalburg Dynasty | 1.12.2; SHA-1 648c88a4 | works | works | partly | works | works | partly | None |
| Dreamstone Mysteries | The one release; SHA-1 20e91aec | works | works | partly | works | works | partly | None |
| Monomyth | 1.0 (SHA-1 ae1c4be9), 1.1 (eb660caf) | works | works | partly | works | works | partly | None. Its `DEX` has `EVOLVE`, `AREA` and `MOVES`, and its guide reads its trainers' teams and wild tables |
| Emerald Rogue | 2.2.1 EX; SHA-1 7600af1f | works | works | works | works | no | no | No `DEX` tab (no guide on Android either) |
| SoulGold | 1.1.4, 1.2 (3 builds), 1.2.1 (2 builds) | works | works | partly | works | no | partly | No `DEX` tab |
| R.O.W.E. | 2.1.9.1 Experimental; SHA-1 81bd0f4b | works | works | partly | works | partly | partly | GUIDE has HERE but no NEXT BOSS (its gyms come in any order), as on Android |

## What the gaps come from

- **The DS games.** Platinum (USA) has the whole companion, `BATTLE` and
  `GUIDE` included. HeartGold and SoulSilver (USA) have it too, and so do
  Black, White, Black 2 and White 2 (USA / Europe). Renegade Platinum reads
  its own types, moves and type chart from its cart. Origin HeartGold has
  `PARTY`, `BAG`, `MAP`, `DEX` and `BATTLE` from its own cart and its own
  battle engine, and a `GUIDE` with its wild creatures, which the app doesn't
  have.
  The other languages and the French and Spanish Platinum carts get no
  companion yet.
- **A missing `DEX` tab.** Heart and Soul, Wishes of Tomorrow, Dreamstone
  Mysteries, Lazarus, Monomyth, Petalburg Dynasty, Let's Go and Emerald Seaglass now have their `DEX` on the desktop, read from
  each game's own data. The other games built the same way get it too once the
  desktop sees that data in the game. Emerald Rogue and SoulGold still
  have no `DEX` tab on the desktop, though the app shows one.
- **No foe team in battle.** In the German, French, Italian and Japanese FireRed
  and LeafGreen carts, the battle panes show your side and the Pokémon you face,
  but not the rest of the foe's team.
- **No guide for the Japanese carts** of FireRed, LeafGreen, Ruby, Sapphire and
  Emerald.

If a gap here matters to you, ask for it on the fork's issues page and name the
game and version.
