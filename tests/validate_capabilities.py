"""Cross-check the C++ catalogue against every profile and the codegen slot budget."""
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'components/jura_native'))
from profiles import load_profiles, capability_slots


def validate(binary):
    fields = ('strength', 'grinder', 'temperature', 'water_ml', 'milk_seconds',
              'foam_seconds', 'bypass_ml', 'pause_seconds')
    profiles = load_profiles()
    rows = subprocess.check_output([str(binary)], encoding='utf-8').splitlines()
    assert len(rows) == len(profiles)
    for raw in rows:
        data = json.loads(raw)
        assert data['v'] == 1
        profile = profiles[data['p']]
        expected = {p['name']: {fields[o['attr']]: [c[1] for c in o['items']] if o['items']
                     else {'n': [o['low'], o['high'], max(1, o['step'])]}
                     for o in p['options'] if o['attr'] < len(fields)}
                     for p in profile['products'] if p['flags'] & 1}
        assert data['d'] == expected, data['p']
        assert len(raw.encode('utf-8')) <= capability_slots() * 237, data['p']
    print(f'PASS: C++ catalogue matches all {len(rows)} profiles; {capability_slots()} bounded slots cover every profile')


if __name__ == '__main__':
    validate(Path(sys.argv[1]).resolve())
