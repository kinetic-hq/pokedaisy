# PokeDaisy for Windows

The Linux SDL2 host (`linux/`, see its README) built natively for Windows x64:
the pinned mGBA library, the portable companion in `3ds/core/` and SDL2. Same
controls, settings, HUD, quick menu, EXPAND and companion as on Linux.

## Use the portable zip

Unzip `PokeDaisy-<version>-windows-x86_64.zip` anywhere you can write to. Keep
`PokeDaisy.exe` beside `SDL2.dll`. Your games go in `DATA\roms`; saves, save
states, `settings.ini` and the log (`pokedaisy.log`, when started without a
console) are written under `DATA`, beside the program, whatever folder it is
started from. `--data FOLDER` picks another one.

`POCKET_FULLSCREEN=1` fills the screen (SDL's current display mode),
`POCKET_WINDOW=WxH` opens a window of that size. A ROM path argument starts that
game directly.

## Build from source

The build uses MinGW-w64 (the UCRT64 environment of [MSYS2](https://www.msys2.org)):

```
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-python
git submodule update --init third_party/mgba
cmake -S linux -B linux/build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build linux/build-win
ctest --test-dir linux/build-win --output-on-failure
python linux/windows/package.py linux/build-win/pokedaisy.exe linux/build-win/package
```

Run these in an MSYS2 UCRT64 shell (or with `C:\msys64\ucrt64\bin` first on
PATH). MSYS2's CMake is 4.x, which refuses the pinned mGBA's old
`cmake_minimum_required` without `CMAKE_POLICY_VERSION_MINIMUM`. The C runtime is linked in, so `SDL2.dll` is the only DLL the package
needs; `package.py` finds it with `objdump` and refuses any other DLL it has no
license notice for. The executable carries a manifest that sets the UTF-8 code
page (paths with accents work in Windows 10 1903 and later) and per-monitor DPI
awareness, so whole-pixel scaling uses the screen's real pixels.

What differs from Linux, all in `linux/pocket_os.c`: directories are created and
listed with the Windows API, and state and backup files are moved into place
with `MoveFileEx(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` (a plain
`rename` can't replace a file on Windows). The Brick's Fn-key node reader is
Linux only. libmgba uses Windows threads there.

CI: `.github/workflows/windows.yml` builds, runs CTest and uploads the zip.
