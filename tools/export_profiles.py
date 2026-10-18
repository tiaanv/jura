"""Regenerate readable profile exports from the bundled runtime blob."""
import html
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("jura_profiles", root / "components/jura_native/profiles.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
profiles = module.load_profiles()
dest = root / "docs/profiles"
dest.mkdir(parents=True, exist_ok=True)
(dest / "database.json").write_bytes((json.dumps(profiles, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))


def cell(value):
    return html.escape(value).replace("|", "\\|").replace("\n", " ")


lines = ["# Bundled model index", "", f"Generated from the bundled JPRF v4 data: **{len(profiles)} profiles**.",
         "Names are profile data, not evidence of hardware testing. See [profile notes](README.md)",
         "and [the readable export](database.json) for options, limits, defaults and alerts.", "",
         "| EF profile | Family | Named products |", "| --- | --- | --- |"]
for ef, profile in profiles.items():
    products = [p["name"] for p in profile["products"] if p["flags"] & 1]
    lines.append(f"| `{ef}` | {cell(profile['family'] or 'Unspecified')} | {cell(', '.join(products))} |")
(dest / "models.md").write_bytes(("\n".join(lines) + "\n").encode("utf-8"))
print(f"Exported {len(profiles)} profiles to docs/profiles/")
