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
    static const uint8_t pickup_weapons[BTGA_PICKUP_KIND_COUNT] = {
        0, 2, 3, 4, 16, 7, 6, 0, 0, 8,
        9, 0, 10, 0, 15, 12, 13, 14, 0, 0,
    };
    static const uint16_t pickup_amounts[BTGA_PICKUP_KIND_COUNT] = {
        0, 5, 5, 5, 2, 1, 20, 0, 0, 5,
        5, 0, 5, 0, 20, 100, 2, 5, 0, 0,
    };
    BtgaRuntime runtime;
    BtgaInput input = {0};
    unsigned int index;

    for (index = 0; index < BTGA_PICKUP_KIND_COUNT; index++) {
        assert(btga_runtime_pickup_weapon((uint8_t)index) ==
               pickup_weapons[index]);
        assert(btga_runtime_pickup_weapon_amount((uint8_t)index) ==
               pickup_amounts[index]);
    }
    assert(btga_runtime_pickup_weapon(BTGA_PICKUP_KIND_COUNT) == 0);
    assert(btga_runtime_weapon_cooldown(2) == 30);
    assert(btga_runtime_weapon_cooldown(4) == 90);
    assert(btga_runtime_weapon_cooldown(12) == 2);

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
    assert(btga_runtime_add_pickup(&runtime, 0.0f, 0.0f, 576.0f, 4));
    {
        BtgaWorldObject object = {
            .x = 100, .y = 0, .z = 100,
            .min_x = -50, .min_y = -20, .min_z = -50,
            .max_x = 50, .max_y = 80, .max_z = 50,
            .kind = 21, .mission_type = 1,
        };
        assert(btga_runtime_add_mission_object(&runtime, &object));
        assert(btga_runtime_active_objectives(&runtime) == 1);
        assert(runtime.mission_objects[0].health == 100);
    }
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
    assert(runtime.mission_objects[0].active &&
           runtime.mission_objects[0].health == 100);
    input.pressed = BTGA_BUTTON_CANCEL;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_BRIEFING);
    input.pressed = BTGA_BUTTON_CONFIRM;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_GAMEPLAY);
    runtime.player.health = 50;
    runtime.enemies[0].yaw = 1.0f;

    input = (BtgaInput){.held = BTGA_BUTTON_FIRE, .move_y = -32768,
                        .pressed = BTGA_BUTTON_FIRE};
    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US);
    assert(runtime.enemies[0].turret_yaw != 0.0f);
    assert(runtime.player.velocity > 0.0f);
    assert(runtime.player.ammo == 100);
    assert(runtime.player.health == 80);
    assert(runtime.weapon_ammo[2] == 5);
    assert(runtime.weapon_ammo[16] == 2);
    assert(runtime.player.selected_weapon == 2);
    assert(btga_runtime_active_pickups(&runtime) == 0);
    assert(runtime.last_pickup_kind == 4 && runtime.pickup_message_ticks != 0);
    assert(runtime.projectiles[0].active);
    assert(runtime.sound_serial == 1 && runtime.sound_effect == 3);
    input.pressed = 0;
    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US * 4);
    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US * 4);
    assert(runtime.player.ammo == 99);
    assert(runtime.sound_serial == 2 && runtime.sound_effect == 3);

    input = (BtgaInput){.held = BTGA_BUTTON_ALT_FIRE,
                        .pressed = BTGA_BUTTON_ALT_FIRE};
    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US);
    assert(runtime.weapon_ammo[2] == 4);
    assert(runtime.player.secondary_cooldown != 0);
    assert(runtime.sound_effect == 0x14);
    {
        unsigned int swarmer_count = 0;
        for (index = 0; index < BTGA_MAX_PROJECTILES; index++)
            if (runtime.projectiles[index].active &&
                runtime.projectiles[index].weapon == 2) swarmer_count++;
        assert(swarmer_count == 3);
    }
    input = (BtgaInput){.pressed = BTGA_BUTTON_SELECT};
    btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US);
    assert(runtime.player.selected_weapon == 16);

    {
        BtgaWorldCollision collision = {
            (int16_t)runtime.player.x, (int16_t)runtime.player.y,
            (int16_t)runtime.player.z, 0,
            -100, -100, -100, 100, 100, 100,
        };
        float blocked_x = runtime.player.x;
        float blocked_z = runtime.player.z;
        btga_runtime_set_collisions(&runtime, &collision, 1);
        input = (BtgaInput){.move_y = -32768};
        btga_runtime_update(&runtime, &input, BTGA_SIMULATION_STEP_US);
        assert(runtime.player.x == blocked_x && runtime.player.z == blocked_z);
        btga_runtime_set_collisions(&runtime, NULL, 0);
    }

    input = (BtgaInput){.pressed = BTGA_BUTTON_START};
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_PAUSED);

    input.pressed = BTGA_BUTTON_CANCEL;
    btga_runtime_update(&runtime, &input, 0);
    assert(runtime.screen == BTGA_SCREEN_TITLE);

    assert(runtime.running);
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
