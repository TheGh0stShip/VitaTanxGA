# PS Vita port architecture

The production target is a static recompilation of the complete retail N64
program. `port/recomp/prepare.sh` verifies the user's ROM, runs N64Recomp, and
produces 35 local C translation units containing 1,732 translated bodies. HLE,
patch, stub, and N64-only boundaries cover the rest of the authoritative
1,879-entry catalogue. Those generated files are ignored and are never
committed.

## Target and ABI

The Vita is little-endian ARMv7-A/Thumb with an ILP32 hard-float ABI. The
original program is big-endian MIPS III/R4300 using the 32-bit o32 ABI. Guest
addresses remain 32-bit tokens into bounded RDRAM; they are not native
pointers. Byte and halfword guest accesses preserve N64 word-swapped memory
semantics.

The build checks every code-bearing input and the final executable for ELF32,
ARM, CPU architecture v7, and VFP-register arguments. This verifies ABI
consistency but does not prove correctness on physical hardware.

## Runtime

The translated game is linked to a Vita-focused subset of N64ModernRuntime:

- cooperative N64 threads, scheduling, message queues, events, and timers;
- VI, AI, PI, SP, and DP services;
- translated audio microcode;
- four-player Vita/PSTV controller, analog, external-controller rumble, audio,
  and storage callbacks;
- Controller Pak HLE backed by emulator-compatible 32 KiB `.mpk` files; and
- a static translated-function lookup table with no desktop JIT dependency.

RDRAM is a bounded 8 MiB Expansion Pak allocation. The runtime loads the retail program at
its original guest address and initializes the values normally supplied by
IPL3 before entering the translated cartridge entrypoint. The allocation is
64-byte aligned for ARM cache and NEON access, and renderer conversions reject
matrix, vertex, viewport, light, command, and texture ranges outside RDRAM.

The original AI frequency is converted into Vita's fixed 48 kHz main-output
rate. Buffered-frame reports are converted back to the guest rate so the
retail audio scheduler retains its original timing model. A bounded queue drops
new transition/cutscene audio above 200 ms rather than allowing later effects
to play seconds late.

Compatibility hooks carry fractional host-frame steps through retail integer
countdowns and guard the intro/credits projection against an uninitialized far
plane. Six exact known outer-screen scissor words are corrected in the private
runtime ROM copy to cover 320x240; the verified ROM file is never modified.

## Graphics

The renderer consumes the game's live F3DEX 2.07 display lists using their
second-generation GBI2 command layout. A bridge
validates guest ranges, converts big-endian command payloads and structures,
stages textures safely, and passes the resulting lists to a vitaGL rendering
backend derived from the open-source Fast3D implementation used by
DaedalusX64-vitaGL.

The bootstrap pins and builds vitaGL with the Vita renderer's required
single-threaded, splash-free configuration rather than linking the generic
SDK archive. Runtime-compiled vitaGL shaders are cached under the title's own
`ux0:data/VitaTanxGA/shader_cache` directory.

`runtime.log` records durable first-event milestones for VI delivery, accepted
and rejected display lists, frame presentation, and audio initialization. A
clean shutdown also records totals, allowing black-screen and early-boot faults
to be localized without committing private dumps or hardware evidence.

The renderer remains driven by the original game. It does not package a
hand-selected subset of extracted levels or substitute a separately authored
campaign loop.

## ROM and saves

The VPK contains no ROM data. At runtime the port requires:

```text
ux0:data/VitaTanxGA/baserom.z64
```

The file must be exactly 8 MiB, big-endian `.z64`, and match SHA-1
`805248fb0a0ee694cad8d7dc927b631d860dd8cf`. A mismatch is recorded in
`runtime.log` and rejected before game execution.

The original game stores profiles on Controller Pak rather than a cartridge
save chip. The port persists those files under `ux0:data/VitaTanxGA/saves` as
standard `.mpk` images using explicit on-disk widths and byte order.

## Build and validation

```sh
make vita-recomp-bootstrap  # first checkout only
make vita
```

The resulting package is
`port/vita/build-full-recomp/VitaTanxGA-recomp.vpk`. `make vita-check` is an
alias for the same primary cross-build gate and does not perform redundant
prototype builds.

The older `port/runtime` portable prototype is retained for reference and
loader tests only. It is available through `make vita-legacy`; it is not the
shipping port architecture.

Do not install or launch on physical hardware without an explicitly supervised
hardware-test request. Emulator behavior is useful development evidence but is
not a physical-Vita release gate.
