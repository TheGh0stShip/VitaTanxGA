#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
recomp="$root/.deps/N64Recomp/build/N64Recomp"
rsp_recomp="$root/.deps/N64Recomp/build/RSPRecomp"
runtime_header="$root/.deps/N64ModernRuntime/N64Recomp/include/recomp.h"
rom="$root/baseroms/us/baserom.z64"

python3 "$root/tools/verify_rom.py" "$rom"
[[ -x "$recomp" ]] || { echo "Run port/recomp/bootstrap.sh first" >&2; exit 1; }
[[ -x "$rsp_recomp" ]] || { echo "RSPRecomp is missing" >&2; exit 1; }
[[ -f "$runtime_header" ]] || { echo "N64ModernRuntime is missing" >&2; exit 1; }

cd "$root/port/recomp"
mkdir -p input generated
ln -sfn "$root/config/vita/recomp_function_boundaries.toml" input/functions.toml
ln -sfn "$rom" input/battletanx_ga.z64
rm -rf generated/functions generated/battletanx_ga.bin generated/n_aspMain.cpp
"$recomp" btga.toml
"$rsp_recomp" n_aspMain.toml
python3 "$root/tools/make_vita_recomp_header.py" "$runtime_header" \
    generated/include/recomp.h
python3 "$root/tools/rename_vita_recomp_symbols.py" generated/functions

count="$(find generated/functions -name 'funcs_*.c' -type f | wc -l)"
[[ "$count" -eq 35 ]] || {
    echo "Expected 35 translated game units, generated $count" >&2
    exit 1
}
echo "Generated the complete 1,879-function game in 35 units and audio microcode locally"
