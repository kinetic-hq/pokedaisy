# Local setup

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 146-170). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## Local setup on this machine

- `local.mk` (untracked, see `local.mk.example`): `DECOMPS` →
  `~/Projects/tests/my-rom-hacks/build`, `MON_ICONS` → that repo's
  `tools/telemetry-viewer/assets/pokemon`. `make` exports both; outside make, export them
  yourself for `python3 scripts/gen_*.py` or `gradle render`.
- `local.properties` (untracked): Android `sdk.dir`.
- `third_party/mgba`: git submodule at mGBA `0.10.5`; `third_party/rcheevos`: at `v12.5.0`
  (`make submodules` fetches them and `third_party/melonds`).
- ROMs/saves for headless captures: `scripts/host_roms.conf`. On-device paths:
  `scripts/roms.conf` (`/sdcard/Android/data/com.pokedaisy.app/files/...`).
- The ROMs the tests read (`RETAIL_ROM_DIR`, `-DromDir` / `POKEDAISY_ROM_DIR` to move it) sit in
  `~/Downloads/Game ROMs & Emulation/`: `gba/` (the default dir, every language's GBA carts and the hacks, since 2026-11-07;
  others are found relative to it: `retailRomFiles` picks the retail ones by name), `gbc/` and `nds/`. Retail
  keeps its No-Intro name; a hack is `Pokemon <Title> (v<version>) (<variant>).<ext>` (Polished Crystal's
  `Pokemon Polished Crystal (v3.2.3) (Faithful).gbc`). Nothing loose in `~/Downloads`. A test, host_roms.conf line or
  fixture README naming a ROM uses these names; renaming a file means grepping for the old one (many tests skip
  quietly when their ROM isn't found, so a stale name only shows as a test that did nothing).
- The package / applicationId was renamed twice: `com.pokedaisey.PokeDaiseyApp` →
  `com.pokedaisey.app` → `com.pokedaisy.app` (2026-10-07, fixing the "Daisey" misspelling
  everywhere, prefs file and `LaunchActivity` component included; the GitHub repo became
  `lidor30/pokedaisy`, which GitHub redirects from the old name). Each is a new app to
  Android: an old install's data (`roms/`, `saves/`, states, prefs) sits under the old
  id's `Android/data/` folder and doesn't carry over by itself.
