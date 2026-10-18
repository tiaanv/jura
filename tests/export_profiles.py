"""Export the bundled blob to readable JSON and a model/product index."""
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("jura_profiles", root / "components/jura_native/profiles.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
profiles = module.load_profiles()
(root / "PROFILE_DATABASE.json").write_text(json.dumps(profiles, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
lines = ["Bundled Jura profile reference — 75 profiles", "===========================================", "",
         "Names below come from the upstream profile data, not from tested hardware.",
         "PROFILE_DATABASE.json includes parameter limits, choices, defaults and alerts.", ""]
for ef, profile in profiles.items():
    products = [p["name"] for p in profile["products"] if p["flags"] & 1]
    lines.extend([f"{ef} — {profile['family']}", "  " + ", ".join(products), ""])
(root / "PROFILE_REFERENCE.txt").write_text("\n".join(lines), encoding="utf-8")
print(f"Exported {len(profiles)} profiles")
