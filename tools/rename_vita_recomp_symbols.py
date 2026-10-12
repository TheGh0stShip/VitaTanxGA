#!/usr/bin/env python3
"""Namespace translated guest helpers that collide with ARM EABI libgcc."""

from pathlib import Path
import sys

RENAMES = {
    "__cmpdi2": "btga_guest_cmpdi2",
    "__floatdisf": "btga_guest_floatdisf",
    "__udivmoddi4": "btga_guest_udivmoddi4",
}


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} GENERATED_FUNCTION_DIR")
    root = Path(sys.argv[1])
    paths = [root / "funcs.h", root / "recomp_overlays.inl"]
    paths.extend(sorted(root.glob("funcs_*.c")))
    for path in paths:
        text = path.read_text(encoding="utf-8")
        for source, destination in RENAMES.items():
            text = text.replace(source, destination)
        path.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
