#!/usr/bin/env python3
"""Decode the retail libmus SFX bank into Vita-ready PCM assets."""

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
EXPECTED_BANK_SHA256 = "ec5c6982a4b4ca33ab325e854eb9087337507715f1baaf48c75ac29169606850"
EXPECTED_INDEX_SHA256 = "f9fd0f4c3deed3e6f8c91c83b0a29106959b1a13d254aa8a864758b861501ccb"
EXPECTED_EFFECT_MAP_SHA256 = "e413404f4086f7014f526d263a9727ce1d7f6f3c6dd5b88f6c5df68f2412c114"
EFFECT_COUNT = 93
NO_WAVE = 0xFF


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


def effect_wave_index(effect_data: bytes, effects: dict, effect_index: int) -> int:
    entries = effects["effects"]
    start = entries[effect_index]["offset"]
    end = (entries[effect_index + 1]["offset"]
           if effect_index + 1 < len(entries) else len(effect_data))
    bytecode = effect_data[start:end]
    for index in range(len(bytecode) - 1):
        if bytecode[index] == 0x81:
            return bytecode[index + 1]
    return NO_WAVE


def decode_wave(pointer_data: bytes, wave_data: bytes, wave: dict) -> list[int]:
    encoded = wave_data[wave["sample_offset"]:
                        wave["sample_offset"] + wave["sample_size"]]
    if wave["type"] == "raw16":
        return list(struct.unpack(f">{len(encoded) // 2}h", encoded))
    if wave["book"]["order"] != 2:
        raise ValueError(f"SFX wave {wave['index']} uses unsupported ADPCM order")
    coefficient_count = wave["book"]["predictors"] * 16
    coefficients = list(struct.unpack_from(
        f">{coefficient_count}h", pointer_data, wave["book_offset"] + 8))
    return decode_adpcm(encoded, coefficients, wave["book"]["predictors"])


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
    effect_waves = bytes(effect_wave_index(effect_data, effects, index)
                         for index in range(EFFECT_COUNT))
    wave_index = effect_waves[EFFECT_INDEX]
    wave = bank["waves"][wave_index]
    if wave_index != 3 or wave["type"] != "adpcm" or wave["book"]["order"] != 2:
        raise SystemExit("retail cannon effect/wave layout changed unexpectedly")
    samples = decode_wave(pointer_data, wave_data, wave)
    pcm = struct.pack(f"<{len(samples)}h", *samples)
    pcm_sha256 = hashlib.sha256(pcm).hexdigest()
    if len(samples) != EXPECTED_SAMPLE_COUNT or pcm_sha256 != EXPECTED_PCM_SHA256:
        raise SystemExit("decoded cannon PCM disagrees with the verified retail corpus")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "cannon.pcm16le").write_bytes(pcm)
    pcm_bank = bytearray()
    index_data = bytearray()
    wave_records = []
    for bank_wave in bank["waves"]:
        wave_samples = decode_wave(pointer_data, wave_data, bank_wave)
        sample_offset = len(pcm_bank) // 2
        pcm_bank.extend(struct.pack(f"<{len(wave_samples)}h", *wave_samples))
        index_data.extend(struct.pack("<II", sample_offset, len(wave_samples)))
        wave_records.append({
            "wave_index": bank_wave["index"],
            "sample_offset": sample_offset,
            "sample_count": len(wave_samples),
        })
    bank_sha256 = hashlib.sha256(pcm_bank).hexdigest()
    index_sha256 = hashlib.sha256(index_data).hexdigest()
    effect_map_sha256 = hashlib.sha256(effect_waves).hexdigest()
    if (bank_sha256 != EXPECTED_BANK_SHA256 or
        index_sha256 != EXPECTED_INDEX_SHA256 or
        effect_map_sha256 != EXPECTED_EFFECT_MAP_SHA256):
        raise SystemExit("decoded SFX bank disagrees with the verified retail corpus")
    (args.output / "sfx_bank.pcm16le").write_bytes(pcm_bank)
    (args.output / "sfx_index.bin").write_bytes(index_data)
    (args.output / "effect_wave_map.bin").write_bytes(effect_waves)
    manifest = {
        "format": "BattleTanx Global Assault Vita audio v2",
        "rom_sha1": actual,
        "effect_index": EFFECT_INDEX,
        "wave_index": wave_index,
        "sample_rate": OUTPUT_RATE,
        "channels": 1,
        "sample_format": "signed PCM16 little-endian",
        "sample_count": len(samples),
        "sha256": pcm_sha256,
        "wave_count": bank["wave_count"],
        "effect_count": len(effect_waves),
        "pcm_bank_sha256": bank_sha256,
        "index_sha256": index_sha256,
        "effect_wave_map_sha256": effect_map_sha256,
        "waves": wave_records,
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Decoded {bank['wave_count']} SFX waves for {len(effect_waves)} effects; "
          f"cannon effect {EFFECT_INDEX}/wave {wave_index} remains verified")


if __name__ == "__main__":
    main()
