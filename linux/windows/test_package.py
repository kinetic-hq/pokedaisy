"""Check the real Windows package: the executable, SDL2.dll, licenses, empty DATA folders, no game data."""
from pathlib import Path
import subprocess, sys, tempfile, zipfile

exe = Path(sys.argv[1]).resolve()
options = sys.argv[2:]
name = 'PokeDaisy-windows-x86_64' + ('-ds-experiment' if '--dsperate' in options else '')
script = Path(__file__).with_name('package.py')
with tempfile.TemporaryDirectory(prefix='pd-windows-package-') as d:
    result = subprocess.run([sys.executable, str(script), *options, str(exe), d], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    archive = Path(d) / (name + '.zip')
    assert archive.exists() and Path(result.stdout.strip()) == archive
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        names = z.namelist()
        assert all(n.startswith(name + '/') for n in names), names
        assert z.read(name + '/PokeDaisy.exe') == exe.read_bytes()
        assert z.read(name + '/SDL2.dll')[:2] == b'MZ'
        assert name + '/licenses/SDL2-zlib.txt' in names and name + '/licenses/mGBA-MPL-2.0.txt' in names
        assert name + '/licenses/xBRZ.txt' in names
        for folder in ('DATA/', 'DATA/roms/', 'DATA/saves/', 'DATA/states/'):
            assert name + '/' + folder in names, folder
        dlls = [n for n in names if n.lower().endswith('.dll')]
        assert dlls == [name + '/SDL2.dll'], dlls
        assert z.read(name + '/SOURCE.txt').startswith(b'Source: https://github.com/kinetic-hq/pokedaisy')
        if '--melonds' in options:
            assert name + '/licenses/melonDS-GPL-3.0.txt' in names and name + '/licenses/Teakra-MIT.txt' in names
            assert b'melonDS: https://github.com/melonDS-emu/melonDS' in z.read(name + '/SOURCE.txt')
        assert not any(n.lower().endswith(('.gb', '.gbc', '.gba', '.nds', '.sav', '.ss', '.srm', '.bin')) for n in names)
    before = archive.read_bytes()
    result = subprocess.run([sys.executable, str(script), *options, str(exe), d], capture_output=True, text=True)
    assert result.returncode != 0 and archive.read_bytes() == before
# A release: the versioned zip and, for older versions' updater, the same bytes under the old name.
with tempfile.TemporaryDirectory(prefix='pd-windows-package-') as d:
    result = subprocess.run([sys.executable, str(script), *options, '--version', '9.8.7', '--updater-copy', str(exe), d],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    suffix = '-ds-experiment' if '--dsperate' in options else ''
    versioned, legacy = Path(d) / f'PokeDaisy-9.8.7-windows-x86_64{suffix}.zip', Path(d) / 'pokedaisy-windows-x86_64.zip'
    assert result.stdout.split() == [str(versioned), str(legacy)], result.stdout
    assert versioned.read_bytes() == legacy.read_bytes()
    with zipfile.ZipFile(legacy) as z:
        # 1.18.1's installer copies staging\pokedaisy-windows-x86_64 (Windows matches the folder whatever its case).
        assert all(n.lower().startswith('pokedaisy-windows-x86_64') for n in z.namelist())
        assert z.read(name + '/VERSION.txt').strip() == b'9.8.7'
print('PASS Windows package: executable, SDL2.dll, licenses, empty DATA folders and overwrite refusal')
