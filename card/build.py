"""Build the standalone dashboard card. No npm/runtime dependencies required."""
import argparse
import json
from pathlib import Path

CARD = Path(__file__).resolve().parent
ROOT = CARD.parent


def build():
    source = (CARD / 'src/jura-native-card.js').read_text(encoding='utf-8')
    for token, name in [('/*GRAPHIC*/', 'machine.svg'), ('/*STYLES*/', 'jura-native-card.css')]:
        assert source.count(token) == 1, token
        source = source.replace(token, json.dumps((CARD / 'src' / name).read_text(encoding='utf-8')))
    version = (CARD / 'VERSION').read_text().strip()
    assert source.startswith('// Jura Native Graphical Card ' + version + ' ')
    return source


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Fail if dist differs from source')
    args = parser.parse_args()
    target = ROOT / 'dist/jura-native-card.js'
    result = build()
    if args.check:
        assert target.read_text(encoding='utf-8') == result, 'Run python card/build.py and commit dist'
        print('PASS standalone card matches source')
    else:
        target.parent.mkdir(exist_ok=True)
        target.write_text(result, encoding='utf-8', newline='\n')
        print('Built dist/jura-native-card.js')
