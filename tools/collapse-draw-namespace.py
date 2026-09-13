#!/usr/bin/env python3
from pathlib import Path
import re

DRAW_DIR = Path(__file__).resolve().parents[1] / "scwx-qt" / "source" / "scwx" / "qt" / "draw"

START = re.compile(
    r"namespace gl\s*\{\s*\n\s*namespace draw\s*\{", re.MULTILINE
)
END = re.compile(r"\}\s*//\s*namespace draw\s*\n\}\s*//\s*namespace gl", re.MULTILINE)


def fix(text: str) -> str:
    text = START.sub("namespace draw\n{", text)
    text = END.sub("} // namespace draw", text)
    return text


for path in sorted(DRAW_DIR.glob("*")):
    if path.suffix not in {".cpp", ".hpp"}:
        continue
    original = path.read_text(encoding="utf-8")
    updated = fix(original)
    if updated != original:
        path.write_text(updated, encoding="utf-8")
        print("fixed", path.name)
