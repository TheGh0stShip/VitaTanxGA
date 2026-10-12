#include "vita_platform.h"

int main(void) {
    BtgaRuntime runtime;
    uint64_t previous_time;

    if (!vita_platform_startup()) {
        return 1;
    }
    btga_runtime_init(&runtime, true);
    previous_time = vita_platform_now_us();

    while (runtime.running) {
        BtgaInput input;
        uint64_t current_time = vita_platform_now_us();
        uint64_t elapsed = current_time - previous_time;
        previous_time = current_time;

        vita_platform_poll_input(&input);
        btga_runtime_update(&runtime, &input,
                            elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed);
        vita_platform_begin_frame();
        vita_platform_draw(&runtime);
        vita_platform_end_frame();
    }

    vita_platform_shutdown();
    return 0;
}
