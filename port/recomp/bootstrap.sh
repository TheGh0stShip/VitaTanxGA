#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
recomp_root="$root/.deps/N64Recomp"
runtime_root="$root/.deps/N64ModernRuntime"
vitagl_root="$root/.deps/vitaGL"
recomp_commit=ffb39cdad1da5de07eaaa48bd1db4a89a7986771
runtime_commit=cdf5abbd5026fef5c364c676e4667c45e42b6863
vitagl_commit=73dd57a8857f89f2353881c6de5891959c5c1983

if [[ ! -d "$recomp_root/.git" ]]; then
    mkdir -p "$root/.deps"
    git clone --recursive https://github.com/N64Recomp/N64Recomp.git "$recomp_root"
fi
git -C "$recomp_root" fetch --depth 1 origin "$recomp_commit"
git -C "$recomp_root" checkout --detach "$recomp_commit"
git -C "$recomp_root" submodule update --init --recursive
cmake -S "$recomp_root" -B "$recomp_root/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$recomp_root/build" --target N64RecompCLI RSPRecomp

if [[ ! -d "$runtime_root/.git" ]]; then
    git clone https://github.com/N64Recomp/N64ModernRuntime.git "$runtime_root"
fi
git -C "$runtime_root" fetch --depth 1 origin "$runtime_commit"
git -C "$runtime_root" checkout --detach "$runtime_commit"
git -C "$runtime_root" submodule update --init --recursive
git -C "$runtime_root" reset --hard "$runtime_commit"
git -C "$runtime_root" apply "$root/port/recomp/patches/n64modernruntime-vita.patch"

# Fast3D needs the same Vita-oriented vitaGL feature set used by the proven
# Daedalus renderer.  In particular, the generic VitaSDK archive enables a
# threaded library splash screen before the game runtime starts.  Keep this
# private dependency pinned, hard-float, single-threaded, and splash-free.
if [[ ! -d "$vitagl_root/.git" ]]; then
    git clone https://github.com/Rinnegatamante/vitaGL.git "$vitagl_root"
fi
git -C "$vitagl_root" fetch --depth 1 origin "$vitagl_commit"
git -C "$vitagl_root" checkout --detach "$vitagl_commit"
make -C "$vitagl_root" clean
make -C "$vitagl_root" -j"$(getconf _NPROCESSORS_ONLN)" \
    READBACKS_SPEEDHACK=1 HAVE_GLSL_TEXTURE_SIZE=1 NO_DEBUG=1 \
    HAVE_UNFLIPPED_FBOS=1 NO_TEX_COMBINER=1 HAVE_CUSTOM_HEAP=1 \
    SINGLE_THREADED_GC=1 HAVE_FFP_SHADER_SUPPORT=1 \
    DEPTH_STENCIL_HACK=1 CIRCULAR_POOL_SPEEDHACK=1 \
    ENABLE_LEGACY_PIPELINE=1 NO_SPLASHSCREEN=1 HAVE_SHADER_CACHE=1
