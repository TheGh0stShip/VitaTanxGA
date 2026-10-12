#include "btga_runtime.h"

#include <assert.h>

int btga_world_tests(void);
int btga_geometry_tests(void);
int btga_view_tests(void);
int btga_material_tests(void);
int btga_texture_tests(void);
int btga_tank_tests(void);
int btga_script_tests(void);

int main(void) {
    BtgaRuntime runtime;
    BtgaInput input = {0};

    btga_runtime_init(&runtime, true);
    assert(runtime.running);
    assert(runtime.screen == BTGA_SCREEN_TITLE);
    assert(btga_runtime_add_enemy(&runtime, 100.0f, 0.0f, 200.0f, 0));
    assert(runtime.enemies[0].active && runtime.enemies[0].health == 100);

    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US * 2);
    assert(runtime.simulation_ticks == 2);

    input.pressed = BTGA_BUTTON_CONFIRM;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_GAMEPLAY);

    input = (BtgaInput){.move_y = -32768, .pressed = BTGA_BUTTON_FIRE};
    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US);
    assert(runtime.player.velocity > 0.0f);
    assert(runtime.player.ammo == 199);
    assert(runtime.projectiles[0].active);

    input = (BtgaInput){.pressed = BTGA_BUTTON_START};
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_PAUSED);

    input.pressed = BTGA_BUTTON_CANCEL;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_TITLE);

    input.pressed = BTGA_BUTTON_SELECT;
    btga_runtime_update(&runtime, &input, 0);
    assert(!runtime.running);
    btga_world_tests();
    btga_geometry_tests();
    btga_view_tests();
    btga_material_tests();
    btga_texture_tests();
    btga_tank_tests();
    btga_script_tests();
    return 0;
}
