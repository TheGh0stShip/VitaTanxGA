#!/usr/bin/env sh
set -eu

if [ "$#" -lt 1 ]; then
    echo "usage: $0 <Vita ELF> [linked static library ...]" >&2
    exit 2
fi

: "${VITASDK:?Set VITASDK before running the Vita ABI check.}"
READELF="$VITASDK/bin/arm-vita-eabi-readelf"
ELF="$1"
shift

header="$($READELF -h "$ELF")"
attributes="$($READELF -A "$ELF")"

printf '%s\n' "$header" | grep -q 'Class:.*ELF32'
printf '%s\n' "$header" | grep -q 'Machine:.*ARM'
printf '%s\n' "$attributes" | grep -q 'Tag_CPU_arch: v7'
printf '%s\n' "$attributes" | grep -q 'Tag_ABI_VFP_args: VFP registers'

for library in "$@"; do
    library_attributes="$($READELF -A "$library")"
    if ! printf '%s\n' "$library_attributes" | grep -q 'Tag_ABI_VFP_args: VFP registers'; then
        echo "Vita ABI mismatch: $library is not VFP-register hard-float" >&2
        exit 1
    fi
done

echo "Verified Vita ELF: ELF32 ARMv7-A with VFP-register hard-float ABI"
