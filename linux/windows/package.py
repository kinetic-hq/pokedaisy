"""Package pokedaisy.exe as a portable Windows zip, never game data.

pokedaisy-windows-x86_64/: pokedaisy.exe, the DLLs it needs that aren't Windows' own
(SDL2.dll: found with objdump beside the compiler), README, licenses, the source
record and an empty DATA/roms, DATA/saves and DATA/states."""
from pathlib import Path
import argparse, os, shutil, subprocess, zipfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('exe', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--dsperate', action='store_true', help='label the opt-in DS experiment and include its notices')
p.add_argument('--melonds', action='store_true', help='a POCKET_MELONDS build: DS games on melonDS, with its notices')
p.add_argument('--dll-dir', type=Path, action='append', default=[],
               help='where to look for DLLs (default: beside objdump / gcc on PATH)')
a = p.parse_args()
repo = Path(__file__).resolve().parents[2]
data = a.exe.read_bytes()
pe = int.from_bytes(data[0x3C:0x40], 'little')
if data[:2] != b'MZ' or data[pe:pe + 4] != b'PE\0\0':
    p.error('Expected a Windows executable')
if int.from_bytes(data[pe + 4:pe + 6], 'little') != 0x8664:
    p.error('Supported package target: x86_64')
name = 'pokedaisy-windows-x86_64'
if a.dsperate:
    if b'DSperate %s' not in data:
        p.error('Expected a POCKET_DSPERATE build')
    name += '-ds-experiment'
if a.melonds:
    if b'.melonds' not in data:
        p.error('Expected a POCKET_MELONDS build')
    name += '-ds'


def objdump():
    for tool in ('objdump', 'x86_64-w64-mingw32-objdump'):
        found = shutil.which(tool)
        if found:
            return found
    p.error('objdump (MinGW-w64 binutils) is needed to list the DLLs')


def imports(path):
    out = subprocess.check_output([objdump(), '-p', str(path)], text=True, errors='replace')
    return [line.split(':', 1)[1].strip() for line in out.splitlines() if 'DLL Name:' in line]


dirs = a.dll_dir or [Path(shutil.which(t)).parent for t in ('objdump', 'gcc') if shutil.which(t)]
system = Path(os.environ.get('SystemRoot', r'C:\Windows')) / 'System32'


def local_dll(dll):
    """The DLL's own copy to ship, or None for one Windows has itself."""
    if dll.lower().startswith(('api-ms-win-', 'ext-ms-')):
        return None
    for d in dirs:
        if (d / dll).exists():
            return d / dll
    if (system / dll).exists():
        return None
    p.error(f'{dll} not found in {", ".join(map(str, dirs))}')


ship, todo = {}, [a.exe]
while todo:
    for dll in imports(todo.pop()):
        path = local_dll(dll)
        if path and dll.lower() not in ship:
            ship[dll.lower()] = path
            todo.append(path)

a.output.mkdir(parents=True, exist_ok=True)
top = a.output / name
top.mkdir()  # Refuse to overwrite an earlier package.
shutil.copy2(a.exe, top / 'pokedaisy.exe')
for path in ship.values():
    shutil.copy2(path, top / path.name)
for source, target in [(repo / 'linux/windows/README.md', 'README.md'),
                       (repo / 'LICENSE', 'LICENSE'), (repo / 'NOTICE', 'NOTICE')]:
    shutil.copy2(source, top / target)
licenses = top / 'licenses'
licenses.mkdir()
for path, license_name in [('LICENSE', 'mGBA-MPL-2.0.txt'), ('src/third-party/blip_buf/license.txt', 'blip_buf-LGPL-2.1.txt'),
                           ('src/third-party/inih/LICENSE.txt', 'inih-BSD.txt')]:
    shutil.copy2(repo / 'third_party/mgba' / path, licenses / license_name)
(licenses / 'PixelOperator.txt').write_text('Pixel Operator by Jayvee Enaguas. CC0. See NOTICE.\n')
if 'sdl2.dll' in ship:
    shutil.copy2(repo / 'linux/windows/SDL2-zlib.txt', licenses / 'SDL2-zlib.txt')
unknown = sorted(set(ship) - {'sdl2.dll'})
if unknown:
    p.error(f'No license notice for {", ".join(unknown)}: link it statically or add its notice')
if a.dsperate:
    ds = repo / 'third_party/dsperate'
    for source, target in [('LICENSE', 'DSperate-GPL-3.0.txt'),
                           ('src/core/bios/LICENSE.freebios', 'FreeBIOS-BSD.txt'),
                           ('src/core/cart/miniz/LICENSE', 'miniz-MIT.txt'),
                           ('src/core/io/dsi_font/LICENSE-WenQuanYi-MicroHei.txt', 'WenQuanYi-MicroHei.txt'),
                           ('src/core/io/dsi_font/LICENSE-NotoSans-OFL-1.1.txt', 'NotoSans-OFL-1.1.txt')]:
        shutil.copy2(ds / source, licenses / target)
    shutil.copy2(repo / 'linux/dsperate/README.md', top / 'DS-EXPERIMENT.md')
if a.melonds:
    # melonDS and what it bundles (NOTICE lists them); tiny-AES-c and Steve Reid's SHA-1 carry no license file.
    ds = repo / 'third_party/melonds'
    for source, target in [(ds / 'LICENSE', 'melonDS-GPL-3.0.txt'), (ds / 'src/fatfs/LICENSE.txt', 'FatFs.txt'),
                           (ds / 'src/teakra/LICENSE', 'Teakra-MIT.txt'), (ds / 'src/blip-buf/license.txt', 'melonDS-blip_buf-LGPL-2.1.txt'),
                           (ds / 'src/dolphin/license_dolphin.txt', 'Dolphin-emitter-GPL-2.0.txt'),
                           (repo / 'third_party/licenses/xxHash-BSD-2-Clause.txt', 'xxHash-BSD-2-Clause.txt')]:
        shutil.copy2(source, licenses / target)
    (licenses / 'melonDS-public-domain.txt').write_text('tiny-AES-c: The Unlicense (public domain).\n'
                                                        'SHA-1 by Steve Reid: public domain.\n')
sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip()
core = subprocess.check_output(['git', 'rev-parse', 'HEAD:third_party/mgba'], cwd=repo, text=True).strip()
(top / 'SOURCE.txt').write_text(f'Source: https://github.com/lidor30/pokedaisy\nCommit: {sha}\nmGBA: {core}\n'
                                'Build: linux/windows/README.md at this commit\n')
if a.melonds:
    melon = subprocess.check_output(['git', 'rev-parse', 'HEAD:third_party/melonds'], cwd=repo, text=True).strip()
    with (top / 'SOURCE.txt').open('a') as f:
        f.write(f'melonDS: https://github.com/melonDS-emu/melonDS {melon}\n'
                'with third_party/patches/melonds-pokedaisy.patch at this commit\n')
archive = a.output / (name + '.zip')
empty = ['DATA/', 'DATA/roms/', 'DATA/saves/', 'DATA/states/']
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(top.rglob('*')):
        if f.is_file():
            z.write(f, f.relative_to(a.output).as_posix())
    for d in empty:
        z.writestr(f'{name}/{d}', '')
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert not any(n.lower().endswith(('.gb', '.gbc', '.gba', '.nds', '.sav', '.ss', '.srm', '.bin')) for n in z.namelist())
print(archive)
