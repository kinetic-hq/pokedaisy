# PokeDaisy (kinetic-hq fork): Notes for Claude

PokeDaisy is a dual-screen Android app (AYN Thor, Retroid Pocket Duo). It runs an embedded libmgba core (and melonDS
for DS games) on the top screen and a live companion for Gen 3 Pokémon games, ROM hacks, Game Boy and DS games on the
bottom screen. This fork adds a desktop host for Windows and Linux (`linux/`) and a 3DS port (`3ds/`).

Where to read more:

- `README.md`: the fork's features and its "How this fork differs from PokeDaisy" list.
- `docs/pokedaisy-readme.md`: PokeDaisy's own README.
- `docs/DEVELOPMENT.md`: per-ROM support, how each game was mapped, frontends, WIDE / EXPAND, DS, and so on.
- `docs/notes/`: the project notes that used to fill this file, moved there verbatim (index below).
- `docs/ROM_FEATURES.md`: which ROM has which feature.

This file keeps only the rules and commands every session needs. Before you change an area, read its note in
`docs/notes/`. Code comments and the add-rom skill that say "see CLAUDE.md" mean those notes.

## Ground rules (never break these)

- **This repo is public.** Never commit ROMs, saves, savestates, game frames or secrets. Keys come from the user at
  runtime (SteamGridDB, RetroAchievements) or from CI secrets. Game art is rebuilt from the player's own ROM
  (`RomArt`) and is never bundled. Write home paths as `~` in anything you commit.
- **No AI attribution anywhere**: no `Co-Authored-By`, no "Generated with", no model names, never authored as
  Claude. A local `commit-msg` hook rejects them. Never bypass it (`--no-verify` is forbidden).
- **The user's files**: never create, move, rename or delete anything in the emulation folders (`D:\Emulation`,
  `~/Downloads/Game ROMs & Emulation/` and the like: ROMs, saves, states), and never run a game from there. Copy what
  a test needs into a scratch folder (`D:\pdbuild\...` on the Windows machine) and work on the copy.
- **Save files are sacred.** Every path that writes a save needs a backup first and must check that the file hasn't
  changed underneath it. A state never writes the save. See `docs/notes/ui-and-features.md` ("Save files are sacred",
  "Where a save is").
- **Exposed surfaces** (`LaunchActivity`, `CoopLinkActivity`, the updater, release notes, pinned workflow actions)
  keep their checks. See `docs/notes/project.md`. New bundled code, fonts or data go into NOTICE and the LICENSES page.
- **A change to a ROM's features** (EXPAND, battle buttons, one-press picks, the POKéMON switch, field items, FLY,
  the card) updates its row in `docs/ROM_FEATURES.md` in the same commit.

## Fork working rules (kinetic-hq/pokedaisy)

These come first where they differ from upstream's notes:

- **Branches** are `tanjid/<change-type>/<short-name>`, where the type is `feat`, `fix`, `docs`, `perf`, `refactor`,
  `test` or `chore`. **Commits** are `<change-type>: <what changed>`.
- **Auto PR and merge**: once the local build and tests pass, push and open the PR against `main` of
  kinetic-hq/pokedaisy (never lidor30/pokedaisy). Then run `gh pr merge --squash --delete-branch`. Never force-push
  `main`.
- **README's "How this fork differs from PokeDaisy"** gets a line for every behaviour change, in the same PR. The
  manual gets one too (`docs/manual/windows.md` for the desktop host).
- **Upstream sync**: `.github/workflows/upstream-sync.yml` merges lidor30/pokedaisy daily. `CLAUDE.md` always
  resolves as ours. Whatever upstream changed in its CLAUDE.md is appended to `docs/notes/upstream-claude-md.md` for a
  person to fold into the right note. The workflow defines the `ours` merge driver itself; before syncing by hand, run
  `git config merge.ours.driver true` once in the checkout, or `merge=ours` is ignored and CLAUDE.md conflicts.

## Build and test

### Windows (MSYS2 UCRT64 or WinLibs MinGW-w64, Ninja, the SDL2 2.32 MinGW dev package)

```
cmake -S linux -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=<SDL2>/x86_64-w64-mingw32 -DPOCKET_MELONDS=ON
cmake --build build-win
ctest --test-dir build-win -E "package-smoke|linux-runtime-test"
```

Re-run the configure step after adding files. SDL2.dll must be on PATH to run the exe or the tests.

### Linux (or WSL)

```
cmake -S linux -B linux/build && cmake --build linux/build && ctest --test-dir linux/build
```

### 3DS core preview (any host compiler)

```
gcc -std=c11 -O2 -Wall -Wextra -Werror -o pd_preview 3ds/core/*.c 3ds/desktop/pd_preview.c 3ds/desktop/pd_png.c -lm
```

`make -C 3ds preview` builds the same previews on Linux. Run `PD_PREVIEW_WING=WxH` with a fixture directory to get a
wing render.

### Android app

- **Everyday check**: `make test-quick` (about a minute).
- **While working**: run only the tests you touched, e.g. `make test-quick T='*MoneyTest'`.
- **Ask the user before any full run** (`make test`, `make check`, `make test-roms`, every Paparazzi class, the whole
  ui-preview suites). Details are in `docs/notes/testing.md`.
- **Look at every UI change before calling it done**: one Paparazzi class, or `cd ui-preview && gradle render
  -Ponly=<screen>`. See `docs/notes/ui-and-features.md`.
- **Per-game data** is TOML in `gamedata/` (`make gamedata`). A new config or a changed address goes in the TOML,
  never in Kotlin. See `docs/notes/gamedata.md`.

### Known local-only failures

- `package-smoke` needs SDL2.dll next to the packaged exe.
- `linux-runtime-test` needs a real ROM and save, and misreads when a real controller is plugged in.

CI runs both.

### Headless looks (desktop host)

```
SDL_VIDEODRIVER=dummy POCKET_WINDOW=1920x1080 pokedaisy.exe --data <dir> --frames N --shot out.png [rom]
```

Script input with `POCKET_KEYS`, `POCKET_MENU_KEYS`, `POCKET_DS_HOST_TAPS` or `POCKET_DS_KEYS`. Point `--data` at a
scratch copy, never at the user's folders. Read the PNG.

## Releases (desktop)

- **Automatic**: `desktop-release.yml` cuts `desktop-vX.Y.Z` pre-releases from `main` by itself whenever there are
  feat / fix / perf commits.
- **By hand**:
  1. Build the Windows zip with `python linux/windows/package.py --melonds --version X --dll-dir <SDL2 bin> <exe>
     <out>`.
  2. Build the Linux zip under WSL with `linux/package.py --melonds ...`.
  3. Write a `SHA256SUMS` and the notes.
  4. Run `gh release create desktop-vX.Y.Z --repo kinetic-hq/pokedaisy --target <full commit SHA> --prerelease`.
- Always pass the **full** SHA and `--prerelease`.
- Android releases (`v<versionName>` tags, the owner's signing key) follow `docs/RELEASING.md`.

## The manual (`docs/manual/`)

Every change a player can notice updates the manual in the same change. That covers a new feature, setting, tab,
button or supported game, a changed default, a renamed label, and a removed option. If unsure, it counts.

The writing rules (the user's):

- Write plainly, in the second person.
- **No em or en dashes**, and no spaced hyphen standing in for one.
- **No real Pokémon, move or item names**, in text or pictures. The invented creatures in
  `website/src/lib/creatures.js` stand in.

`make docs-check` enforces this, along with links, images and settings keywords. The full rules are in
`docs/manual/README.md` and `docs/notes/manual.md`.

**NEW tags**: every new player-facing feature gets one (`NewFeatures`, `docs/notes/new-tags.md`).

## Answering GitHub issues (the user's rules)

- **Draft first and post only after a clear OK.** Keep replies to two or three plain sentences, with no internals.
- **Use the future tense until a fix ships**, and name the next version.
- **Make no follow-up promises.** Point to Watch > Custom > Releases instead.
- **Use the reporter's own game names.**
- **Never write "é" in "Poke" words.**

Full list: `docs/notes/ui-and-features.md` ("Answering GitHub issues").

## Notes index (`docs/notes/`)

| Note | What's in it |
| --- | --- |
| `project.md` | What the app is, public-repo rules, exposed surfaces, the co-op link and joined battles |
| `manual.md` | The manual's format, the docs API, search, screenshots |
| `new-tags.md` | NEW tags on new features |
| `gamedata.md` | `gamedata/` TOML, generators, the migration gate |
| `feature-tips.md` | TIP cards and their scheduler's rules |
| `private-repo.md` | The private ROM-hack repo PokeDaisy came from (never named in user-facing docs) |
| `local-setup.md` | `local.mk`, submodules, where ROMs live and how they're named, package renames |
| `headless-captures.md` | `mgba_dump` in Docker, scripted battles |
| `3ds-port.md` | The 3DS port |
| `linux-port.md` | The SDL2 host, DS on DSperate / melonDS, DS LAYOUT |
| `testing.md` | Quick vs full runs |
| `ui-and-features.md` | The look (OPTION windows, pixel corners, two-tone buttons, themes, dark mode) and every feature's notes, game by game: HUD, EXPAND, FF music, battle input, save editing, releases, the website, languages... |
| `upstream-claude-md.md` | Upstream CLAUDE.md changes waiting to be folded into the notes (made by the sync) |

These notes are long. Search them (`grep -n "<topic>" docs/notes/*.md`) rather than reading them whole. When you learn
a gotcha a future change would trip on, add it to the matching note, not here.
