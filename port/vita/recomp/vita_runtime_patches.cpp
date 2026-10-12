#include <algorithm>
#include <bit>
#include <cstdint>

#include "recomp.h"

extern "C" void yield_self_1ms(std::uint8_t* rdram);

namespace {
float read_guest_float(std::uint8_t* rdram, std::uint32_t address) {
    const gpr guest = static_cast<gpr>(static_cast<std::int32_t>(address));
    return std::bit_cast<float>(static_cast<std::uint32_t>(MEM_W(0, guest)));
}

struct DeadlineRemainder {
    std::uint32_t key;
    float value;
};

// The retail game has a small, fixed set of live countdown owners. Keeping
// this state bounded avoids a desktop unordered_map allocation in the frame
// path while retaining the PC port's fractional-frame correction.
DeadlineRemainder deadline_remainders[32]{};

float& deadline_remainder(std::uint32_t key) {
    for (auto& entry : deadline_remainders) {
        if (entry.key == key || entry.key == 0) {
            entry.key = key;
            return entry.value;
        }
    }
    deadline_remainders[0] = {key, 0.0F};
    return deadline_remainders[0].value;
}

std::int16_t find_free_render_slot(std::uint8_t* rdram, gpr context) {
    for (std::int16_t index = 0; index < 3; ++index) {
        if (index != MEM_H(0xB6, context) && index != MEM_H(0xB4, context) &&
            index != MEM_H(0xB2, context)) return index;
    }
    return -1;
}
} // namespace

extern "C" void btga_shield_flash_step(std::uint8_t* rdram,
                                         recomp_context* ctx) {
    ctx->f2.fl = read_guest_float(rdram, 0x80219488U);
}

extern "C" void btga_turn_step(std::uint8_t* rdram, recomp_context* ctx) {
    const float step = read_guest_float(rdram, 0x80219488U);
    const std::int32_t amount =
        step > 0.0F ? static_cast<std::int32_t>(step * 1024.0F) : 0;
    ctx->r6 = std::min(amount, 0xFFFF);
}

extern "C" void btga_deadline_step(std::uint8_t* rdram,
                                     recomp_context* ctx) {
    float& remainder = deadline_remainder(static_cast<std::uint32_t>(ctx->r20));
    const float total = read_guest_float(rdram, 0x80219488U) + remainder;
    const std::int32_t whole =
        total > 0.0F ? static_cast<std::int32_t>(total) : 0;
    remainder = total - static_cast<float>(whole);
    ctx->r3 = whole;
}

extern "C" void btga_score_count_step(std::uint8_t* rdram,
                                        recomp_context* ctx) {
    if (static_cast<std::int32_t>(ctx->r3) == 0 &&
        read_guest_float(rdram, 0x803A5948U) > 0.0F) ctx->r3 = 1;
}

extern "C" void btga_cutscene_far_plane(std::uint8_t*, recomp_context* ctx) {
    constexpr std::int32_t near_plane = 16;
    constexpr std::int32_t fallback_far_plane = 32767;
    if (static_cast<std::int32_t>(ctx->r3) <= near_plane) {
        ctx->r3 = fallback_far_plane;
    }
}

extern "C" void func_80098B40(std::uint8_t* rdram, recomp_context*) {
    constexpr gpr pending = static_cast<gpr>(static_cast<std::int32_t>(0x80219230U));
    while (MEM_W(0, pending) != 0) yield_self_1ms(rdram);
}

extern "C" void func_800A1384(std::uint8_t* rdram, recomp_context*) {
    constexpr gpr state = static_cast<gpr>(static_cast<std::int32_t>(0x80222930U));
    for (;;) {
        if (MEM_HU(0x1F4, state) == 0 && MEM_W(0x1FC, state) == 0 &&
            MEM_HU(0x1F0, state) >= 5) return;
        yield_self_1ms(rdram);
    }
}

extern "C" void func_80079FF0(std::uint8_t* rdram, recomp_context*) {
    constexpr gpr global = static_cast<gpr>(static_cast<std::int32_t>(0x801144F8U));
    const gpr state = MEM_W(0, global);
    while (MEM_W(0xC4, MEM_W(0, global)) != 0) yield_self_1ms(rdram);
    const gpr source = ADD32(state, 0xC8);
    const gpr destination = ADD32(state, 0x90 + (MEM_HU(0xC0, state) << 4));
    MEM_W(0, source) = MEM_W(4, destination);
    MEM_W(4, source) = MEM_W(8, destination);
    MEM_W(8, source) = ADD32(MEM_W(12, destination), 0x40);
}

extern "C" void func_8007A818(std::uint8_t* rdram, recomp_context*) {
    constexpr gpr global = static_cast<gpr>(static_cast<std::int32_t>(0x80114500U));
    gpr context = MEM_W(0, global);
    while (MEM_H(0xB0, context) != -1) yield_self_1ms(rdram);
    while (find_free_render_slot(rdram, context) == -1) yield_self_1ms(rdram);
    context = MEM_W(0, global);
    MEM_H(0xB0, context) = find_free_render_slot(rdram, context);
}
