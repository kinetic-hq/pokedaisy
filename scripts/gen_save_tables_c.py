#!/usr/bin/env python3
"""Write linux/save_tables_gen.h from the app's SaveTablesGen.kt (itself made by scripts/gen_save_tables.py from the
decomps): what a retail Gen 3 Pokemon may learn, for the desktop's EDIT SAVE.

Usage: python3 scripts/gen_save_tables_c.py [--check]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KT = ROOT / 'app/src/main/kotlin/com/pokedaisy/app/companion/data/save/SaveTablesGen.kt'
OUT = ROOT / 'linux/save_tables_gen.h'
SPECIES = 412


def block(text, name):
    m = re.search(r'internal val ' + name + r' = (intArrayOf|listOf)\((.*?)\n?\)\n', text, re.S)
    if not m:
        sys.exit('missing ' + name)
    body = m.group(2)
    if m.group(1) == 'intArrayOf':
        return [int(x) for x in re.findall(r'-?\d+', body)]
    return re.findall(r'"([0-9a-fA-F]*)"', body)


def ints(name, values, ctype, per=16):
    rows = [', '.join(str(v) for v in values[i:i + per]) for i in range(0, len(values), per)]
    return 'static const %s %s[%d] = {\n  %s\n};\n' % (ctype, name, max(len(values), 1), ',\n  '.join(rows) or '0')


def main():
    text = KT.read_text(encoding='utf-8')
    out = ['/* Made by scripts/gen_save_tables_c.py from SaveTablesGen.kt: do not edit. Species are internal indexes. */',
           '#pragma once', '#include <stdint.h>', '']
    out.append(ints('ST_TMHM_MOVES', block(text, 'tmHmMoves'), 'uint16_t'))
    pre = block(text, 'preEvolution')
    assert len(pre) == SPECIES, len(pre)
    out.append(ints('ST_PRE_EVOLUTION', pre, 'uint16_t'))
    for game, tag in (('RS', 'RS'), ('E', 'E'), ('FRLG', 'FRLG')):
        level = block(text, 'levelUp' + tag)
        tmhm = block(text, 'tmHm' + tag)
        tutor_moves = block(text, 'tutorMoves' + tag)
        tutors = block(text, 'tutors' + tag)
        assert len(level) == len(tmhm) == len(tutors) == SPECIES, (tag, len(level), len(tmhm), len(tutors))
        flat, start = [], []
        for s in level:
            start.append(len(flat))
            for i in range(0, len(s), 5):
                lv, mv = int(s[i:i + 2], 16), int(s[i + 2:i + 5], 16)
                assert lv < 128 and mv < 512
                flat.append(lv << 9 | mv)
        start.append(len(flat))
        out.append(ints('ST_LEVEL_%s' % game, flat, 'uint16_t'))
        out.append(ints('ST_LEVEL_START_%s' % game, start, 'uint16_t'))
        out.append(ints('ST_TMHM_%s' % game, ['0x%xull' % int(x or '0', 16) for x in tmhm], 'uint64_t', 8))
        out.append(ints('ST_TUTOR_MOVES_%s' % game, tutor_moves, 'uint16_t'))
        out.append('enum { ST_TUTOR_COUNT_%s = %d };\n' % (game, len(tutor_moves)))
        out.append(ints('ST_TUTOR_%s' % game, ['0x%xu' % int(x or '0', 16) for x in tutors], 'uint32_t', 8))
    body = '\n'.join(out)
    if '--check' in sys.argv:
        if not OUT.exists() or OUT.read_text(encoding='utf-8') != body:
            sys.exit('linux/save_tables_gen.h is stale: run scripts/gen_save_tables_c.py')
        return
    OUT.write_text(body, encoding='utf-8', newline='\n')
    print('wrote', OUT.relative_to(ROOT))


if __name__ == '__main__':
    main()
