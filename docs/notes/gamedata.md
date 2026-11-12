# Per-game data

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 100-118). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## Per-game data lives in `gamedata/`

The Gen 3 `NativeConfig`s are TOML in `gamedata/gen3/` (one file per game family, comments and all), shared by
every platform: `make gamedata` (`scripts/gen_gamedata.py`) writes the app's `NativeConfigsGen.kt` from them, and
`make check` / CI fail while it's stale. **A new config or a changed address goes in the TOML, never in Kotlin**;
a new field goes in `NativeConfig` *and* `gamedata/schema/gen3_config.toml` (the generator checks they agree).
`gamedata/README.md` has the format. The language ports are there too (`gamedata/gen3/ports/`, written by
`port_retail.py`), with their dex / guide tables spelled out as sub-tables over a named Kotlin val
(`[config.port_axpd0.pokedex]`, `base = 'POKEDEX_SAPPHIRE'`; `gamedata/schema/gen3_pokedex.toml` / `gen3_guide.toml`).
Phase 2: the records the 3DS / Linux companion reads are in `gamedata/gen3/records/` (retail FireRed / LeafGreen /
Emerald's `POKEDEX_*`, `GUIDE_TABLES_*`, `TRAINER_CARD_*`, `*_DEX_FLAGS`, `*_BAG_POCKET_ORDER`, `FIELD_ITEMS_*` (+ `PARTY_ITEMS_*`, `PC_*`, `FLY_*`, `HEAL_WARP_EMERALD`): `GameDataRecordsGen.kt`,
lazy vals), and the 3DS port's `struct pd_config`s are generated too (`3ds/core/pd_configs_gen.h`, from
`gamedata/targets/3ds.toml`: per config, which parts the port reads). The other records (hacks', other languages',
layouts: still Kotlin vals the TOML names), the games' identity and the name tables come later. Until phase 1's gate passes,
`GameDataMigrationTest` checks the generated configs and records equal the old Kotlin ones (`LegacyNativeConfigs.kt`,
`LegacyRetailPorts.kt`, `LegacyRecords.kt`, test sources; `scripts/convert_native_configs.py` made the first, a
one-off), and `make -C 3ds test-configs-gen` the generated C against the old hand-written tables
(`3ds/test/configs_gen_test.c`).
