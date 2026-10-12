#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <string>

#include "librecomp/game.hpp"
#include "recomp.h"
#include "ultramodern/ultramodern.hpp"
#include "vita_recomp_diagnostics.hpp"

std::filesystem::path config_path{"ux0:data/VitaTanxGA"};
std::atomic_bool exited{false};
moodycamel::LightweightSemaphore graphics_shutdown_ready;

namespace {
constexpr std::uint32_t kStatusFr = 0x04000000U;
const std::u8string kGameId = u8"battletanx-ga.n64.us.1.0";
}

extern "C" void cop0_status_write(recomp_context* ctx, gpr value) {
    const std::uint32_t old_status = ctx->status_reg;
    const std::uint32_t new_status = static_cast<std::uint32_t>(value);
    const std::uint32_t changed = old_status ^ new_status;
    if ((changed & kStatusFr) != 0U) {
        if ((new_status & kStatusFr) != 0U) {
            ctx->f_odd = &ctx->f1.u32l;
            ctx->mips3_float_mode = true;
        } else {
            ctx->f_odd = &ctx->f0.u32h;
            ctx->mips3_float_mode = false;
        }
    }
    // BattleTanx only changes FR through the translated path. Treat any
    // other status mutation as a guest invariant violation.
    if ((changed & ~kStatusFr) != 0U) {
        btga_vita_log("guest attempted an unsupported COP0 status change");
        std::abort();
    }
    ctx->status_reg = new_status;
}

extern "C" gpr cop0_status_read(recomp_context* ctx) {
    return static_cast<gpr>(static_cast<std::int32_t>(ctx->status_reg));
}

extern "C" void switch_error(const char* function, std::uint32_t vram,
                             std::uint32_t table) {
    char message[192];
    std::snprintf(message, sizeof(message),
                  "translated switch out of range: %s pc=%08X table=%08X",
                  function ? function : "unknown", vram, table);
    btga_vita_log(message);
    std::abort();
}

extern "C" void do_break(std::uint32_t vram) {
    char message[96];
    std::snprintf(message, sizeof(message), "guest BREAK at %08X", vram);
    btga_vita_log(message);
    std::abort();
}

void run_thread_function(std::uint8_t* rdram, std::uint64_t address,
                         std::uint64_t stack, std::uint64_t argument) {
    char message[128];
    std::snprintf(message, sizeof(message),
                  "starting translated thread pc=%08X sp=%08X arg=%08X",
                  static_cast<std::uint32_t>(address),
                  static_cast<std::uint32_t>(stack),
                  static_cast<std::uint32_t>(argument));
    btga_vita_log(message);
    recomp_context ctx{};
    ctx.r29 = stack;
    ctx.r4 = argument;
    ctx.f_odd = &ctx.f0.u32h;
    get_function(static_cast<std::int32_t>(address))(rdram, &ctx);
}

bool ultramodern::is_game_started() {
    return true;
}

recomp::SaveType recomp::get_save_type() {
    // The retail cartridge has no EEPROM, SRAM, or FlashRAM. Profiles live on
    // Controller Pak and are handled independently by controller_pak_hle.cpp.
    return SaveType::None;
}

bool recomp::sram_allowed() {
    return false;
}

bool recomp::eeprom_allowed() {
    return false;
}

bool recomp::flashram_allowed() {
    return false;
}

std::u8string recomp::current_game_id() {
    return kGameId;
}

std::filesystem::path recomp::get_config_path() {
    return config_path;
}

void recomp::register_config_path(std::filesystem::path path) {
    config_path = std::move(path);
}
