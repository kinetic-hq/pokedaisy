# Headless captures

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 171-185). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## Headless captures

`native-capture/mgba_dump` (see its README) runs in the `pokedaisy-capture` Docker image
(`native-capture/Dockerfile`; the capture scripts build it on first use, or run
`make capture-image`). The image's `ENTRYPOINT` is `/bin/bash`, so always run
`docker run <image> -c "<full command>"`. Bare args make bash treat the command as a
script and fail with a misleading `cannot execute binary file`. Never reuse a `.ss`
savestate against a rebuilt ROM (stored code pointers go stale). Boot from a `.srm`
instead. To get a battle without walking to grass: poke a tiny script
(`setwildbattle` / `dowildbattle` / `end` bytes) into free EWRAM and `call` the game's
`ScriptContext_SetupScript` with its address - find that function through the
literal-pool words pointing at `gScriptCmdTable` (Lazarus: `0x0820B2B0`, see the
`lazarus_battle` fixture's README; newer expansion's `setwildbattle` takes
species2/level2/item2 too, and zero padding is harmless since 0x00 is `nop`).
