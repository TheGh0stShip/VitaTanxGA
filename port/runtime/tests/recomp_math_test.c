#include "recomp.h"

#include <assert.h>
#include <stdint.h>

static void test_unsigned(uint64_t a, uint64_t b, uint64_t expected_lo,
                          uint64_t expected_hi) {
    uint64_t lo, hi;
    DMULTU(a, b, &lo, &hi);
    assert(lo == expected_lo && hi == expected_hi);
}

static void test_signed(int64_t a, int64_t b, uint64_t expected_lo,
                        uint64_t expected_hi) {
    int64_t lo, hi;
    DMULT(a, b, &lo, &hi);
    assert((uint64_t)lo == expected_lo && (uint64_t)hi == expected_hi);
}

int main(void) {
    test_unsigned(UINT64_MAX, UINT64_MAX, UINT64_C(0x1),
                  UINT64_C(0xFFFFFFFFFFFFFFFE));
    test_unsigned(UINT64_C(0x123456789ABCDEF0),
                  UINT64_C(0xFEDCBA9876543210),
                  UINT64_C(0x236D88FE5618CF00),
                  UINT64_C(0x121FA00AD77D7422));
    test_unsigned(UINT64_C(0x8000000000000000), UINT64_C(2), 0, 1);
    test_signed(-1, 2, UINT64_C(0xFFFFFFFFFFFFFFFE), UINT64_MAX);
    test_signed(INT64_MIN, -1, UINT64_C(0x8000000000000000), 0);
    test_signed(-INT64_C(0x123456789ABCDEF),
                INT64_C(0x102030405060708),
                UINT64_C(0xA4EB663781660788),
                UINT64_C(0xFFFEDA709F443D68));
    return 0;
}
