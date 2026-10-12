#ifndef VITATANXGA_RUNTIME_H
#define VITATANXGA_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

enum BtgaButton {
    BTGA_BUTTON_CONFIRM = 1u << 0,
    BTGA_BUTTON_CANCEL = 1u << 1,
    BTGA_BUTTON_START = 1u << 2,
    BTGA_BUTTON_UP = 1u << 3,
    BTGA_BUTTON_DOWN = 1u << 4,
    BTGA_BUTTON_LEFT = 1u << 5,
    BTGA_BUTTON_RIGHT = 1u << 6,
    BTGA_BUTTON_FIRE = 1u << 7,
    BTGA_BUTTON_ALT_FIRE = 1u << 8,
};

typedef struct BtgaInput {
    uint32_t held;
    uint32_t pressed;
    uint32_t released;
    int16_t move_x;
    int16_t move_y;
    int16_t aim_x;
    int16_t aim_y;
} BtgaInput;

typedef enum BtgaRuntimeScreen {
    BTGA_SCREEN_BOOT = 0,
    BTGA_SCREEN_ASSET_PREVIEW = 1,
} BtgaRuntimeScreen;

typedef struct BtgaRuntime {
    uint64_t simulation_ticks;
    uint32_t accumulator_us;
    BtgaRuntimeScreen screen;
    bool assets_ready;
    bool running;
} BtgaRuntime;

enum {
    BTGA_SIMULATION_HZ = 30,
    BTGA_SIMULATION_STEP_US = 1000000 / BTGA_SIMULATION_HZ,
};

void btga_runtime_init(BtgaRuntime *runtime, bool assets_ready);
void btga_runtime_update(BtgaRuntime *runtime, const BtgaInput *input,
                         uint32_t elapsed_us);

#endif
