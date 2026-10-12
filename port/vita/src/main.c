#include "vita_platform.h"

int main(void) {
    BtgaRuntime runtime;
    BtgaWorldBounds bounds;
    BtgaWorldSpawn spawn;
    uint64_t previous_time;

    if (!vita_platform_startup()) {
        return 1;
    }
    btga_runtime_init(&runtime, vita_platform_assets_ready());
    {
        const uint8_t *intro_data;
        size_t intro_size;
        if (vita_platform_intro_script(&intro_data, &intro_size))
            btga_runtime_set_intro_script(&runtime, intro_data, intro_size);
    }
    if (vita_platform_world_bounds(&bounds)) {
        btga_runtime_set_world_bounds(&runtime, bounds.min_x, bounds.min_z,
                                      bounds.max_x, bounds.max_z);
    }
    btga_runtime_set_terrain(&runtime, vita_platform_terrain());
    if (vita_platform_spawn(0, 0, &spawn)) {
        btga_runtime_set_player_spawn(&runtime, spawn.x, spawn.y, spawn.z, spawn.yaw);
        btga_runtime_set_terrain(&runtime, vita_platform_terrain());
    }
    {
        static const uint8_t one_player_order[BTGA_CAMPAIGN_SLOT_COUNT] =
            {0, 2, 3, 4, 1};
        BtgaMissionConfig config;
        uint8_t slot;
        if (vita_platform_mission_config(&config)) {
            for (slot = 1; slot <= config.ai_count; slot++) {
                uint8_t source_slot = one_player_order[slot];
                if (vita_platform_spawn(0, source_slot, &spawn))
                    btga_runtime_add_enemy_configured(
                        &runtime, spawn.x, spawn.y, spawn.z, spawn.yaw,
                        (uint8_t)config.tank_kinds[source_slot],
                        config.teams[source_slot]);
            }
        }
    }
    {
        size_t index;
        BtgaWorldPickup pickup;
        for (index = 0; vita_platform_pickup(index, &pickup); index++)
            btga_runtime_add_pickup(&runtime, pickup.x, pickup.y, pickup.z,
                                    pickup.kind);
    }
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
