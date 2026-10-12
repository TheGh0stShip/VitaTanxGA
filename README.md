# VitaTanxGA

VitaTanxGA is an in-progress PlayStation Vita port of **BattleTanx: Global
Assault**. It uses the recovered N64 source from
[BattleTanx-Global-Assault](https://github.com/TheGh0stShip/BattleTanx-Global-Assault)
as its reference foundation while keeping Vita portability work isolated from
the matching-decompilation project.

## Current status

The repository currently produces an installable PS Vita VPK that:

- targets 32-bit ARMv7-A/Thumb with the VitaSDK hard-float ABI;
- initializes Vita2D and controller input;
- loads and displays a decoded game image from `app0:assets`;
- packages decoded worlds, images, scripts, and libmus audio resources; and
- validates the executable and code-bearing libraries for ABI consistency.

This is a **port bootstrap**, not yet a playable version of the game. Recovered
gameplay, rendering, audio, menus, saving, and platform services still need to
be connected to the portable runtime.

## Requirements

- A legally obtained USA Rev 0 BattleTanx: Global Assault ROM
- [VitaSDK](https://vitasdk.org/), including Vita2D
- CMake and Python 3

The supported ROM is an 8 MiB big-endian `.z64` dump with SHA-1:

```text
805248fb0a0ee694cad8d7dc927b631d860dd8cf
```

ROMs, extracted resources, generated binaries, and hardware evidence must not
be committed to this repository.

## Build

Export VitaSDK and place the verified ROM at `baseroms/us/baserom.z64`:

```sh
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
make vita-check
```

`make vita-check` performs the complete local pipeline:

1. verifies the ROM;
2. extracts decoded game resources into the ignored
   `assets/extracted/us/vita` directory;
3. cross-compiles the Vita executable;
4. packages the extracted resources into the VPK; and
5. verifies ELF32 ARMv7-A and hard-float attributes.

The resulting package is:

```text
port/vita/build/vitatanxga.vpk
```

See [docs/VITA_PORT.md](docs/VITA_PORT.md) for architecture and packaging
details.

## Architecture rules

- Original N64 code uses big-endian MIPS III with a 32-bit o32 ABI.
- PS Vita uses little-endian ARMv7-A with an ILP32 ABI. It is not AArch64.
- ROM addresses and serialized pointers remain explicit 32-bit tokens.
- Big-endian resource data must be decoded explicitly rather than cast through
  native Vita structures.
- Vita-specific APIs remain under `port/vita`; matching N64 source remains a
  reference and evidence boundary.
- Emulator or host success does not prove physical Vita correctness.

No package should be described as hardware-validated until it has completed a
supervised test on a physical PS Vita.

## License

Project-authored code and imported GPL research are licensed under GPL-3.0;
see [COPYING](COPYING). BattleTanx: Global Assault and its game data remain the
property of their respective rights holders.
