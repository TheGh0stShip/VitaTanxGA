#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

#include "recomp_overlays.inl"
#include "vita_recomp_diagnostics.hpp"

namespace {
constexpr std::uint32_t kSectionRam = 0x80071000U;

recomp_func_t* find_exact(std::uint32_t offset) {
    std::size_t first = 0;
    std::size_t count = ARRLEN(section_0_resident_first_mb_funcs);
    while (count != 0) {
        const std::size_t step = count / 2;
        const std::size_t index = first + step;
        const std::uint32_t candidate =
            section_0_resident_first_mb_funcs[index].offset;
        if (candidate < offset) {
            first = index + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    if (first < ARRLEN(section_0_resident_first_mb_funcs) &&
        section_0_resident_first_mb_funcs[first].offset == offset) {
        return section_0_resident_first_mb_funcs[first].func;
    }
    return nullptr;
}
} // namespace

extern "C" recomp_func_t* get_function(std::int32_t address) {
    const std::uint32_t vram = static_cast<std::uint32_t>(address);
    if (vram < kSectionRam) {
        btga_vita_log("guest indirect call below the translated section");
        std::abort();
    }
    recomp_func_t* function = find_exact(vram - kSectionRam);
    if (function == nullptr) {
        char message[96];
        std::snprintf(message, sizeof(message),
                      "unresolved guest indirect call at %08X", vram);
        btga_vita_log(message);
        std::abort();
    }
    return function;
}

gpr get_entrypoint_address() {
    return static_cast<gpr>(static_cast<std::int32_t>(kSectionRam));
}
