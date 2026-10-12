#!/usr/bin/env python3
"""Generate an ARMv7-compatible N64Recomp ABI header from the pinned runtime."""

from __future__ import annotations

import argparse
from pathlib import Path


PORTABLE_MULTIPLY = r'''#else

static inline void btga_umul_64x64_128(uint64_t a, uint64_t b,
                                      uint64_t *lo64, uint64_t *hi64) {
    uint64_t a0 = (uint32_t)a, a1 = a >> 32;
    uint64_t b0 = (uint32_t)b, b1 = b >> 32;
    uint64_t p00 = a0 * b0, p01 = a0 * b1;
    uint64_t p10 = a1 * b0, p11 = a1 * b1;
    uint64_t lo = p00;
    uint64_t add = p01 << 32;
    uint64_t carry = (lo += add) < add;
    add = p10 << 32;
    carry += (lo += add) < add;
    *lo64 = lo;
    *hi64 = p11 + (p01 >> 32) + (p10 >> 32) + carry;
}

static inline void DMULTU(uint64_t a, uint64_t b, uint64_t* lo64,
                          uint64_t* hi64) {
    btga_umul_64x64_128(a, b, lo64, hi64);
}

static inline void DMULT(int64_t a, int64_t b, int64_t* lo64, int64_t* hi64) {
    uint64_t lo, hi;
    uint64_t ua = a < 0 ? 0u - (uint64_t)a : (uint64_t)a;
    uint64_t ub = b < 0 ? 0u - (uint64_t)b : (uint64_t)b;
    btga_umul_64x64_128(ua, ub, &lo, &hi);
    if ((a < 0) != (b < 0)) {
        lo = ~lo + 1u;
        hi = ~hi + (lo == 0);
    }
    *lo64 = (int64_t)lo;
    *hi64 = (int64_t)hi;
}

#endif'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.source.read_text()
    needle = '#else\n#error "128-bit integer type not found"\n#endif'
    if source.count(needle) != 1:
        raise SystemExit("pinned recomp.h 128-bit guard changed")
    output = source.replace(needle, PORTABLE_MULTIPLY)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)


if __name__ == "__main__":
    main()
