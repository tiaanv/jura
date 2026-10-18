"""Check local Markdown links/anchors and current example local-source paths.

Requires PyYAML, included with the ESPHome development environment.
External URLs are deliberately not fetched. Legacy anchors are left historical.
"""
from collections import Counter
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit
import yaml

root = Path(__file__).resolve().parents[1]
errors = []
excluded = {".git", ".esphome", ".tools", ".venv", "__pycache__", "dist", "generated-configs", "build"}


def without_fences(text):
    lines = []
    fenced = False
    for line in text.splitlines():
        if line.lstrip().startswith(("```", "~~~")):
            fenced = not fenced
            continue
        if not fenced:
            lines.append(line)
    return "\n".join(lines)


def anchors(path):
    counts = Counter()
    result = set()
    for title in re.findall(r"^#{1,6}\s+(.+?)\s*#*\s*$", without_fences(path.read_text(encoding="utf-8")), re.M):
        slug = re.sub(r"[^\w\- ]", "", title.lower()).replace(" ", "-")
        n = counts[slug]
        counts[slug] += 1
        result.add(slug if n == 0 else f"{slug}-{n}")
    return result


checked = 0
for file in root.rglob("*.md"):
    if any(part in excluded for part in file.relative_to(root).parts):
        continue
    content = without_fences(file.read_text(encoding="utf-8"))
    for target in re.findall(r"\]\(([^\n)]+)\)", content):
        target = target.strip().strip("<>")
        if urlsplit(target).scheme or target.startswith("//"):
            continue
        path, _, fragment = target.partition("#")
        resolved = (file.parent / unquote(path)).resolve() if path else file
        checked += 1
        if not resolved.is_relative_to(root) or not resolved.exists():
            errors.append(f"{file.relative_to(root)}: missing local target {target}")
        elif fragment and resolved.suffix == ".md" and "legacy" not in file.relative_to(root).parts:
            if unquote(fragment) not in anchors(resolved):
                errors.append(f"{file.relative_to(root)}: missing anchor {target}")

for file in sorted((root / "examples").glob("*.yaml")):
    for document in yaml.safe_load_all(file.read_text(encoding="utf-8")):
        if not isinstance(document, dict):
            continue
        for component in document.get("external_components", []):
            source = component.get("source", {})
            if isinstance(source, dict) and source.get("type") == "local":
                path = (file.parent / source["path"]).resolve()
                if not path.is_relative_to(root) or not path.is_dir():
                    errors.append(f"{file.relative_to(root)}: invalid local source {source['path']}")

if errors:
    raise SystemExit("\n".join(errors))
print(f"PASS: {checked} local Markdown targets/anchors and current example source paths")
