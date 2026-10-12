#pragma once

#include <cstddef>
#include <cstdint>

namespace btga::vita {

// BattleTanx: Global Assault requires the N64 Expansion Pak.  Guest virtual
// addresses are translated into this 8 MiB RDRAM image; this is not a general
// host heap and must not be inflated to mask an invalid guest address.
inline constexpr std::size_t kRdramSize = 8U * 1024U * 1024U;
inline constexpr std::uint32_t kRdramSize32 =
    static_cast<std::uint32_t>(kRdramSize);

static_assert(sizeof(void*) == 4, "The Vita application requires an ILP32 ABI");
static_assert(kRdramSize == 0x00800000U);

} // namespace btga::vita
