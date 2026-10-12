#include "btga_runtime.h"

#include <stddef.h>

enum {
    BTGA_MAX_FRAME_US = 250000,
    BTGA_MAX_STEPS_PER_FRAME = 4,
};

void btga_runtime_init(BtgaRuntime *runtime, bool assets_ready) {
    if (runtime == NULL) {
        return;
    }

    *runtime = (BtgaRuntime){
        .assets_ready = assets_ready,
        .running = true,
    };
}

void btga_runtime_update(BtgaRuntime *runtime, const BtgaInput *input,
                         uint32_t elapsed_us) {
    unsigned int steps = 0;

    if (runtime == NULL || input == NULL || !runtime->running) {
        return;
    }

    if ((input->pressed & BTGA_BUTTON_START) != 0) {
        runtime->running = false;
        return;
    }
    if ((input->pressed & BTGA_BUTTON_CONFIRM) != 0 && runtime->assets_ready) {
        runtime->screen = BTGA_SCREEN_ASSET_PREVIEW;
    }
    if ((input->pressed & BTGA_BUTTON_CANCEL) != 0) {
        runtime->screen = BTGA_SCREEN_BOOT;
    }

    if (elapsed_us > BTGA_MAX_FRAME_US) {
        elapsed_us = BTGA_MAX_FRAME_US;
    }
    runtime->accumulator_us += elapsed_us;
    while (runtime->accumulator_us >= BTGA_SIMULATION_STEP_US &&
           steps < BTGA_MAX_STEPS_PER_FRAME) {
        runtime->accumulator_us -= BTGA_SIMULATION_STEP_US;
        runtime->simulation_ticks++;
        steps++;
    }
    if (steps == BTGA_MAX_STEPS_PER_FRAME &&
        runtime->accumulator_us >= BTGA_SIMULATION_STEP_US) {
        runtime->accumulator_us %= BTGA_SIMULATION_STEP_US;
    }
}
