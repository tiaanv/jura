"""Package source, examples, documentation and reproducible host tests."""
import hashlib
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
files = [p for p in root.iterdir() if p.is_file() and p.suffix in (".txt", ".json", ".yaml")]
files += [root / "VERSION", root / "LICENSE", root / ".gitignore"]
files += [p for p in (root / "components").rglob("*") if p.is_file() and "__pycache__" not in p.parts]
files += [p for p in (root / "tests/esphome").rglob("*.h")]
files += [root / "tests" / name for name in (
    "test_controller.cpp", "test_native.cpp", "pgmspace.h", "run_tests.py",
    "run_esphome.py", "validate_schema.py", "export_profiles.py", "package_release.py")]
files = sorted(set(files), key=lambda p: p.relative_to(root).as_posix())
manifest = root / "SHA256SUMS"
manifest.write_text("".join(hashlib.sha256(p.read_bytes()).hexdigest() + "  " +
                            p.relative_to(root).as_posix() + "\n" for p in files), encoding="utf-8")
files.append(manifest)
dest = root.parent / "jura-native-release-0.1.0.zip"
with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as archive:
    for file in files:
        archive.write(file, "jura-native/" + file.relative_to(root).as_posix())
with zipfile.ZipFile(dest) as archive:
    assert archive.testzip() is None
    assert not any(".esphome/" in p or "/.tools/" in p or p.endswith((".exe", ".pdb", ".rsp")) for p in archive.namelist())
print(f"Verified {len(files)} files; {dest.name}: {dest.stat().st_size:,} bytes")
