#pragma once

#include <cstdint>

#define U8_TWIDDLE 3
#define U16_TWIDDLE 2
#define U16H_TWIDDLE 1
#define BSWAP32(value) __builtin_bswap32(value)
#define BSWAP16(value) __builtin_bswap16(value)
