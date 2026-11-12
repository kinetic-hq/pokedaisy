# Testing

> Moved verbatim from CLAUDE.md on 2026-11-12 (lines 257-276). The guidance is Lidor Itzhari's, PokeDaisy's author (lidor30/pokedaisy), with this fork's additions. Code comments and the add-rom skill that say "CLAUDE.md" mean these notes.

## Testing: quick by default, full runs only with the user's OK

The user runs several agents at once and runs the full suites about once a day, so (the user's rule, 2026-11-01):

- **Everyday check: `make test-quick`** (`./gradlew :app:testDebugUnitTest -Pquick`, about 1 min warm). It
  drops the Paparazzi renders (`*ScreenshotTest`: they assert nothing and were 86% of the full run's ~10 min)
  and `BestEffortTest` (26 whole ROMs, ~45 s), and runs the forks side by side. Fixture decodes, ROM-backed
  tests and guards still run. Run it before calling a change done.
- **While working, run only the tests for what you touched**: `make test-quick T='*MoneyTest'` (~25 s), or
  several with `./gradlew :app:testDebugUnitTest -Pquick --tests '*Foo*' --tests '*Bar*'`. A test you add or
  change: run it by name. `BestEffortTest` runs by name too if you touch detection / `BestEffort`.
- **Ask the user before any full run**, and say why: `make test`, `make check`, `make test-roms`, a plain
  `./gradlew :app:testDebugUnitTest` (no `-Pquick`), every Paparazzi class at once (`recordPaparazziDebug` over
  `'*ScreenshotTest'`), and the whole ui-preview suites (`gradle render` with no `-Ponly`, `stress`,
  `docsSearchCheck`, `keyedStateCheck`, `docsShots`, `make docs-shots`). CI runs all of them on every PR, so a
  branch still gets the full check before it merges.
- **Looking at a changed screen is not a full run**: one Paparazzi class (`--tests '*PortraitScreenshotTest'`)
  or one ui-preview render (`gradle render -Ponly=settings-home`, ~25 s warm) is fine without asking (see UI
  work below). Name only the screens you changed.
