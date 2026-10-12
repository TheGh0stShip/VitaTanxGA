#!/usr/bin/env python3
"""Extract the three shared world resource pools from a verified retail ROM."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from inventory_lzari_assets import ROM_SHA1


POOLS = (
    ("texture", 0x102C70, 0x2F8070),
    ("state", 0x2F8070, 0x3013F0),
    ("geometry", 0x3013F0, 0x3F6EE8),
)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--rom", type=Path, default=Path("baseroms/us/baserom.z64"))
    args = parser.parse_args()

    rom = args.rom.read_bytes()
    actual_sha1 = hashlib.sha1(rom).hexdigest()
    if actual_sha1 != ROM_SHA1:
        raise SystemExit(f"base ROM SHA-1 mismatch: expected {ROM_SHA1}, got {actual_sha1}")
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, start, end in POOLS:
        payload = rom[start:end]
        (args.output / f"{name}.bin").write_bytes(payload)
        manifest.append(
            {
                "name": name,
                "rom_start": start,
                "rom_end": end,
                "size": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
        )
    document = {
        "format": "BattleTanx Global Assault world pools v1",
        "rom_sha1": actual_sha1,
        "pools": manifest,
    }
    (args.output / "manifest.json").write_text(json.dumps(document, indent=2) + "\n")
    print(f"Extracted {len(POOLS)} world pools to {args.output}")


if __name__ == "__main__":
    main()
