#include <cstdint>
#include <cstdlib>

#include "runtime_compat.h"
#include "ultramodern/ultra64.h"
#include "vita_recomp_diagnostics.hpp"

extern "C" void __osGetSR_recomp(std::uint8_t*, recomp_context* ctx) {
    ctx->r2 = cop0_status_read(ctx);
}

extern "C" void osDestroyThread_801058B0(std::uint8_t* rdram,
                                           recomp_context* ctx) {
    osDestroyThread(rdram, static_cast<PTR(OSThread)>(ctx->r4));
}

#define BTGA_UNSUPPORTED_RECOMP(name)                                      \
    extern "C" void name(std::uint8_t*, recomp_context*) {                 \
        btga_vita_log("unsupported translated libultra internal: " #name); \
        std::abort();                                                       \
    }

BTGA_UNSUPPORTED_RECOMP(__osCheckPackId_recomp)
BTGA_UNSUPPORTED_RECOMP(__osContAddressCrc_recomp)
BTGA_UNSUPPORTED_RECOMP(__osContRamRead_recomp)
BTGA_UNSUPPORTED_RECOMP(__osContRamWrite_recomp)
BTGA_UNSUPPORTED_RECOMP(__osDequeueThread_recomp)
BTGA_UNSUPPORTED_RECOMP(__osDispatchThread_recomp)
BTGA_UNSUPPORTED_RECOMP(__osPfsRWInode_recomp)
BTGA_UNSUPPORTED_RECOMP(__osPfsSelectBank_recomp)
BTGA_UNSUPPORTED_RECOMP(__osRepairPackId_recomp)
BTGA_UNSUPPORTED_RECOMP(__osSiCreateAccessQueue_recomp)

#undef BTGA_UNSUPPORTED_RECOMP
