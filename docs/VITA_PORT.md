# PS Vita port

`port/vita` is a standalone ARMv7-A/ILP32 target.  It is intentionally not an
alternative build of the matching N64 program: N64 assembly, libultra calls,
display lists, ROM offsets, and generated extraction output stay out of this
target. The target drives the platform-neutral runtime under `port/runtime`.

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
the shared texture/state/geometry pools, scripts, libmus data, and original
fixed-width campaign text table into the ignored `assets/extracted/us/vita`
tree.
Those generated files are packaged under `app0:assets/`; the ROM itself is
never packaged or committed. The executable runs a bounded 30 Hz fixed-step
simulation, maps Vita controls to portable button/analog state, and lets the
runtime select decoded packaged assets without depending on Vita APIs. It
also reads the first world's split component files, decodes their big-endian
fields explicitly, validates table and pool ranges, interprets the referenced
F3DEX2 vertex/triangle command streams, and traverses the recovered
placement/model/part/reference graph. It decodes the paired state and texture
commands, uploads native Vita textures, and renders the placed scene with a
37-degree perspective camera and GXM depth testing. Portable gameplay state
currently covers tank movement, terrain following, turret aim, projectiles,
ammunition, the recovered three-allied/five-opposing first-mission formation,
team-aware pursuit and firing, damage, enemy-only victory evaluation,
repeatable mission resets, and title/gameplay/pause
transitions. Tank body, turret, tread, and color-detail layers are resolved
through the original common-world model-ID table and rendered from retail
geometry, materials, and textures. The companion world is also decoded for
entry type 7 placements; all 41 first-mission pickups resolve through the
retail 20-entry gameplay model table, load their common-world textures, and
render with the original rotation behavior. Their recovered mode handler now
applies the retail quantities for health, base ammunition, swarmers, lasers,
guided missiles, gun buddies, nukes, mines, teleporters, plasma bolts, turbo,
shields, bouncing betties, flamethrower, cloaking, grenades, tank bucks, and
radar. Collected models leave the scene, HUD feedback names the item, and a
mission restart restores the full placement set and clears its inventory.

The frontend executes the recovered opening narrative and then displays the
retail mission-zero location, objective, and complete briefing through the
portable campaign-table reader before deploying the player. The same recovered
objective remains visible during gameplay.

The portable script loader validates the first campaign script header and
all 41 split streams using the retail command-width rules. The full 23,483
command corpus is checked for bounded operands, terminated strings, per-stream
terminators, and stream-type-dependent command sizes before the Vita runtime
marks mission assets ready. A bounded portable cursor now executes the
recovered type-51 opening stream, including its seven text events, color and
position state, original 30 Hz waits, completion, and player-driven advance or
skip. The opening runs between the title and gameplay without copying or
rewriting the extracted strings. Execution of world-object, camera, and combat
mission streams remains to be connected to their runtime consumers.

`make vita-check` tests the portable loaders against synthetic malformed
inputs and all 1,287 geometry/state references plus all 1,285 textured
references in the extracted first-level data,
The gate additionally validates all 23,483 commands in the extracted first
campaign script and executes the opening stream to its terminator while
requiring all seven retail text events. It also checks the extracted campaign
table against the full first briefing, then validates that the port ELF is
32-bit ARMv7-A and uses the same VFP-register calling convention as the
installed VitaSDK. This is a cross-build gate only; it is not a
physical-hardware validation claim. Do not install or launch it on hardware
unless a supervised hardware-test request is made.

## Runtime boundary

Portable game state belongs under `port/runtime`; Vita APIs stay under
`port/vita`. The runtime owns simulation, gameplay, scene decoding, materials,
textures, terrain, and view transforms. Audio, storage, and threading will
cross similarly narrow platform interfaces as their gameplay consumers are
ported. Preserve N64 serialized
addresses and pointer fields as explicit `uint32_t` tokens at that boundary.
Never reinterpret big-endian N64 data through Vita native pointers, and do not
globally pack recovered structs.
