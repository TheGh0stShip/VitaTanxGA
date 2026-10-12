#include "btga_runtime.h"

#include <assert.h>

int btga_world_tests(void);

int main(void) {
    BtgaRuntime runtime;
    BtgaInput input = {0};

    btga_runtime_init(&runtime, true);
    assert(runtime.running);
    assert(runtime.screen == BTGA_SCREEN_BOOT);

    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US * 2);
    assert(runtime.simulation_ticks == 2);

    input.pressed = BTGA_BUTTON_CONFIRM;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_ASSET_PREVIEW);

    input.pressed = BTGA_BUTTON_CANCEL;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_BOOT);

    input.pressed = BTGA_BUTTON_START;
    btga_runtime_update(&runtime, &input, 0);
    assert(!runtime.running);
    btga_world_tests();
    return 0;
}
