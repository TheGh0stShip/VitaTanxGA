# PS Vita port bootstrap

`port/vita` is a standalone ARMv7-A/ILP32 target.  It is intentionally not an
alternative build of the matching N64 program: N64 assembly, libultra calls,
display lists, ROM offsets, and generated extraction output stay out of this
target.  Its initial executable proves the VitaSDK renderer/input path and
provides the platform seam that the portable runtime will use.

## Prerequisites

Install VitaSDK and export its root:

```sh
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
```

The project requires an `arm-vita-eabi-gcc` ARMv7-A build and checks the
resulting ELF and code-bearing linked libraries rather than inferring ABI
compatibility from the Vita CPU. The current SDK baseline is hard-float
(`Tag_ABI_VFP_args: VFP registers`); do not mix softfp objects or libraries
into this target. Import-stub archives contain no machine code and therefore
have no floating-point ABI attribute.

## Build

```sh
make vita
make vita-check
```

The package is written to `port/vita/build/vitatanxga.vpk`. `make vita`
first verifies the supplied ROM and stages decoded LZARI worlds and images,
scripts, and libmus data into the ignored `assets/extracted/us/vita` tree.
Those generated files are packaged under `app0:assets/`; the ROM itself is
never packaged or committed. The executable currently proves the Vita2D
renderer/input path only; wiring these staged assets into the portable runtime
is the next porting task.

`make vita-check` validates that the port ELF is 32-bit ARMv7-A and uses the
same VFP-register calling convention as the installed VitaSDK.  This is a
cross-build gate only; it is not a physical-hardware validation claim.  Do not
install or launch it on hardware unless a supervised hardware-test request is
made.

## Next implementation boundary

Portable game work belongs in a future platform-neutral runtime directory and
will call narrow rendering, audio, input, timing, storage, and threading
interfaces. Preserve N64 serialized addresses and pointer fields as explicit
`uint32_t` tokens at that boundary. Never reinterpret big-endian N64 data
through Vita native pointers, and do not globally pack recovered structs.
