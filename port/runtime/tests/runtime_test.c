#include "btga_runtime.h"

#include <assert.h>

int btga_world_tests(void);
int btga_geometry_tests(void);
int btga_view_tests(void);
int btga_material_tests(void);
int btga_texture_tests(void);
int btga_tank_tests(void);
int btga_script_tests(void);
int btga_campaign_tests(void);

int main(void) {
    BtgaRuntime runtime;
    BtgaInput input = {0};

    btga_runtime_init(&runtime, true);
    assert(runtime.running);
    assert(runtime.screen == BTGA_SCREEN_TITLE);
    assert(btga_runtime_add_enemy(&runtime, 100.0f, 0.0f, 200.0f, 0));
    assert(runtime.enemies[0].tank_kind == 0);
    assert(runtime.enemies[0].team == 1);
    assert(btga_runtime_add_enemy_configured(&runtime, 200.0f, 0.0f, 200.0f,
                                             0, 4, 3));
    assert(runtime.enemies[1].tank_kind == 4 && runtime.enemies[1].team == 3);
    assert(btga_runtime_add_ally(&runtime, -100.0f, 0.0f, 200.0f, 0));
    assert(btga_runtime_active_enemies(&runtime) == 2);
    assert(btga_runtime_active_allies(&runtime) == 1);
    assert(runtime.enemies[2].team == 0);
    assert(runtime.enemies[0].active && runtime.enemies[0].health == 100);
    assert(btga_runtime_add_pickup(&runtime, 0.0f, 0.0f, 576.0f, 7));
    assert(btga_runtime_add_pickup(&runtime, 0.0f, 0.0f, 576.0f, 1));
    assert(btga_runtime_add_pickup(&runtime, 0.0f, 0.0f, 576.0f, 11));
    runtime.enemies[0].x = 50.0f;
    runtime.enemies[0].health = 25;

    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US * 2);
    assert(runtime.simulation_ticks == 2);

    {
        static const uint8_t intro[] = {8, 51, 21, 'H', 'I', 0, 1, 0, 1, 0};
        assert(btga_runtime_set_intro_script(&runtime, intro, sizeof(intro)));
    }
    input.pressed = BTGA_BUTTON_CONFIRM;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_INTRO);
    assert(runtime.enemies[0].x == 100.0f && runtime.enemies[0].health == 100);
    assert(runtime.enemies[0].turret_yaw == 0.0f);
    input.pressed = BTGA_BUTTON_CANCEL;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_BRIEFING);
    input.pressed = BTGA_BUTTON_CONFIRM;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_GAMEPLAY);
    runtime.player.health = 50;
    runtime.enemies[0].yaw = 1.0f;

    input = (BtgaInput){.move_y = -32768, .pressed = BTGA_BUTTON_FIRE};
    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US);
    assert(runtime.enemies[0].turret_yaw != 0.0f);
    assert(runtime.player.velocity > 0.0f);
    assert(runtime.player.ammo == 100);
    assert(runtime.player.health == 80);
    assert(runtime.pickup_inventory[1] == 5);
    assert(btga_runtime_active_pickups(&runtime) == 0);
    assert(runtime.last_pickup_kind == 11 && runtime.pickup_message_ticks != 0);
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
    btga_campaign_tests();
    return 0;
}
