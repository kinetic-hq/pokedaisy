# Project overview, public-repo rules and co-op

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 1-43). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

# PokeDaisy — Project Notes for Claude

Dual-screen Android app (AYN Thor, Retroid Pocket Duo): an embedded libmgba core on the
top screen and a live companion for Gen 3 Pokémon games and ROM hacks on the bottom
screen. Read `README.md` for the user-facing feature list and `docs/DEVELOPMENT.md` for the
per-ROM support details, how each game was mapped, frontend setup and releasing; this file is
process context that isn't obvious from the code. `docs/ROM_FEATURES.md` tracks which ROM has EXPAND, battle
buttons, one-press picks, the POKéMON switch, field items, FLY and the card: a change to any of those for a ROM (or a
new ROM) updates its row in the same commit.

**This repo is public.** Never commit ROMs, saves or secrets. The SteamGridDB API key is
entered by the user at runtime and must stay that way. Game art (FireRed / Emerald's and the
hacks') is rebuilt from the player's own ROM (`RomArt`), not bundled. Keep it that way. Write home paths as `~` in
anything committed (fixture READMEs: `capture_fixture_headless.sh` does). **Commits carry no Claude /
Anthropic attribution** (no `Co-Authored-By`, no "Generated with" lines, never authored as Claude) - the
user's firm rule; a local `commit-msg` hook rejects them, never bypass it.
**Exposed surfaces**: `LaunchActivity` is exported - it refuses the app's own files / provider, copies at
most 40 MB (512 MB with DS games on: `RomIntake.maxCopyBytes`) and plays only what has a cart header (`RomIdentity.looksLikeRom`); the updater installs only
this package, same signer, newer version (`AppUpdateFlow.isOurUpdate`); the website's release notes are
escaped (`website/src/lib/releases.js`, `website/test/releases.test.js`); workflow actions are pinned to
commit SHAs. `CoopLinkActivity` is exported for `pokedaisy://coop/<ticket>` only: it passes a `CoopInvite.parse`d ticket to the unexported Settings, nothing else.
New bundled code / fonts / data go into NOTICE and the LICENSES page (`licensesAsset`).

**Co-op link** (`companion/coop/`; docs/DEVELOPMENT.md's "Co-op connection"): pure Kotlin, no native code - `RelayCoopTransport` over
our own WebSocket relay (`server/coop-relay/`, a separate Kotlin / JVM program on Java-WebSocket, never in the APK; its README covers deploying it) plus direct
TCP and Android NSD on the same Wi-Fi, every path carrying a Noise IK channel (`CoopChannel`, `Noise_IK_25519_AESGCM_SHA256`, Bouncy
Castle's X25519), poll-based (`poll()`); nothing from the network reaches game memory (`CoopSafetyTest`); pairs per save
(`CoopSaveId`), English Emerald only, refused with cheats loaded; the relay URL (`COOP_RELAY_URL`) is a build secret, never committed,
and with none the link is the same Wi-Fi and BY IP only. **Behind `FeatureFlags.coop`** (off; `-Ppokedaisy.coop=true` /
local.properties / marker `files/features/coop`): its rows' keywords sit in `FLAGGED_SETTING_KEYWORDS` and the manual page is a draft
in `docs/manual-drafts/coop.md` (move it into `docs/manual/` with the settings-reference / synonyms entries when the flag goes).
Transport start / close / poll never on the UI thread.
**Co-op joined battles** (Emerald, behind the same flag; docs/DEVELOPMENT.md's "Co-op joined battles" and "The link bridge", messages `0x0200`-`0x02FF`):
a nearby free partner (same map, within SETTINGS > CO-OP > NEARBY RANGE, `Prefs.coopNearbyRange`, 2 / 4 / 8 / 16 / 32 steps) joins a wild,
two-trainer or one-trainer (2+ Pokémon) battle with their first three able Pokémon, and a story battle (gym leader, rival, a boss) in every save when they
stand at the same spot (same trainer, not beaten; the game's own choose-three screen). **A joined battle is Emerald's own link battle on both devices** (the tower's
link multi, host = link master, `pk_coop_link*.c`, link.c's blocks carried by `CoopLinkFlow` over the co-op link, no serial emulation): each player sees it from their
own side with their own game's menus, the app draws only the join prompt, the waits and notices. Rules: the prize is what a two-trainer battle pays (no x2); a ball works
only with one wild Pokémon left and whoever catches it keeps it (the host holds the partner's catch until the partner has saved it); each side's EXP / bag is its own; a player
blacks out only when no Pokémon in the whole party can fight; PLAYER LOOK / TEXT LOOK must be off; a dropped link waits 60 s, then a story battle is a loss with a blackout for
both (every save, also on the team-pick screen) and any other battle ends for both with no prize, the trainer not beaten, what was earned kept, no evolution from it. A stop
always puts the game back before any resume / suspend state is written (a stop that can't be confirmed writes none). Its manual text is the "Battling together" part of the draft.
