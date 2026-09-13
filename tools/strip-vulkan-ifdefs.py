#!/usr/bin/env python3
"""Remove SCWX_RENDER_BACKEND_VULKAN preprocessor branches (Vulkan-only tree)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

IF_VULKAN = re.compile(r"^\s*#if\s+defined\(SCWX_RENDER_BACKEND_VULKAN\)\s*$")
IF_NOT_VULKAN = re.compile(r"^\s*#if\s+!defined\(SCWX_RENDER_BACKEND_VULKAN\)\s*$")
ELSE = re.compile(r"^\s*#else\s*$")
ENDIF = re.compile(r"^\s*#endif\b")


def process(lines: list[str]) -> list[str]:
    out: list[str] = []
    # stack entries: "keep" | "skip"
    stack: list[str] = []

    for line in lines:
        if IF_VULKAN.match(line):
            stack.append("keep")
            continue
        if IF_NOT_VULKAN.match(line):
            stack.append("skip")
            continue
        if ELSE.match(line):
            if not stack:
                out.append(line)
                continue
            stack[-1] = "skip" if stack[-1] == "keep" else "keep"
            continue
        if ENDIF.match(line):
            if stack:
                stack.pop()
                continue
            out.append(line)
            continue
        if not stack or stack[-1] == "keep":
            out.append(line)

    return out


def main() -> int:
    root = Path(__file__).resolve().parents[1] / "scwx-qt"
    changed = 0
    for path in sorted(root.rglob("*")):
        if path.suffix not in {".cpp", ".hpp", ".h"}:
            continue
        text = path.read_text(encoding="utf-8")
        if "SCWX_RENDER_BACKEND_VULKAN" not in text:
            continue
        new_lines = process(text.splitlines(keepends=True))
        new_text = "".join(new_lines)
        if new_text != text:
            path.write_text(new_text, encoding="utf-8")
            changed += 1
            print(path.relative_to(root.parent))
    print(f"updated {changed} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
