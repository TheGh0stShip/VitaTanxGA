#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <cstring>
#include <malloc.h>
#include <mutex>
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
#include "vita_recomp_diagnostics.hpp"
#include "vita_recomp_memory.hpp"

extern RspExitReason n_aspMain(std::uint8_t*, std::uint32_t);
extern std::atomic_bool exited;
extern moodycamel::LightweightSemaphore graphics_shutdown_ready;
std::unique_ptr<ultramodern::renderer::RendererContext>
btga_vita_create_renderer(std::uint8_t*, ultramodern::renderer::WindowHandle, bool);

namespace {
constexpr std::size_t kRomSize = 8U * 1024U * 1024U;
constexpr std::size_t kAudioFrames = 1024;
constexpr std::size_t kAudioRingFrames = 16384;
constexpr std::uint32_t kVitaAudioRate = 48000;
constexpr std::size_t kAudioSoftCapFrames = kVitaAudioRate / 5;
constexpr int kControllerCount = 4;
constexpr std::uint8_t kExpectedSha1[20] = {
    0x80, 0x52, 0x48, 0xfb, 0x0a, 0x0e, 0xe6, 0x94, 0xca, 0xd8,
    0xd7, 0xdc, 0x92, 0x7b, 0x63, 0x1d, 0x86, 0x0d, 0xd8, 0xcf,
};
std::array<std::uint16_t, kControllerCount> buttons{};
std::array<float, kControllerCount> stick_x{};
std::array<float, kControllerCount> stick_y{};
std::array<bool, kControllerCount> controller_connected{};
int audio_port = -1;
std::mutex audio_port_mutex;
std::mutex resampler_mutex;
std::uint32_t source_audio_rate = kVitaAudioRate;
std::uint32_t resample_phase = 0;
std::atomic<bool> audio_running{false};
std::int16_t audio_ring[kAudioRingFrames * 2]{};
std::atomic<std::uint32_t> audio_read{0};
std::atomic<std::uint32_t> audio_write{0};
std::thread audio_thread;

std::uint32_t rotate_left(std::uint32_t value, unsigned amount) {
    return (value << amount) | (value >> (32U - amount));
}

std::array<std::uint8_t, 20> sha1(const std::vector<std::uint8_t>& input) {
    std::vector<std::uint8_t> message = input;
    const std::uint64_t bit_count = static_cast<std::uint64_t>(message.size()) * 8U;
    message.push_back(0x80);
    while ((message.size() % 64U) != 56U) message.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>(bit_count >> shift));
    }
    std::uint32_t h[5] = {0x67452301U, 0xEFCDAB89U, 0x98BADCFEU,
                          0x10325476U, 0xC3D2E1F0U};
    for (std::size_t block = 0; block < message.size(); block += 64U) {
        std::uint32_t w[80]{};
        for (unsigned i = 0; i < 16; ++i) {
            const std::size_t p = block + i * 4U;
            w[i] = (static_cast<std::uint32_t>(message[p]) << 24U) |
                   (static_cast<std::uint32_t>(message[p + 1]) << 16U) |
                   (static_cast<std::uint32_t>(message[p + 2]) << 8U) |
                   message[p + 3];
        }
        for (unsigned i = 16; i < 80; ++i) {
            w[i] = rotate_left(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (unsigned i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999U; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1U; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCU; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6U; }
            const std::uint32_t temp = rotate_left(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotate_left(b, 30); b = a; a = temp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::array<std::uint8_t, 20> digest{};
    for (unsigned i = 0; i < 5; ++i) {
        digest[i * 4] = static_cast<std::uint8_t>(h[i] >> 24U);
        digest[i * 4 + 1] = static_cast<std::uint8_t>(h[i] >> 16U);
        digest[i * 4 + 2] = static_cast<std::uint8_t>(h[i] >> 8U);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(h[i]);
    }
    return digest;
}

bool load_rom(std::vector<std::uint8_t>& bytes) {
    FILE* file = std::fopen("ux0:data/VitaTanxGA/baserom.z64", "rb");
    if (file == nullptr) return false;
    bytes.resize(kRomSize);
    const bool complete = std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size() &&
                          std::fgetc(file) == EOF;
    std::fclose(file);
    const auto digest = complete ? sha1(bytes) : std::array<std::uint8_t, 20>{};
    const bool good = complete && bytes[0] == 0x80 && bytes[1] == 0x37 &&
                      bytes[2] == 0x12 && bytes[3] == 0x40 &&
                      std::equal(digest.begin(), digest.end(), kExpectedSha1);
    if (!good) bytes.clear();
    return good;
}

bool patch_screen_edge_scissors(std::vector<std::uint8_t>& rom) {
    struct Fix { std::size_t offset; std::uint32_t original; std::uint32_t replacement; };
    constexpr Fix fixes[] = {
        {0xB7F74, 0x004FC3BCU, 0x005003C0U},
        {0xB7F8C, 0x004FC1DCU, 0x005001DCU},
        {0xB7FA4, 0x004FC3BCU, 0x005003C0U},
        {0xB7FD4, 0x004FC1DCU, 0x005001DCU},
        {0xB7FEC, 0x0027C3BCU, 0x0027C3C0U},
        {0xB8004, 0x004FC3BCU, 0x005003C0U},
    };
    for (const Fix& fix : fixes) {
        if (fix.offset + 4 > rom.size()) return false;
        const std::uint32_t word =
            (static_cast<std::uint32_t>(rom[fix.offset]) << 24U) |
            (static_cast<std::uint32_t>(rom[fix.offset + 1]) << 16U) |
            (static_cast<std::uint32_t>(rom[fix.offset + 2]) << 8U) |
            rom[fix.offset + 3];
        if (word != fix.original) return false;
        rom[fix.offset] = static_cast<std::uint8_t>(fix.replacement >> 24U);
        rom[fix.offset + 1] = static_cast<std::uint8_t>(fix.replacement >> 16U);
        rom[fix.offset + 2] = static_cast<std::uint8_t>(fix.replacement >> 8U);
        rom[fix.offset + 3] = static_cast<std::uint8_t>(fix.replacement);
    }
    return true;
}

void write_guest_word(std::uint8_t* rdram, std::uint32_t address,
                      std::uint32_t value) {
    const std::uint32_t offset = address & 0x3FFFFFFFU;
    std::memcpy(rdram + offset, &value, sizeof(value));
}

void poll_input() {
    SceCtrlPortInfo ports{};
    const bool have_port_info = sceCtrlGetControllerPortInfo(&ports) >= 0;
    for (int controller = 0; controller < kControllerCount; ++controller) {
        // Port zero is Vita's aggregate primary pad. Paired port one mirrors
        // that pad on PSTV, so additional N64 players begin at Vita port two.
        const int vita_port = controller == 0 ? 0 : controller + 1;
        SceCtrlData pad{};
        const bool sampled = sceCtrlPeekBufferPositive2(vita_port, &pad, 1) > 0;
        controller_connected[controller] = sampled &&
            (controller == 0 || !have_port_info || ports.port[vita_port] != SCE_CTRL_TYPE_UNPAIRED);
        buttons[controller] = 0;
        stick_x[controller] = 0.0F;
        stick_y[controller] = 0.0F;
        if (!controller_connected[controller]) continue;
        if (pad.buttons & SCE_CTRL_CROSS) buttons[controller] |= 0x8000;
        if (pad.buttons & SCE_CTRL_CIRCLE) buttons[controller] |= 0x4000;
        if (pad.buttons & SCE_CTRL_LTRIGGER) buttons[controller] |= 0x2000;
        if (pad.buttons & SCE_CTRL_START) buttons[controller] |= 0x1000;
        if (pad.buttons & SCE_CTRL_UP) buttons[controller] |= 0x0800;
        if (pad.buttons & SCE_CTRL_DOWN) buttons[controller] |= 0x0400;
        if (pad.buttons & SCE_CTRL_LEFT) buttons[controller] |= 0x0200;
        if (pad.buttons & SCE_CTRL_RIGHT) buttons[controller] |= 0x0100;
        if (pad.buttons & SCE_CTRL_SELECT) buttons[controller] |= 0x0020;
        if (pad.buttons & SCE_CTRL_RTRIGGER) buttons[controller] |= 0x0010;
        const int rx = static_cast<int>(pad.rx) - 128;
        const int ry = static_cast<int>(pad.ry) - 128;
        if (ry < -40) buttons[controller] |= 0x0008;
        if (ry > 40) buttons[controller] |= 0x0004;
        if (rx < -40) buttons[controller] |= 0x0002;
        if (rx > 40) buttons[controller] |= 0x0001;
        stick_x[controller] = std::clamp(
            (static_cast<int>(pad.lx) - 128) / 127.0F, -1.0F, 1.0F);
        stick_y[controller] = std::clamp(
            (128 - static_cast<int>(pad.ly)) / 127.0F, -1.0F, 1.0F);
    }
}

bool get_input(int controller, std::uint16_t* out_buttons, float* x, float* y) {
    if (controller < 0 || controller >= kControllerCount ||
        !controller_connected[controller]) return false;
    *out_buttons = buttons[controller];
    *x = stick_x[controller];
    *y = stick_y[controller];
    return controller_connected[controller];
}
void set_rumble(int controller, bool enabled) {
    if (controller < 0 || controller >= kControllerCount) return;
    const int actuator_port = controller + 1;
    SceCtrlPortInfo ports{};
    if (sceCtrlGetControllerPortInfo(&ports) < 0 ||
        ports.port[actuator_port] == SCE_CTRL_TYPE_UNPAIRED) return;
    SceCtrlActuator actuator{};
    actuator.small = enabled ? 0x80 : 0;
    actuator.large = enabled ? 0xFF : 0;
    sceCtrlSetActuator(actuator_port, &actuator);
}
ultramodern::input::connected_device_info_t connected_device(int controller) {
    using ultramodern::input::Device;
    using ultramodern::input::Pak;
    if (controller < 0 || controller >= kControllerCount) return {Device::None, Pak::None};
    if (controller == 0) return {Device::Controller, Pak::RumblePak};
    SceCtrlPortInfo ports{};
    const int vita_port = controller + 1;
    if (sceCtrlGetControllerPortInfo(&ports) < 0 ||
        ports.port[vita_port] == SCE_CTRL_TYPE_UNPAIRED) return {Device::None, Pak::None};
    return {Device::Controller, Pak::RumblePak};
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
        std::lock_guard lock{audio_port_mutex};
        if (audio_port >= 0) sceAudioOutOutput(audio_port, block);
        else ultramodern::sleep_milliseconds(2);
    }
}

void set_audio_frequency(std::uint32_t frequency) {
    if (frequency == 0) return;
    {
        std::lock_guard lock{resampler_mutex};
        source_audio_rate = frequency;
        resample_phase = 0;
    }
    std::lock_guard lock{audio_port_mutex};
    if (audio_port < 0) {
        audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, kAudioFrames,
                                         kVitaAudioRate, SCE_AUDIO_OUT_MODE_STEREO);
        if (audio_port >= 0) {
            sceAudioOutSetConfig(audio_port, kAudioFrames, kVitaAudioRate,
                                 SCE_AUDIO_OUT_MODE_STEREO);
        } else {
            btga_vita_log("48 kHz audio output could not be opened");
        }
        if (audio_port >= 0) btga_vita_note_audio_open();
    }
}
void queue_audio(std::int16_t* samples, std::size_t sample_count) {
    const std::size_t frames = sample_count / 2;
    std::uint32_t write = audio_write.load(std::memory_order_relaxed);
    const std::uint32_t read = audio_read.load(std::memory_order_acquire);
    const std::size_t queued =
        write >= read ? write - read : kAudioRingFrames - read + write;
    // Transitions and skipped cutscenes can submit faster than real time. Do
    // not let stale effects accumulate behind more than 200 ms of Vita audio.
    if (queued > kAudioSoftCapFrames) return;
    std::lock_guard lock{resampler_mutex};
    for (std::size_t frame = 0; frame < frames; ++frame) {
        resample_phase += kVitaAudioRate;
        while (resample_phase >= source_audio_rate) {
            const std::uint32_t next = (write + 1) % kAudioRingFrames;
            if (next == read) {
                audio_write.store(write, std::memory_order_release);
                return;
            }
            audio_ring[write * 2] = samples[frame * 2];
            audio_ring[write * 2 + 1] = samples[frame * 2 + 1];
            write = next;
            resample_phase -= source_audio_rate;
        }
    }
    audio_write.store(write, std::memory_order_release);
}
std::size_t audio_remaining() {
    const std::uint32_t read = audio_read.load(std::memory_order_acquire);
    const std::uint32_t write = audio_write.load(std::memory_order_acquire);
    const std::size_t output_frames =
        write >= read ? write - read : kAudioRingFrames - read + write;
    std::lock_guard lock{resampler_mutex};
    return output_frames * source_audio_rate / kVitaAudioRate;
}

RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    return task->t.type == M_AUDTASK ? n_aspMain : nullptr;
}
void on_vi() { btga_vita_note_vi(); }
void on_gfx_init() {}
void show_error(const char* message) { btga_vita_log(message); }
std::string thread_name(const OSThread* thread) {
    return "BTGA-" + std::to_string(thread->id);
}
} // namespace

int main() {
    sceIoMkdir("ux0:data/VitaTanxGA", 0777);
    sceIoMkdir("ux0:data/VitaTanxGA/shader_cache", 0777);
    std::remove("ux0:data/VitaTanxGA/runtime.log");
    btga_vita_log("starting translated-game runtime");
    std::vector<std::uint8_t> rom;
    if (!load_rom(rom)) {
        btga_vita_log("baserom.z64 is missing or is not the supported USA Rev 0 ROM");
        return 2;
    }
    if (!patch_screen_edge_scissors(rom)) {
        btga_vita_log("verified ROM did not contain the expected viewport display lists");
        return 2;
    }
    auto rdram = std::unique_ptr<std::uint8_t, decltype(&std::free)>(
        static_cast<std::uint8_t*>(memalign(64, btga::vita::kRdramSize)), &std::free);
    if (!rdram) { btga_vita_log("8 MiB guest memory allocation failed"); return 3; }
    std::memset(rdram.get(), 0, btga::vita::kRdramSize);
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
    sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
    if (!vglInitExtended(0, 960, 544, 0x1800000,
                         SCE_GXM_MULTISAMPLE_NONE)) {
        btga_vita_log("vitaGL initialization failed");
        return 4;
    }
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
    btga_vita_log("translated cartridge entrypoint returned to Vita host loop");
    while (!exited.load()) ultramodern::sleep_milliseconds(10);

    graphics_shutdown_ready.signal();
    ultramodern::join_event_threads();
    ultramodern::join_thread_cleaner_thread();
    ultramodern::join_saving_thread();
    audio_running.store(false);
    if (audio_thread.joinable()) audio_thread.join();
    for (int controller = 0; controller < kControllerCount; ++controller) {
        set_rumble(controller, false);
    }
    {
        std::lock_guard lock{audio_port_mutex};
        if (audio_port >= 0) sceAudioOutReleasePort(audio_port);
        audio_port = -1;
    }
    btga_vita_log_runtime_summary();
    btga_vita_log("runtime stopped cleanly");
    return 0;
}
