#include "btga_runtime.h"

#include <stddef.h>
#include <math.h>

enum {
    BTGA_MAX_FRAME_US = 250000,
    BTGA_MAX_STEPS_PER_FRAME = 4,
};

static float clampf(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void spawn_projectile(BtgaRuntime *runtime) {
    float heading = runtime->player.yaw + runtime->player.turret_yaw;
    unsigned int index;
    if (runtime->player.ammo == 0 || runtime->player.fire_cooldown != 0) return;
    for (index = 0; index < BTGA_MAX_PROJECTILES; index++) {
        BtgaProjectile *projectile = &runtime->projectiles[index];
        if (projectile->active) continue;
        projectile->x = runtime->player.x + sinf(heading) * 55.0f;
        projectile->y = runtime->player.y + 35.0f;
        projectile->z = runtime->player.z + cosf(heading) * 55.0f;
        projectile->velocity_x = sinf(heading) * 95.0f;
        projectile->velocity_z = cosf(heading) * 95.0f;
        projectile->lifetime = 75;
        projectile->active = true;
        runtime->player.ammo--;
        runtime->player.fire_cooldown = 8;
        return;
    }
}

static void update_gameplay(BtgaRuntime *runtime, const BtgaInput *input,
                            uint32_t pressed) {
    float throttle = -(float)input->move_y / 32768.0f;
    float steering = (float)input->move_x / 32768.0f;
    float target_velocity = throttle * 42.0f;
    float follow_x;
    float follow_y;
    float follow_z;
    unsigned int index;
    runtime->player.yaw += steering * (0.018f + fabsf(throttle) * 0.032f);
    runtime->player.turret_yaw += (float)input->aim_x / 32768.0f * 0.055f;
    runtime->player.velocity += (target_velocity - runtime->player.velocity) * 0.16f;
    runtime->player.x += sinf(runtime->player.yaw) * runtime->player.velocity;
    runtime->player.z += cosf(runtime->player.yaw) * runtime->player.velocity;
    runtime->player.x = clampf(runtime->player.x, runtime->world_min_x,
                               runtime->world_max_x);
    runtime->player.z = clampf(runtime->player.z, runtime->world_min_z,
                               runtime->world_max_z);
    if (runtime->terrain != NULL) {
        float terrain_height;
        if (btga_terrain_height(runtime->terrain, runtime->player.x, runtime->player.z,
                                &terrain_height)) runtime->player.y = terrain_height;
    }
    if (runtime->player.fire_cooldown != 0) runtime->player.fire_cooldown--;
    if ((pressed & BTGA_BUTTON_FIRE) != 0) spawn_projectile(runtime);
    for (index = 0; index < BTGA_MAX_PROJECTILES; index++) {
        BtgaProjectile *projectile = &runtime->projectiles[index];
        if (!projectile->active) continue;
        projectile->x += projectile->velocity_x;
        projectile->z += projectile->velocity_z;
        if (--projectile->lifetime == 0 || projectile->x < runtime->world_min_x ||
            projectile->x > runtime->world_max_x || projectile->z < runtime->world_min_z ||
            projectile->z > runtime->world_max_z) projectile->active = false;
    }
    follow_x = runtime->player.x - sinf(runtime->player.yaw) * 330.0f;
    follow_y = runtime->player.y + 180.0f;
    follow_z = runtime->player.z - cosf(runtime->player.yaw) * 330.0f;
    runtime->camera.x += (follow_x - runtime->camera.x) * 0.18f;
    runtime->camera.y += (follow_y - runtime->camera.y) * 0.18f;
    runtime->camera.z += (follow_z - runtime->camera.z) * 0.18f;
    runtime->camera.yaw = runtime->player.yaw;
    runtime->camera.pitch = -0.42f;
}

void btga_runtime_set_terrain(BtgaRuntime *runtime, const BtgaTerrain *terrain) {
    float height;
    if (runtime == NULL) return;
    runtime->terrain = terrain;
    if (terrain != NULL && btga_terrain_height(terrain, runtime->player.x,
                                               runtime->player.z, &height)) {
        runtime->camera.y += height - runtime->player.y;
        runtime->player.y = height;
    }
}

void btga_runtime_set_player_spawn(BtgaRuntime *runtime, float x, float y,
                                   float z, uint16_t yaw) {
    float heading;
    if (runtime == NULL) return;
    heading = (float)yaw * (6.28318530718f / 65536.0f);
    runtime->player.x = x;
    runtime->player.y = y;
    runtime->player.z = z;
    runtime->player.yaw = heading;
    runtime->player.turret_yaw = 0.0f;
    runtime->camera.x = x - sinf(heading) * 330.0f;
    runtime->camera.y = y + 180.0f;
    runtime->camera.z = z - cosf(heading) * 330.0f;
    runtime->camera.yaw = heading;
    runtime->camera.pitch = -0.42f;
}

void btga_runtime_init(BtgaRuntime *runtime, bool assets_ready) {
    if (runtime == NULL) {
        return;
    }

    *runtime = (BtgaRuntime){
        .camera = {0.0f, 180.0f, 246.0f, 0.0f, -0.42f},
        .player = {0.0f, 0.0f, 576.0f, 0.0f, 0.0f, 0.0f, 100, 200, 0},
        .world_min_x = -9000.0f,
        .world_min_z = -9000.0f,
        .world_max_x = 9000.0f,
        .world_max_z = 9000.0f,
        .assets_ready = assets_ready,
        .running = true,
    };
}

void btga_runtime_set_world_bounds(BtgaRuntime *runtime, float min_x, float min_z,
                                   float max_x, float max_z) {
    if (runtime == NULL || min_x >= max_x || min_z >= max_z) return;
    runtime->world_min_x = min_x;
    runtime->world_min_z = min_z;
    runtime->world_max_x = max_x;
    runtime->world_max_z = max_z;
    runtime->player.x = clampf(runtime->player.x, min_x, max_x);
    runtime->player.z = clampf(runtime->player.z, min_z, max_z);
}

void btga_runtime_update(BtgaRuntime *runtime, const BtgaInput *input,
                         uint32_t elapsed_us) {
    unsigned int steps = 0;

    if (runtime == NULL || input == NULL || !runtime->running) {
        return;
    }

    runtime->pending_pressed |= input->pressed;
    if ((input->pressed & BTGA_BUTTON_SELECT) != 0) {
        runtime->running = false;
        return;
    }
    if (runtime->screen == BTGA_SCREEN_TITLE &&
        (input->pressed & BTGA_BUTTON_CONFIRM) != 0 && runtime->assets_ready) {
        runtime->screen = BTGA_SCREEN_GAMEPLAY;
        runtime->pending_pressed &= ~BTGA_BUTTON_CONFIRM;
    } else if (runtime->screen == BTGA_SCREEN_GAMEPLAY &&
               (input->pressed & BTGA_BUTTON_START) != 0) {
        runtime->screen = BTGA_SCREEN_PAUSED;
        runtime->pending_pressed &= ~BTGA_BUTTON_START;
    } else if (runtime->screen == BTGA_SCREEN_PAUSED &&
               (input->pressed & BTGA_BUTTON_START) != 0) {
        runtime->screen = BTGA_SCREEN_GAMEPLAY;
        runtime->pending_pressed &= ~BTGA_BUTTON_START;
    } else if (runtime->screen == BTGA_SCREEN_PAUSED &&
               (input->pressed & BTGA_BUTTON_CANCEL) != 0) {
        runtime->screen = BTGA_SCREEN_TITLE;
        runtime->pending_pressed &= ~BTGA_BUTTON_CANCEL;
    }

    if (elapsed_us > BTGA_MAX_FRAME_US) {
        elapsed_us = BTGA_MAX_FRAME_US;
    }
    runtime->accumulator_us += elapsed_us;
    while (runtime->accumulator_us >= BTGA_SIMULATION_STEP_US &&
           steps < BTGA_MAX_STEPS_PER_FRAME) {
        if (runtime->screen == BTGA_SCREEN_GAMEPLAY)
            update_gameplay(runtime, input, runtime->pending_pressed);
        runtime->pending_pressed = 0;
        runtime->accumulator_us -= BTGA_SIMULATION_STEP_US;
        runtime->simulation_ticks++;
        steps++;
    }
    if (steps == BTGA_MAX_STEPS_PER_FRAME &&
        runtime->accumulator_us >= BTGA_SIMULATION_STEP_US) {
        runtime->accumulator_us %= BTGA_SIMULATION_STEP_US;
    }
}
