#include "vita_recomp_diagnostics.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>

namespace {
std::mutex log_mutex;
std::atomic<std::uint32_t> vi_count{0};
std::atomic<std::uint32_t> display_list_count{0};
std::atomic<std::uint32_t> rejected_display_list_count{0};
std::atomic<std::uint32_t> present_count{0};
std::atomic<std::uint32_t> audio_open_count{0};

void note_first(std::atomic<std::uint32_t>& counter, const char* message) {
    if (counter.fetch_add(1, std::memory_order_relaxed) == 0) btga_vita_log(message);
}
} // namespace

void btga_vita_log(const char* message) {
    if (message == nullptr) message = "runtime error";
    std::lock_guard lock{log_mutex};
    std::printf("VitaTanxGA: %s\n", message);
    FILE* log = std::fopen("ux0:data/VitaTanxGA/runtime.log", "ab");
    if (log != nullptr) {
        std::fprintf(log, "%s\n", message);
        std::fclose(log);
    }
}

void btga_vita_note_vi() { note_first(vi_count, "first VI event delivered"); }

void btga_vita_note_display_list(bool accepted) {
    if (accepted) note_first(display_list_count, "first display list accepted");
    else note_first(rejected_display_list_count, "first display list rejected");
}

void btga_vita_note_present() { note_first(present_count, "first frame presented"); }

void btga_vita_note_audio_open() { note_first(audio_open_count, "48 kHz audio output opened"); }

void btga_vita_log_runtime_summary() {
    char summary[192];
    std::snprintf(summary, sizeof(summary),
                  "runtime summary: VI=%u display_lists=%u rejected=%u presents=%u audio_opens=%u",
                  vi_count.load(std::memory_order_relaxed),
                  display_list_count.load(std::memory_order_relaxed),
                  rejected_display_list_count.load(std::memory_order_relaxed),
                  present_count.load(std::memory_order_relaxed),
                  audio_open_count.load(std::memory_order_relaxed));
    btga_vita_log(summary);
}
