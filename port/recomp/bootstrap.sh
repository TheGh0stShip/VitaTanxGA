#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
recomp_root="$root/.deps/N64Recomp"
runtime_root="$root/.deps/N64ModernRuntime"
recomp_commit=ffb39cdad1da5de07eaaa48bd1db4a89a7986771
runtime_commit=cdf5abbd5026fef5c364c676e4667c45e42b6863

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
