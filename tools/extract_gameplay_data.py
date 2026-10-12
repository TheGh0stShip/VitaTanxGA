#!/usr/bin/env python3
"""Extract the retail gameplay-definition data used by the portable runtime."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from inventory_lzari_assets import ROM_SHA1

ROM_START = 0xB2384
ROM_END = 0xB3E40
N64_VRAM = 0x80122384
PROJECTILE_TEXTURE_STARTS = (0x102068, 0x102468, 0x102868)
PROJECTILE_TEXTURE_SIZE = 0x400


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--rom", type=Path, default=Path("baseroms/us/baserom.z64"))
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    actual = hashlib.sha1(rom).hexdigest()
    if actual != ROM_SHA1:
        raise SystemExit(f"base ROM SHA-1 mismatch: expected {ROM_SHA1}, got {actual}")
    data = rom[ROM_START:ROM_END]
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "gameplay.bin").write_bytes(data)
    projectile_textures = []
    for index, start in enumerate(PROJECTILE_TEXTURE_STARTS):
        texture = rom[start:start + PROJECTILE_TEXTURE_SIZE]
        name = f"projectile_{index}.i8"
        (args.output / name).write_bytes(texture)
        projectile_textures.append({
            "index": index,
            "rom_start": start,
            "size": len(texture),
            "width": 32,
            "height": 32,
            "format": "N64 I8",
            "path": name,
            "sha256": hashlib.sha256(texture).hexdigest(),
        })
    manifest = {
        "format": "BattleTanx Global Assault gameplay definitions v1",
        "rom_sha1": actual,
        "rom_start": ROM_START,
        "rom_end": ROM_END,
        "n64_vram": N64_VRAM,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "projectile_textures": projectile_textures,
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Extracted {len(data)} gameplay-definition bytes to {args.output}")


if __name__ == "__main__":
    main()
