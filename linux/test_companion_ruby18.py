"""pc_companion.rb runs inside every fan game's player, Ruby 1.8 included (XP games on a borrowed player): a construct
a newer Ruby added stops such a game before its title screen (1.65.6 shipped %i[] and broke Africanvs). Lists the
lines that use one."""
import re
import sys
from pathlib import Path

BANNED = [
    (r'%[iI][\[({<]', '%i[] (Ruby 2.0)'),
    (r'\bsingleton_class\b', 'singleton_class (1.9.2): use (class << X; self; end)'),
    (r'\bFile\.(binread|binwrite|write)\b', 'File.binread, binwrite or write (1.9): use File.open with a block'),
    (r'&\.', 'safe navigation &. (2.3)'),
    (r'->\s*[({]', 'a stabby lambda (1.9)'),
    (r'\.then\b', '.then (2.6)'),
    (r'\bprepend\b', 'prepend (2.0)'),
    (r'[({,]\s*[a-z_]\w*: [^:]', 'a 1.9 style hash key or keyword argument'),
]
STRING = re.compile(r"'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"")


def main():
    path = Path(__file__).with_name('pc_companion.rb')
    bad = 0
    for n, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        code = STRING.sub('""', line)
        if code.lstrip().startswith('#'):
            continue
        code = re.sub(r'\s#.*$', '', code)
        for pat, what in BANNED:
            if re.search(pat, code):
                print(f'pc_companion.rb:{n}: {what}: {line.strip()}')
                bad += 1
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
