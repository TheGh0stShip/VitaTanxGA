#include "vita_platform.h"

static void configure_mission(BtgaRuntime *runtime) {
    BtgaWorldBounds bounds;
    BtgaWorldSpawn spawn;
    static const uint8_t one_player_order[BTGA_CAMPAIGN_SLOT_COUNT] =
        {0, 2, 3, 4, 1};
    BtgaMissionConfig config;
    uint8_t slot;
    size_t index;

    btga_runtime_init(runtime, vita_platform_assets_ready());
    {
        const uint8_t *intro_data;
        size_t intro_size;
        if (vita_platform_intro_script(&intro_data, &intro_size))
            btga_runtime_set_intro_script(runtime, intro_data, intro_size);
    }
    if (vita_platform_world_bounds(&bounds))
        btga_runtime_set_world_bounds(runtime, bounds.min_x, bounds.min_z,
                                      bounds.max_x, bounds.max_z);
    btga_runtime_set_terrain(runtime, vita_platform_terrain());
    if (vita_platform_spawn(0, 0, &spawn))
        btga_runtime_set_player_spawn(runtime, spawn.x, spawn.y, spawn.z,
                                      spawn.yaw);
    if (vita_platform_mission_config(&config)) {
        for (slot = 1; slot <= config.ai_count; slot++) {
            uint8_t source_slot = one_player_order[slot];
            if (vita_platform_spawn(0, source_slot, &spawn))
                btga_runtime_add_enemy_configured(
                    runtime, spawn.x, spawn.y, spawn.z, spawn.yaw,
                    (uint8_t)config.tank_kinds[source_slot],
                    config.teams[source_slot]);
        }
    }
    {
        BtgaWorldPickup pickup;
        for (index = 0; vita_platform_pickup(index, &pickup); index++)
            btga_runtime_add_pickup(runtime, pickup.x, pickup.y, pickup.z,
                                    pickup.kind);
    }
}

int main(void) {
    BtgaRuntime runtime;
    uint64_t previous_time;

    if (!vita_platform_startup()) {
        return 1;
    }
    configure_mission(&runtime);
    previous_time = vita_platform_now_us();

    while (runtime.running) {
        BtgaInput input;
        uint64_t current_time = vita_platform_now_us();
        uint64_t elapsed = current_time - previous_time;
        previous_time = current_time;

        vita_platform_poll_input(&input);
        if (runtime.screen == BTGA_SCREEN_TITLE &&
            (input.pressed & (BTGA_BUTTON_LEFT | BTGA_BUTTON_RIGHT)) != 0) {
            uint8_t mission = vita_platform_mission();
            if ((input.pressed & BTGA_BUTTON_LEFT) != 0)
                mission = mission == 0 ?
                    vita_platform_highest_unlocked_mission() :
                    (uint8_t)(mission - 1);
            else mission = mission >= vita_platform_highest_unlocked_mission() ?
                0 : (uint8_t)(mission + 1);
            if (vita_platform_load_mission(mission)) configure_mission(&runtime);
        } else if (runtime.screen == BTGA_SCREEN_VICTORY &&
                   (input.pressed & BTGA_BUTTON_CONFIRM) != 0) {
            uint8_t mission = vita_platform_mission();
            vita_platform_complete_mission(mission);
            mission = mission == 18 ? 0 : (uint8_t)(mission + 1);
            if (vita_platform_load_mission(mission)) configure_mission(&runtime);
        }
        btga_runtime_update(&runtime, &input,
                            elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed);
        vita_platform_begin_frame();
        vita_platform_draw(&runtime);
        vita_platform_end_frame();
    }

    vita_platform_shutdown();
    return 0;
}
