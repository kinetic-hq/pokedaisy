# The private ROM-hack repo

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 133-145). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## Relationship to the private ROM-hack repo

PokeDaisy was split out of the user's **private** ROM-hack repo (local
checkout `~/Projects/tests/my-rom-hacks`, where it lived as `tools/pokedaisey`
under the misspelled name pokedaisey) on 2026-10-06, without git history. That repo still owns:

- **The Go `tools/telemetry-viewer`**. Several Kotlin files here are ports of its readers
  (`native.go`, `gen3mon.go`, `unbound.go`, ...) and say "keep in sync".
- **The pinned, built decomps** (`build/pokefirered`, `build/pokeemerald`, `build/pokeruby`,
  `build/pokehns`, ...) that the generator scripts and ui-preview read via `$DECOMPS`.

Public readers can't see that repo, so user-facing docs don't name it or link to it.
