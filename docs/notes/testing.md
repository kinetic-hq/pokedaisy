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

## The Android build on the Windows machine (portable, no system install)

The fork's Windows machine runs the Android unit tests from a portable toolchain kept under `D:\pdbuild`, never
installed system wide and never put on PATH or in the registry:

- **JDK 17** (Temurin, the Windows x64 zip from Adoptium) unzipped to `D:\pdbuild\jdk\jdk-17.<x>`. The app targets 17,
  and Gradle 8.10.2 won't start on a much newer JDK (an installed Java 27 doesn't work).
- **Android SDK**: the command-line tools zip from dl.google.com unzipped to
  `D:\pdbuild\android-sdk\cmdline-tools\latest`, then `sdkmanager --sdk_root=D:\pdbuild\android-sdk --licenses`
  (the owner accepts, or approves accepting) and `"platforms;android-34" "build-tools;34.0.0" "platform-tools"`. The
  first build installs the NDK named by `ndkVersion` by itself.

Set the variables in the build shell only (Git Bash):

```
export JAVA_HOME="D:/pdbuild/jdk/jdk-17.<x>" ANDROID_HOME="D:/pdbuild/android-sdk"; export PATH="$JAVA_HOME/bin:$PATH"
./gradlew :app:testDebugUnitTest -Pquick --tests '*OriginHeartGoldTest'
```

In cmd, write `set "JAVA_HOME=..."` with the quotes, or the space before `&&` becomes part of the path. A ROM-backed
test reads `POKEDAISY_ROM_DIR` (the `gba` folder; its parent holds `nds/`): point it at a scratch copy under
`D:\pdbuild`, never at the emulation folders.

On this Windows checkout (`core.autocrlf=true`) 17 quick tests fail for Windows reasons alone, and pass on Linux / CI:
CRLF sources read by `PerformanceGuardsTest` and `SiteDataExportTest`; files renamed over an existing one
(`FlacWriterTest`, `RzipSaveTest`, `FfMusicBudgetTest`, `SaveFileEditsTest.aTradeWritesBothOrNeither`,
`RomAddedTest`); `\` paths (`RomFolderTest`, `BpsPatchTest`). Anything else failing is real.
