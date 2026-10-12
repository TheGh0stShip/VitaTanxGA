# VitaTanxGA

VitaTanxGA is the PlayStation Vita port of **BattleTanx: Global Assault**. It
translates the retail N64 program to C and compiles the complete game for the
Vita rather than recreating individual scenes in a new engine. Portability
work lives in this repository and remains separate from the matching
decompilation project.

## Status

The primary VPK currently includes:

- all 1,879 translated retail functions across 35 generated translation units;
- N64 scheduler, threads, message queues, timers, controller, PI, AI, VI, SP,
  DP, and audio-microcode services;
- a live F3DEX 2.07 display-list renderer backed by vitaGL;
- Vita controls, analog input, rumble, and threaded stereo audio;
- Controller Pak profile storage in standard 32 KiB `.mpk` images;
- strict runtime identification of the supported ROM; and
- ARMv7-A hard-float ELF validation for the executable and linked libraries.

The application is a complete-game runtime target, but it is not yet described
as a finished release: full campaign runtime validation and supervised testing
on physical Vita hardware remain outstanding. Emulator results do not establish
physical-hardware correctness.

An older portable gameplay prototype remains in the tree for reference and
targeted loader tests. It is not the main port and is built only with
`make vita-legacy`.

## Requirements

- A legally obtained USA Rev 0 BattleTanx: Global Assault ROM
- [VitaSDK](https://vitasdk.org/) with vitaGL
- CMake and Python 3

The supported ROM is an 8 MiB big-endian `.z64` dump with SHA-1:

```text
805248fb0a0ee694cad8d7dc927b631d860dd8cf
```

ROMs, extracted resources, generated translation sources, VPKs, crash dumps,
and private hardware evidence must not be committed.

## Build

Place the verified ROM at `baseroms/us/baserom.z64`, export VitaSDK, and build:

```sh
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
make vita-recomp-bootstrap  # first checkout only
make vita
```

`make vita` verifies the ROM, generates the complete translation locally,
cross-compiles the game and runtime, links the Vita application, packages the
VPK, and checks ELF32 ARMv7 hard-float attributes. The output is:

```text
port/vita/build-full-recomp/VitaTanxGA-recomp.vpk
```

The ROM is never embedded in the VPK. Copy the same verified ROM to the target:

```text
ux0:data/VitaTanxGA/baserom.z64
```

Controller Pak profiles and `runtime.log` are written beside it. An absent or
incorrect ROM is rejected before the translated game starts.

## Architecture

- Original game: big-endian MIPS III/R4300, 32-bit o32 ABI.
- Vita: little-endian ARMv7-A Cortex-A9, 32-bit ARM/Thumb, ILP32 hard-float ABI.
- ROM, disk, checksums, and guest pointers retain explicit 32-bit and endian
  semantics; live host pointers are never truncated into guest tokens.
- Host and emulator success are development evidence only. A Vita release gate
  requires a clean cross-build and supervised physical-hardware testing.

See [docs/VITA_PORT.md](docs/VITA_PORT.md) for implementation details.

## License

Project-authored code and imported GPL research are licensed under GPL-3.0; see
[COPYING](COPYING). BattleTanx: Global Assault and its game data remain the
property of their respective rights holders.
