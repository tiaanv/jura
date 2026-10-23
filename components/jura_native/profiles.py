"""Build-time view of the complete upstream JPRF v4 database (MIT)."""
from functools import lru_cache
from pathlib import Path
import re
import struct
import json


def capability_slots():
    """Upper bound for UTF-8-safe 240-byte chunks, for entity creation at codegen."""
    fields = ('strength', 'grinder', 'temperature', 'water_ml', 'milk_seconds',
              'foam_seconds', 'bypass_ml', 'pause_seconds')
    largest = 0
    for ef, profile in load_profiles().items():
        drinks = {p['name']: {fields[o['attr']]: [c[1] for c in o['items']] if o['items']
                  else {'n': [o['low'], o['high'], max(1, o['step'])]}
                  for o in p['options'] if o['attr'] < len(fields)}
                  for p in profile['products'] if p['flags'] & 1}
        raw = json.dumps({'v': 1, 'p': ef, 'd': drinks}, ensure_ascii=False, separators=(',', ':'))
        largest = max(largest, len(raw.encode('utf-8')))
    return (largest + 236) // 237  # At most three bytes lost at a UTF-8 boundary.


@lru_cache(maxsize=1)
def load_profiles():
    source = (Path(__file__).parent / "jura_profiles_blob.h").read_text()
    data = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})\b", source))
    version, count, pool = struct.unpack_from("<HHI", data, 4)
    if data[:4] != b"JPRF" or version != 4:
        raise ValueError("Unsupported Jura profile database")

    def string(ref):
        begin = pool + ref
        return data[begin:data.index(0, begin)].decode("utf-8")

    out = {}
    for index in range(count):
        ef, family, pos = struct.unpack_from("<HHI", data, 12 + index * 8)
        profile = {"family": string(family), "products": [], "alerts": []}
        n = data[pos]
        pos += 1
        for _ in range(n):
            code, name, flags, n_opts = struct.unpack_from("<BHBB", data, pos)
            pos += 5
            product = {"code": code, "name": string(name), "flags": flags, "options": []}
            for _ in range(n_opts):
                attr, arg, default, low, high, step, n_items = struct.unpack_from("<BBHHHHB", data, pos)
                pos += 11
                items = []
                for _ in range(n_items):
                    value, label = struct.unpack_from("<BH", data, pos)
                    pos += 3
                    items.append((value, string(label)))
                product["options"].append(dict(attr=attr, arg=arg, default=default,
                                                low=low, high=high, step=step, items=items))
            profile["products"].append(product)
        for key in ("maintenance_counts", "maintenance_percentages"):
            n = data[pos]
            pos += 1
            profile[key] = [string(struct.unpack_from("<H", data, pos + i * 2)[0]) for i in range(n)]
            pos += n * 2
        n = struct.unpack_from("<H", data, pos)[0]
        pos += 2
        for _ in range(n):
            bit, name = struct.unpack_from("<HH", data, pos)
            pos += 4
            profile["alerts"].append((bit & 0x7FFF, string(name), bool(bit & 0x8000)))
        out[string(ef)] = profile
    return out
