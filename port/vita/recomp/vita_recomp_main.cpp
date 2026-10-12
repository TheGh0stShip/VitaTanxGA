#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <vitaGL.h>

#include "funcs.h"
#include "librecomp/game.hpp"
#include "librecomp/rsp.hpp"
#include "ultramodern/ultramodern.hpp"

extern RspExitReason n_aspMain(std::uint8_t*, std::uint32_t);
extern std::atomic_bool exited;
extern moodycamel::LightweightSemaphore graphics_shutdown_ready;
std::unique_ptr<ultramodern::renderer::RendererContext>
btga_vita_create_renderer(std::uint8_t*, ultramodern::renderer::WindowHandle, bool);

namespace {
constexpr std::size_t kRomSize = 8U * 1024U * 1024U;
constexpr std::size_t kRdramSize = 64U * 1024U * 1024U;
constexpr std::size_t kAudioFrames = 1024;
constexpr std::size_t kAudioRingFrames = 16384;
std::uint16_t buttons;
float stick_x;
float stick_y;
int audio_port = -1;
std::atomic<bool> audio_running{false};
std::int16_t audio_ring[kAudioRingFrames * 2]{};
std::atomic<std::uint32_t> audio_read{0};
std::atomic<std::uint32_t> audio_write{0};
std::thread audio_thread;

bool load_rom(std::vector<std::uint8_t>& bytes) {
    FILE* file = std::fopen("ux0:data/VitaTanxGA/baserom.z64", "rb");
    if (file == nullptr) return false;
    bytes.resize(kRomSize);
    const bool good = std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size() &&
        std::fgetc(file) == EOF && bytes[0] == 0x80 && bytes[1] == 0x37 &&
        bytes[2] == 0x12 && bytes[3] == 0x40;
    std::fclose(file);
    if (!good) bytes.clear();
    return good;
}

void write_guest_word(std::uint8_t* rdram, std::uint32_t address,
                      std::uint32_t value) {
    const std::uint32_t offset = address & 0x3FFFFFFFU;
    std::memcpy(rdram + offset, &value, sizeof(value));
}

void poll_input() {
    SceCtrlData pad{};
    sceCtrlPeekBufferPositive(0, &pad, 1);
    buttons = 0;
    if (pad.buttons & SCE_CTRL_CROSS) buttons |= 0x8000;
    if (pad.buttons & SCE_CTRL_CIRCLE) buttons |= 0x4000;
    if (pad.buttons & SCE_CTRL_LTRIGGER) buttons |= 0x2000;
    if (pad.buttons & SCE_CTRL_START) buttons |= 0x1000;
    if (pad.buttons & SCE_CTRL_UP) buttons |= 0x0800;
    if (pad.buttons & SCE_CTRL_DOWN) buttons |= 0x0400;
    if (pad.buttons & SCE_CTRL_LEFT) buttons |= 0x0200;
    if (pad.buttons & SCE_CTRL_RIGHT) buttons |= 0x0100;
    if (pad.buttons & SCE_CTRL_SELECT) buttons |= 0x0020;
    if (pad.buttons & SCE_CTRL_RTRIGGER) buttons |= 0x0010;
    const int rx = static_cast<int>(pad.rx) - 128;
    const int ry = static_cast<int>(pad.ry) - 128;
    if (ry < -40) buttons |= 0x0008;
    if (ry > 40) buttons |= 0x0004;
    if (rx < -40) buttons |= 0x0002;
    if (rx > 40) buttons |= 0x0001;
    stick_x = std::clamp((static_cast<int>(pad.lx) - 128) / 127.0F, -1.0F, 1.0F);
    stick_y = std::clamp((128 - static_cast<int>(pad.ly)) / 127.0F, -1.0F, 1.0F);
}

bool get_input(int controller, std::uint16_t* out_buttons, float* x, float* y) {
    if (controller != 0) return false;
    *out_buttons = buttons; *x = stick_x; *y = stick_y;
    return true;
}
void set_rumble(int, bool) {}
ultramodern::input::connected_device_info_t connected_device(int controller) {
    using ultramodern::input::Device;
    using ultramodern::input::Pak;
    return controller == 0 ? ultramodern::input::connected_device_info_t{Device::Controller, Pak::RumblePak}
                           : ultramodern::input::connected_device_info_t{Device::None, Pak::None};
}

void run_audio() {
    alignas(64) std::int16_t block[kAudioFrames * 2]{};
    while (audio_running.load()) {
        std::uint32_t read = audio_read.load(std::memory_order_relaxed);
        const std::uint32_t write = audio_write.load(std::memory_order_acquire);
        for (std::size_t frame = 0; frame < kAudioFrames; ++frame) {
            if (read != write) {
                block[frame * 2] = audio_ring[read * 2];
                block[frame * 2 + 1] = audio_ring[read * 2 + 1];
                read = (read + 1) % kAudioRingFrames;
            } else {
                block[frame * 2] = block[frame * 2 + 1] = 0;
            }
        }
        audio_read.store(read, std::memory_order_release);
        sceAudioOutOutput(audio_port, block);
    }
}

void set_audio_frequency(std::uint32_t frequency) {
    if (audio_port >= 0) sceAudioOutReleasePort(audio_port);
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, kAudioFrames,
                                     frequency, SCE_AUDIO_OUT_MODE_STEREO);
    sceAudioOutSetConfig(audio_port, kAudioFrames, frequency,
                         SCE_AUDIO_OUT_MODE_STEREO);
}
void queue_audio(std::int16_t* samples, std::size_t sample_count) {
    const std::size_t frames = sample_count / 2;
    std::uint32_t write = audio_write.load(std::memory_order_relaxed);
    const std::uint32_t read = audio_read.load(std::memory_order_acquire);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::uint32_t next = (write + 1) % kAudioRingFrames;
        if (next == read) break;
        audio_ring[write * 2] = samples[frame * 2];
        audio_ring[write * 2 + 1] = samples[frame * 2 + 1];
        write = next;
    }
    audio_write.store(write, std::memory_order_release);
}
std::size_t audio_remaining() {
    const std::uint32_t read = audio_read.load(std::memory_order_acquire);
    const std::uint32_t write = audio_write.load(std::memory_order_acquire);
    return write >= read ? write - read : kAudioRingFrames - read + write;
}

RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    return task->t.type == M_AUDTASK ? n_aspMain : nullptr;
}
void on_vi() {}
void on_gfx_init() {}
void show_error(const char*) {}
std::string thread_name(const OSThread* thread) {
    return "BTGA-" + std::to_string(thread->id);
}
} // namespace

int main() {
    sceIoMkdir("ux0:data/VitaTanxGA", 0777);
    std::vector<std::uint8_t> rom;
    if (!load_rom(rom)) return 2;
    auto rdram = std::unique_ptr<std::uint8_t, decltype(&std::free)>(
        static_cast<std::uint8_t*>(std::calloc(1, kRdramSize)), &std::free);
    if (!rdram) return 3;
    recomp::set_rom_contents(std::move(rom));
    recomp::do_rom_read(rdram.get(),
                        static_cast<gpr>(static_cast<std::int32_t>(0x80071000U)),
                        0x10001000, 0x100000);
    // Values normally established by the N64 IPL3 before control reaches the
    // cartridge entrypoint.
    write_guest_word(rdram.get(), 0x80000300U, 1U);             // NTSC
    write_guest_word(rdram.get(), 0x80000308U, 0xB0000000U);   // ROM base
    write_guest_word(rdram.get(), 0x8000030CU, 0U);             // cold reset
    write_guest_word(rdram.get(), 0x80000318U, 8U * 1024U * 1024U);

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    if (!vglInitExtended(0, 960, 544, 0x1800000,
                         SCE_GXM_MULTISAMPLE_NONE)) return 4;
    const recomp::rsp::callbacks_t rsp_select{get_rsp_microcode};
    recomp::rsp::set_callbacks(rsp_select);
    const ultramodern::rsp::callbacks_t rsp{
        recomp::rsp::constants_init, recomp::rsp::run_task};
    const ultramodern::renderer::callbacks_t renderer{btga_vita_create_renderer};
    const ultramodern::audio_callbacks_t audio{queue_audio, audio_remaining,
                                               set_audio_frequency};
    const ultramodern::input::callbacks_t input{poll_input, get_input,
                                                set_rumble, connected_device};
    const ultramodern::gfx_callbacks_t gfx{};
    const ultramodern::events::callbacks_t events{on_vi, on_gfx_init};
    const ultramodern::error_handling::callbacks_t errors{show_error};
    const ultramodern::threads::callbacks_t threads{thread_name};
    ultramodern::set_callbacks(rsp, renderer, audio, input, gfx, events,
                               errors, threads);

    audio_running.store(true);
    audio_thread = std::thread(run_audio);
    ultramodern::preinit(rdram.get(), nullptr);
    ultramodern::init_saving(rdram.get());
    recomp_context context{};
    context.f_odd = &context.f0.u32h;
    recomp_entrypoint(rdram.get(), &context);
    while (!exited.load()) ultramodern::sleep_milliseconds(10);

    graphics_shutdown_ready.signal();
    ultramodern::join_event_threads();
    ultramodern::join_thread_cleaner_thread();
    ultramodern::join_saving_thread();
    audio_running.store(false);
    if (audio_thread.joinable()) audio_thread.join();
    if (audio_port >= 0) sceAudioOutReleasePort(audio_port);
    return 0;
}
