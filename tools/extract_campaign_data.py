#!/usr/bin/env python3
"""Extract the retail campaign text table for the portable runtime."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from inventory_lzari_assets import ROM_SHA1

TEXT_ROM_START = 0xB3E40
TEXT_ROM_END = 0xB4B30
CONFIG_ROM_START = 0xB4B80
CONFIG_RECORD_SIZE = 0x70
CONFIG_COUNT = 19
CONFIG_ROM_END = CONFIG_ROM_START + CONFIG_RECORD_SIZE * CONFIG_COUNT


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--rom", type=Path, default=Path("baseroms/us/baserom.z64"))
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    actual = hashlib.sha1(rom).hexdigest()
    if actual != ROM_SHA1:
        raise SystemExit(f"base ROM SHA-1 mismatch: expected {ROM_SHA1}, got {actual}")
    data = rom[TEXT_ROM_START:TEXT_ROM_END]
    config = rom[CONFIG_ROM_START:CONFIG_ROM_END]
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "campaign_text.bin").write_bytes(data)
    (args.output / "campaign_config.bin").write_bytes(config)
    manifest = {
        "format": "BattleTanx Global Assault campaign text v1",
        "rom_sha1": actual,
        "rom_start": TEXT_ROM_START,
        "rom_end": TEXT_ROM_END,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "location_count": 24,
        "objective_count": 26,
        "briefing_count": 19,
        "config_rom_start": CONFIG_ROM_START,
        "config_rom_end": CONFIG_ROM_END,
        "config_record_size": CONFIG_RECORD_SIZE,
        "config_count": CONFIG_COUNT,
        "config_sha256": hashlib.sha256(config).hexdigest(),
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Extracted {len(data)} campaign-text bytes to {args.output}")


if __name__ == "__main__":
    main()
