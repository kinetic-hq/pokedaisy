# melonDS backend for the SDL host

`-DPOCKET_MELONDS=ON` builds the Windows / Linux host with Nintendo DS games on melonDS
(`third_party/melonds`, pinned, with `third_party/patches/melonds-pokedaisy.patch` applied at configure
time, as the Android app does). Pick it or `POCKET_DSPERATE`, not both: the frontend, companion and field
read are shared (`linux/dsperate/`), and only `DsCore` (`linux/dsperate/ds_core.h`) differs.

- `core_melonds.cpp`: boot (FreeBIOS, generated firmware, direct boot), frames, input, touch, audio,
  the battery save and states, through the app's `app/src/main/cpp/nds/pk_nds_platform.cpp`.
- `build.cmake`: melonDS's core library only (no Qt, no OpenGL, no GDB stub, no LTO), JIT on.

Saves go beside the ROM as `<name>.sav` (the cartridge's own memory, the format other DS emulators use),
backed up to `DATA/backups` on every start; states go to `DATA/states/<file>.melonds.ss` and never carry
the save.

Build and test:

```
git submodule update --init third_party/mgba third_party/melonds
cmake -S linux -B linux/build-melonds -DCMAKE_BUILD_TYPE=Release -DPOCKET_MELONDS=ON
cmake --build linux/build-melonds && ctest --test-dir linux/build-melonds -R ds-
```

On Windows add `-G Ninja` and build in MSYS2 UCRT64 (see `linux/windows/README.md`); package with
`linux/windows/package.py --melonds` (or `linux/package.py --melonds` on Linux).
