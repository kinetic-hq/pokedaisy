"""Wrap linux/icon.png in a .ico for pokedaisy.exe's resource file (a PNG entry, Windows Vista on)."""
import struct, sys
from pathlib import Path
png = Path(sys.argv[1]).read_bytes()
assert png[:8] == b'\x89PNG\r\n\x1a\n', 'expected a PNG'
w, h = struct.unpack('>II', png[16:24])
# ICONDIR, one ICONDIRENTRY (0 = 256), then the PNG itself.
entry = struct.pack('<BBBBHHII', w % 256, h % 256, 0, 0, 1, 32, len(png), 6 + 16)
Path(sys.argv[2]).write_bytes(struct.pack('<HHH', 0, 1, 1) + entry + png)
