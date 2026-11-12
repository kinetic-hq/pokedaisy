# 3DS port

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 186-210). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## 3DS port (`3ds/`, branch `port-3ds`)

A homebrew 3DS build, early preview (see `3ds/README.md`): a portable C core (`3ds/core/`) re-implements
the companion's reads (addresses generated from `gamedata/`: `pd_configs_gen.h`; decoding from `Gen3Mon.kt` - keep in sync) and
draws it at 320x240 in the OPTION look; `3ds/ctr/` runs it beside libmgba (the same submodule). Its
tables, font, map layouts, guides (`Guide*.kt` + `GuideAreas*Gen.kt`, by a Kotlin-subset parser in
`tools/gen_guide.py`) are generated from the app's (`make -C 3ds tables`). Look at UI changes with `make -C
3ds preview` (PNGs from the RAM fixtures); `make -C 3ds azahar-test` boots the real `.3dsx` in the
Azahar emulator on a fixture test ROM (GBA homebrew that loads a fixture's RAM, `3ds/test/`). MAP,
POKéDEX and GUIDE read the ROM: `make -C 3ds decomps` builds pret's FireRed rev 1 / Emerald (retail
SHA1s, checked) into `3ds/build/decomps/`, the preview passes them in, and `fixture-roms` writes the
fixture ROM over the decomp's start (`fixture-*-rom.gba`: the tables sit past 0x08200000) - build/ only,
never committed. Only retail English FireRed / LeafGreen / Emerald so far (ROM table addresses for FireRed
rev 1 and Emerald only: rev 0 / LeafGreen get MAP but no POKéDEX, and GUIDE without wild / BOSS); runs on a New 3DS
(first tried 2026-11-04: `3ds/README.md`). Its tabs: PARTY, BAG, MAP, GUIDE, POKéDEX (BATTLE takes the last chip in a battle: INFO /
SUGGESTIONS + the FOE TEAM, ported from BattleInfoScreen / SuggestionsScreen / CompanionScreen's foe state), and
SETTINGS (screen mode, FF, one save state slot - states never carry the save data), with the TRAINER CARD
under it (`pd_card`: its renders match `TrainerCardTest`'s pinned CRCs - `make preview` prints them; keep it
so). ROM art (`pd_romart`) is one scan per ROM for every blob (map + card), done while the game loads. No retail trainer-battle
fixture exists, so `make preview` builds one from `emerald_de_battle` (party structs copied into gEnemyParty)
and keeps it in `3ds/build/fixtures/emerald_trainer` for `make azahar-battle-test`. mGBA's 3DS build
loads every ROM into one fixed buffer, so per-ROM caches key on `pd_rom_id()`, never the ROM pointer. The GPU path
(`3ds/ctr/gpu.c`) writes textures from the CPU in the tiled order: display transfers into textures drew
upside down / rotated in Azahar, so don't go back to them without checking on hardware.
