#!/usr/bin/env python3
"""Decode the retail cannon effect into a compact Vita PCM asset."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from inventory_lzari_assets import ROM_SHA1
from inventory_libmus_assets import parse_effect_bank, parse_pointer_bank, read_file_table

OUTPUT_RATE = 22050
FRAME_BYTES = 9
EFFECT_INDEX = 3
EXPECTED_SAMPLE_COUNT = 20896
EXPECTED_PCM_SHA256 = "63fa7cd4ec5baf22510c3332d60047b36feaf3763f81181fbd27e9aeaa0b844b"


def decode_adpcm(encoded: bytes, coefficients: list[int], predictors: int) -> list[int]:
    if len(encoded) % FRAME_BYTES:
        raise ValueError("cannon ADPCM is not frame-aligned")
    samples: list[int] = []
    state = [0] * 8
    for frame_offset in range(0, len(encoded), FRAME_BYTES):
        header = encoded[frame_offset]
        scale = header >> 4
        predictor = header & 15
        if scale > 12 or predictor >= predictors:
            raise ValueError("cannon ADPCM uses an invalid frame header")
        coefficient_base = predictor * 16
        for half in range(2):
            payload = frame_offset + 1 + half * 4
            residuals = []
            for index in range(8):
                packed = encoded[payload + (index >> 1)]
                nibble = packed >> 4 if index & 1 == 0 else packed & 15
                residuals.append((nibble if nibble < 8 else nibble - 16) << scale)
            older, newer = state[6], state[7]
            for index in range(8):
                accumulator = residuals[index] << 11
                accumulator += coefficients[coefficient_base + index] * older
                accumulator += coefficients[coefficient_base + 8 + index] * newer
                for previous in range(index):
                    accumulator += coefficients[coefficient_base + 8 + previous] * \
                        residuals[index - 1 - previous]
                decoded = accumulator >> 11
                state[index] = max(-32768, min(32767, decoded))
                samples.append(state[index])
    return samples


def cannon_wave_index(effect_data: bytes, effects: dict) -> int:
    entries = effects["effects"]
    start = entries[EFFECT_INDEX]["offset"]
    end = entries[EFFECT_INDEX + 1]["offset"]
    bytecode = effect_data[start:end]
    for index in range(len(bytecode) - 1):
        if bytecode[index] == 0x81:
            return bytecode[index + 1]
    raise ValueError("retail cannon effect has no wave command")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--rom", type=Path, default=Path("baseroms/us/baserom.z64"))
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    actual = hashlib.sha1(rom).hexdigest()
    if actual != ROM_SHA1:
        raise SystemExit(f"base ROM SHA-1 mismatch: expected {ROM_SHA1}, got {actual}")
    files = read_file_table(rom)
    payloads = [rom[item["start"]:item["end"]] for item in files]
    pointer_data, effect_data, wave_data = payloads[0], payloads[1], payloads[2]
    effects = parse_effect_bank(effect_data)
    bank = parse_pointer_bank(pointer_data, wave_data, "SFX")
    wave_index = cannon_wave_index(effect_data, effects)
    wave = bank["waves"][wave_index]
    if wave_index != 3 or wave["type"] != "adpcm" or wave["book"]["order"] != 2:
        raise SystemExit("retail cannon effect/wave layout changed unexpectedly")
    book_offset = wave["book_offset"] + 8
    coefficient_count = wave["book"]["predictors"] * 16
    coefficients = list(struct.unpack_from(f">{coefficient_count}h", pointer_data,
                                           book_offset))
    encoded = wave_data[wave["sample_offset"]:
                        wave["sample_offset"] + wave["sample_size"]]
    samples = decode_adpcm(encoded, coefficients, wave["book"]["predictors"])
    pcm = struct.pack(f"<{len(samples)}h", *samples)
    pcm_sha256 = hashlib.sha256(pcm).hexdigest()
    if len(samples) != EXPECTED_SAMPLE_COUNT or pcm_sha256 != EXPECTED_PCM_SHA256:
        raise SystemExit("decoded cannon PCM disagrees with the verified retail corpus")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "cannon.pcm16le").write_bytes(pcm)
    manifest = {
        "format": "BattleTanx Global Assault Vita audio v1",
        "rom_sha1": actual,
        "effect_index": EFFECT_INDEX,
        "wave_index": wave_index,
        "sample_rate": OUTPUT_RATE,
        "channels": 1,
        "sample_format": "signed PCM16 little-endian",
        "sample_count": len(samples),
        "sha256": pcm_sha256,
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Decoded cannon effect {EFFECT_INDEX}/wave {wave_index}: "
          f"{len(samples)} PCM16 samples at {OUTPUT_RATE} Hz")


if __name__ == "__main__":
    main()
