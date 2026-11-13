#!/usr/bin/env python3
"""Write 3ds/core/pd_modern_emerald_gen.h from the app's ModernEmeraldGen.kt: Modern Emerald's types that follow the
save's settings (classic types, no Fairy, the two new ones) and its "Modern" type chart, for the desktop and 3DS cores.
The default tables (modern types, the Gen VI+ chart) reach the core through gamedata/targets/3ds.toml's
[text.modern_emerald].

Usage: python3 scripts/gen_modern_emerald_c.py [--check]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KT = ROOT / 'app/src/main/kotlin/com/pokedaisy/app/companion/data/ModernEmeraldGen.kt'
OUT = ROOT / '3ds/core/pd_modern_emerald_gen.h'


def table(text, name):
    m = re.search(r'internal val ' + name + r': Map<Int, \w+> = mapOf\((.*?)\n\)\n', text, re.S)
    if not m:
        sys.exit('missing ' + name)
    return m.group(1)


def types(text, name, ident):
    rows = re.findall(r'(\d+) to SpeciesTypes\((\d+), (\d+)\)', table(text, name))
    if not rows:
        sys.exit('empty ' + name)
    out = ['static const struct pd_modern_types %s[%d] = {' % (ident, len(rows))]
    out += ['    { %s, %s, %s },' % r for r in rows]
    return out + ['};', '']


def main():
    text = KT.read_text(encoding='utf-8')
    out = ['// Made by scripts/gen_modern_emerald_c.py from ModernEmeraldGen.kt: do not edit.', '#pragma once',
           '#include <stdint.h>', '',
           'struct pd_modern_types { uint16_t species; uint8_t t1, t2; };', '']
    out += types(text, 'classicTypesModernEmerald', 'PD_MODERN_CLASSIC')
    out += types(text, 'noFairyTypesModernEmerald', 'PD_MODERN_NO_FAIRY')
    out += types(text, 'modernNewTypesModernEmerald', 'PD_MODERN_NEW')
    chart = re.findall(r'(\d+) to (\d+)', table(text, 'typeChartModernModernEmerald'))
    out.append('// gTypeEffectiveness: TYPE CHART "Modern" ({atk, def, percent}).')
    out.append('static const struct pd_type_pair PD_MODERN_CHART[%d] = {' % len(chart))
    out += ['    { %d, %d, %s },' % (int(k) // 100, int(k) % 100, v) for k, v in chart]
    out += ['};', '']
    body = '\n'.join(out)
    if '--check' in sys.argv:
        if not OUT.exists() or OUT.read_text(encoding='utf-8') != body:
            sys.exit('3ds/core/pd_modern_emerald_gen.h is stale: run scripts/gen_modern_emerald_c.py')
        return
    OUT.write_text(body, encoding='utf-8', newline='\n')
    print('wrote', OUT.relative_to(ROOT))


if __name__ == '__main__':
    main()
