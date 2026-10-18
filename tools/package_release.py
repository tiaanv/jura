"""Package the documented repository layout and regenerate SHA256SUMS.

Includes the legacy archive as historical reference. Excludes local runtimes,
Python caches, generated firmware, test binaries, secrets and previous archives.
"""
import hashlib
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
excluded = {".git", ".esphome", ".tools", ".venv", "__pycache__", "build", "generated-configs", "dist"}
extensions = {".py", ".h", ".cpp", ".yaml", ".md", ".json"}
files = [root / name for name in ("README.md", "CHANGELOG.md", "LICENSE", "VERSION", ".gitignore")]
for directory in ("components", "docs", "examples", "tests", "tools", "legacy"):
    for file in (root / directory).rglob("*"):
        relative = file.relative_to(root)
        if (file.is_file() and not any(part in excluded for part in relative.parts)
                and (file.suffix in extensions or file.name.startswith("LICENSE"))
                and file.name != "secrets.yaml"):
            files.append(file)
files = sorted(set(files), key=lambda p: p.relative_to(root).as_posix())
manifest = root / "SHA256SUMS"
manifest.write_bytes("".join(hashlib.sha256(p.read_bytes()).hexdigest() + "  " +
                              p.relative_to(root).as_posix() + "\n" for p in files).encode("utf-8"))
files.append(manifest)
version = (root / "VERSION").read_text().strip()
dest = root / "dist" / f"jura-native-{version}.zip"
dest.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as archive:
    for file in files:
        archive.write(file, "jura-native/" + file.relative_to(root).as_posix())
with zipfile.ZipFile(dest) as archive:
    assert archive.testzip() is None
    names = archive.namelist()
    assert not any(any(part in excluded for part in Path(name).parts) for name in names)
    assert not any(name.endswith((".exe", ".pdb", ".lib", ".rsp", ".pyc", ".zip", "/secrets.yaml")) for name in names)
    for line in archive.read("jura-native/SHA256SUMS").decode("utf-8").splitlines():
        digest, path = line.split("  ", 1)
        assert hashlib.sha256(archive.read("jura-native/" + path)).hexdigest() == digest
print(f"PASS: {len(files)} source/doc files, checksums and ZIP integrity: {dest.relative_to(root)}")
