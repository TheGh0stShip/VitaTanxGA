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

static void spawn_projectile(BtgaRuntime *runtime, float x, float y, float z,
                             float heading, uint8_t owner, uint8_t team) {
    unsigned int index;
    if (owner == 0 &&
        (runtime->player.ammo == 0 || runtime->player.fire_cooldown != 0)) return;
    for (index = 0; index < BTGA_MAX_PROJECTILES; index++) {
        BtgaProjectile *projectile = &runtime->projectiles[index];
        if (projectile->active) continue;
        projectile->x = x + sinf(heading) * 55.0f;
        projectile->y = y + 35.0f;
        projectile->z = z + cosf(heading) * 55.0f;
        projectile->velocity_x = sinf(heading) * 95.0f;
        projectile->velocity_z = cosf(heading) * 95.0f;
        projectile->lifetime = 75;
        projectile->owner = owner;
        projectile->team = team;
        projectile->active = true;
        if (owner == 0) {
            runtime->player.ammo--;
            runtime->player.fire_cooldown = 8;
        }
        return;
    }
}

static float angle_delta(float target, float current) {
    float delta = fmodf(target - current, 6.28318530718f);
    if (delta > 3.14159265359f) delta -= 6.28318530718f;
    if (delta < -3.14159265359f) delta += 6.28318530718f;
    return delta;
}

static void update_enemies(BtgaRuntime *runtime) {
    unsigned int index;
    for (index = 0; index < BTGA_MAX_ENEMIES; index++) {
        BtgaEnemy *enemy = &runtime->enemies[index];
        float dx, dz, distance, target_yaw, turn;
        float target_x = runtime->player.x;
        float target_z = runtime->player.z;
        unsigned int target_index;
        if (!enemy->active) continue;
        if (enemy->allied) {
            float best = 1.0e30f;
            bool found = false;
            for (target_index = 0; target_index < BTGA_MAX_ENEMIES; target_index++) {
                const BtgaEnemy *candidate = &runtime->enemies[target_index];
                float tx, tz, squared;
                if (!candidate->active || candidate->allied) continue;
                tx = candidate->x - enemy->x;
                tz = candidate->z - enemy->z;
                squared = tx * tx + tz * tz;
                if (squared < best) {
                    best = squared;
                    target_x = candidate->x;
                    target_z = candidate->z;
                    found = true;
                }
            }
            if (!found) continue;
        } else {
            float best = (runtime->player.x - enemy->x) *
                         (runtime->player.x - enemy->x) +
                         (runtime->player.z - enemy->z) *
                         (runtime->player.z - enemy->z);
            for (target_index = 0; target_index < BTGA_MAX_ENEMIES; target_index++) {
                const BtgaEnemy *candidate = &runtime->enemies[target_index];
                float tx, tz, squared;
                if (!candidate->active || !candidate->allied) continue;
                tx = candidate->x - enemy->x;
                tz = candidate->z - enemy->z;
                squared = tx * tx + tz * tz;
                if (squared < best) {
                    best = squared;
                    target_x = candidate->x;
                    target_z = candidate->z;
                }
            }
        }
        dx = target_x - enemy->x;
        dz = target_z - enemy->z;
        distance = sqrtf(dx * dx + dz * dz);
        target_yaw = atan2f(dx, dz);
        turn = clampf(angle_delta(target_yaw, enemy->yaw), -0.035f, 0.035f);
        enemy->yaw += turn;
        enemy->velocity += ((distance > 420.0f ? 20.0f : 0.0f) - enemy->velocity) * 0.1f;
        enemy->x += sinf(enemy->yaw) * enemy->velocity;
        enemy->z += cosf(enemy->yaw) * enemy->velocity;
        enemy->x = clampf(enemy->x, runtime->world_min_x, runtime->world_max_x);
        enemy->z = clampf(enemy->z, runtime->world_min_z, runtime->world_max_z);
        if (runtime->terrain != NULL) {
            float height;
            if (btga_terrain_height(runtime->terrain, enemy->x, enemy->z, &height))
                enemy->y = height;
        }
        if (enemy->fire_cooldown != 0) enemy->fire_cooldown--;
        if (distance < 1400.0f && fabsf(angle_delta(target_yaw, enemy->yaw)) < 0.12f &&
            enemy->fire_cooldown == 0) {
            spawn_projectile(runtime, enemy->x, enemy->y, enemy->z,
                             enemy->yaw, (uint8_t)(index + 1),
                             enemy->allied ? 0 : 1);
            enemy->fire_cooldown = 45;
        }
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
    if (runtime->pickup_message_ticks != 0) runtime->pickup_message_ticks--;
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
    if ((pressed & BTGA_BUTTON_FIRE) != 0)
        spawn_projectile(runtime, runtime->player.x, runtime->player.y,
                         runtime->player.z,
                         runtime->player.yaw + runtime->player.turret_yaw, 0, 0);
    update_enemies(runtime);
    for (index = 0; index < runtime->pickup_count; index++) {
        BtgaPickup *pickup = &runtime->pickups[index];
        float dx = pickup->x - runtime->player.x;
        float dz = pickup->z - runtime->player.z;
        if (!pickup->active || dx * dx + dz * dz > 4900.0f) continue;
        pickup->active = false;
        runtime->last_pickup_kind = pickup->kind;
        runtime->pickup_message_ticks = 90;
        switch (pickup->kind) {
            case 1: case 2: case 3: case 9: case 10: case 17:
                runtime->pickup_inventory[pickup->kind] += 5;
                break;
            case 4: runtime->pickup_inventory[4] += 2; break;
            case 5: runtime->pickup_inventory[5] += 1; break;
            case 6: case 14: runtime->pickup_inventory[pickup->kind] += 20; break;
            case 7:
                if (runtime->player.health < runtime->player.max_health) {
                    uint16_t health = (uint16_t)(runtime->player.health + 30);
                    runtime->player.health = health > runtime->player.max_health
                        ? runtime->player.max_health : health;
                } else runtime->pickup_inventory[11]++;
                break;
            case 8:
                runtime->player.ammo = runtime->player.ammo + 16u > runtime->player.max_ammo
                    ? runtime->player.max_ammo : (uint16_t)(runtime->player.ammo + 16u);
                runtime->player.health = runtime->player.health + 15u > runtime->player.max_health
                    ? runtime->player.max_health : (uint16_t)(runtime->player.health + 15u);
                break;
            case 11:
                runtime->player.ammo = runtime->player.ammo +
                    runtime->player.max_ammo / 3u > runtime->player.max_ammo
                    ? runtime->player.max_ammo
                    : (uint16_t)(runtime->player.ammo + runtime->player.max_ammo / 3u);
                break;
            case 12: runtime->pickup_inventory[12] += 5; break;
            case 13: runtime->pickup_inventory[13] = 100; break;
            case 15: runtime->pickup_inventory[15] += 100; break;
            case 16: runtime->pickup_inventory[16] += 2; break;
            case 18: runtime->pickup_inventory[18] += 5; break;
            case 19: runtime->pickup_inventory[19] = 1; break;
            default: break;
        }
    }
    for (index = 0; index < BTGA_MAX_PROJECTILES; index++) {
        BtgaProjectile *projectile = &runtime->projectiles[index];
        if (!projectile->active) continue;
        projectile->x += projectile->velocity_x;
        projectile->z += projectile->velocity_z;
        if (projectile->team == 0) {
            unsigned int enemy_index;
            for (enemy_index = 0; enemy_index < BTGA_MAX_ENEMIES; enemy_index++) {
                BtgaEnemy *enemy = &runtime->enemies[enemy_index];
                float dx = projectile->x - enemy->x;
                float dz = projectile->z - enemy->z;
                if (enemy->active && !enemy->allied &&
                    dx * dx + dz * dz < 2500.0f) {
                    enemy->health = enemy->health > 25 ? enemy->health - 25 : 0;
                    if (enemy->health == 0) enemy->active = false;
                    projectile->active = false;
                    break;
                }
            }
        } else {
            float dx = projectile->x - runtime->player.x;
            float dz = projectile->z - runtime->player.z;
            if (dx * dx + dz * dz < 2500.0f) {
                runtime->player.health = runtime->player.health > 10
                    ? runtime->player.health - 10 : 0;
                projectile->active = false;
            }
            if (projectile->active) {
                unsigned int ally_index;
                for (ally_index = 0; ally_index < BTGA_MAX_ENEMIES; ally_index++) {
                    BtgaEnemy *ally = &runtime->enemies[ally_index];
                    dx = projectile->x - ally->x;
                    dz = projectile->z - ally->z;
                    if (ally->active && ally->allied &&
                        dx * dx + dz * dz < 2500.0f) {
                        ally->health = ally->health > 10 ? ally->health - 10 : 0;
                        if (ally->health == 0) ally->active = false;
                        projectile->active = false;
                        break;
                    }
                }
            }
        }
        if (!projectile->active) continue;
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
    if (runtime->player.health == 0) runtime->screen = BTGA_SCREEN_DEFEAT;
    else if (btga_runtime_active_enemies(runtime) == 0)
        runtime->screen = BTGA_SCREEN_VICTORY;
}

uint32_t btga_runtime_active_enemies(const BtgaRuntime *runtime) {
    uint32_t count = 0;
    unsigned int index;
    if (runtime == NULL) return 0;
    for (index = 0; index < BTGA_MAX_ENEMIES; index++)
        if (runtime->enemies[index].active && !runtime->enemies[index].allied) count++;
    return count;
}

uint32_t btga_runtime_active_allies(const BtgaRuntime *runtime) {
    uint32_t count = 0;
    unsigned int index;
    if (runtime == NULL) return 0;
    for (index = 0; index < BTGA_MAX_ENEMIES; index++)
        if (runtime->enemies[index].active && runtime->enemies[index].allied) count++;
    return count;
}

uint32_t btga_runtime_active_pickups(const BtgaRuntime *runtime) {
    uint32_t count = 0;
    unsigned int index;
    if (runtime == NULL) return 0;
    for (index = 0; index < runtime->pickup_count; index++)
        if (runtime->pickups[index].active) count++;
    return count;
}

const char *btga_runtime_pickup_name(uint8_t kind) {
    static const char *const names[BTGA_PICKUP_KIND_COUNT] = {
        "POWERUP", "SWARMERS", "LASERS", "GUIDED MISSILES", "GUN BUDDIES",
        "NUKE", "MINES", "HEALTH", "STAR", "TELEPORTERS", "PLASMA BOLTS",
        "AMMO", "TURBO", "SHIELDS", "BOUNCING BETTIES", "FLAMETHROWER",
        "CLOAKING", "GRENADES", "TANK BUCKS", "RADAR",
    };
    return kind < BTGA_PICKUP_KIND_COUNT ? names[kind] : "POWERUP";
}

bool btga_runtime_add_pickup(BtgaRuntime *runtime, float x, float y, float z,
                             uint8_t kind) {
    BtgaPickup *pickup;
    if (runtime == NULL || kind >= BTGA_PICKUP_KIND_COUNT ||
        runtime->pickup_count >= BTGA_MAX_PICKUPS) return false;
    pickup = &runtime->pickups[runtime->pickup_count++];
    *pickup = (BtgaPickup){x, y, z, kind, true};
    return true;
}

bool btga_runtime_add_enemy(BtgaRuntime *runtime, float x, float y, float z,
                            uint16_t yaw) {
    return btga_runtime_add_enemy_kind(runtime, x, y, z, yaw, 0);
}

bool btga_runtime_add_enemy_kind(BtgaRuntime *runtime, float x, float y, float z,
                                 uint16_t yaw, uint8_t tank_kind) {
    unsigned int index;
    if (runtime == NULL) return false;
    for (index = 0; index < BTGA_MAX_ENEMIES; index++) {
        if (runtime->enemies[index].active) continue;
        runtime->enemies[index] = (BtgaEnemy){
            .x = x, .y = y, .z = z,
            .yaw = (float)yaw * (6.28318530718f / 65536.0f),
            .spawn_x = x, .spawn_y = y, .spawn_z = z,
            .spawn_yaw = (float)yaw * (6.28318530718f / 65536.0f),
            .health = 100, .fire_cooldown = 30,
            .tank_kind = tank_kind,
            .configured = true, .active = true,
        };
        return true;
    }
    return false;
}

bool btga_runtime_add_ally(BtgaRuntime *runtime, float x, float y, float z,
                           uint16_t yaw) {
    unsigned int index;
    if (!btga_runtime_add_enemy(runtime, x, y, z, yaw)) return false;
    for (index = 0; index < BTGA_MAX_ENEMIES; index++) {
        BtgaEnemy *actor = &runtime->enemies[index];
        if (actor->configured && actor->spawn_x == x && actor->spawn_y == y &&
            actor->spawn_z == z && !actor->allied) {
            actor->allied = true;
            return true;
        }
    }
    return false;
}

void btga_runtime_restart_mission(BtgaRuntime *runtime) {
    unsigned int index;
    if (runtime == NULL) return;
    runtime->player.x = runtime->player_spawn_x;
    runtime->player.y = runtime->player_spawn_y;
    runtime->player.z = runtime->player_spawn_z;
    runtime->player.yaw = runtime->player_spawn_yaw;
    runtime->player.turret_yaw = 0.0f;
    runtime->player.velocity = 0.0f;
    runtime->player.health = runtime->player.max_health;
    runtime->player.ammo = runtime->player.max_ammo;
    runtime->player.fire_cooldown = 0;
    for (index = 0; index < BTGA_MAX_PROJECTILES; index++)
        runtime->projectiles[index].active = false;
    for (index = 0; index < runtime->pickup_count; index++)
        runtime->pickups[index].active = true;
    for (index = 0; index < BTGA_PICKUP_KIND_COUNT; index++)
        runtime->pickup_inventory[index] = 0;
    runtime->pickup_message_ticks = 0;
    for (index = 0; index < BTGA_MAX_ENEMIES; index++) {
        BtgaEnemy *enemy = &runtime->enemies[index];
        if (!enemy->configured) continue;
        enemy->x = enemy->spawn_x;
        enemy->y = enemy->spawn_y;
        enemy->z = enemy->spawn_z;
        enemy->yaw = enemy->spawn_yaw;
        enemy->velocity = 0.0f;
        enemy->health = 100;
        enemy->fire_cooldown = 30;
        enemy->active = true;
    }
    btga_runtime_set_player_spawn(runtime, runtime->player.x, runtime->player.y,
                                  runtime->player.z,
                                  (uint16_t)(runtime->player.yaw *
                                             (65536.0f / 6.28318530718f)));
}

bool btga_runtime_set_intro_script(BtgaRuntime *runtime, const uint8_t *data,
                                   size_t size) {
    if (runtime == NULL) return false;
    runtime->intro_ready = btga_script_cursor_init(&runtime->intro, data, size);
    return runtime->intro_ready;
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
    runtime->player_spawn_x = x;
    runtime->player_spawn_y = y;
    runtime->player_spawn_z = z;
    runtime->player_spawn_yaw = heading;
}

void btga_runtime_init(BtgaRuntime *runtime, bool assets_ready) {
    if (runtime == NULL) {
        return;
    }

    *runtime = (BtgaRuntime){
        .camera = {0.0f, 180.0f, 246.0f, 0.0f, -0.42f},
        .player = {
            .x = 0.0f, .y = 0.0f, .z = 576.0f,
            .health = 100, .ammo = 100, .max_health = 100, .max_ammo = 100,
        },
        .world_min_x = -9000.0f,
        .world_min_z = -9000.0f,
        .world_max_x = 9000.0f,
        .world_max_z = 9000.0f,
        .player_spawn_x = 0.0f,
        .player_spawn_y = 0.0f,
        .player_spawn_z = 576.0f,
        .player_spawn_yaw = 0.0f,
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
        btga_runtime_restart_mission(runtime);
        if (runtime->intro_ready) {
            const uint8_t *data = runtime->intro.data;
            size_t size = runtime->intro.size;
            runtime->intro_ready = btga_script_cursor_init(&runtime->intro,
                                                            data, size);
        }
        runtime->screen = runtime->intro_ready ? BTGA_SCREEN_INTRO
                                               : BTGA_SCREEN_BRIEFING;
        runtime->pending_pressed &= ~BTGA_BUTTON_CONFIRM;
    } else if (runtime->screen == BTGA_SCREEN_INTRO &&
               (input->pressed & BTGA_BUTTON_CANCEL) != 0) {
        runtime->screen = BTGA_SCREEN_BRIEFING;
        runtime->pending_pressed &= ~BTGA_BUTTON_CANCEL;
    } else if (runtime->screen == BTGA_SCREEN_BRIEFING &&
               (input->pressed & BTGA_BUTTON_CONFIRM) != 0) {
        runtime->screen = BTGA_SCREEN_GAMEPLAY;
        runtime->pending_pressed &= ~BTGA_BUTTON_CONFIRM;
    } else if (runtime->screen == BTGA_SCREEN_BRIEFING &&
               (input->pressed & BTGA_BUTTON_CANCEL) != 0) {
        runtime->screen = BTGA_SCREEN_TITLE;
        runtime->pending_pressed &= ~BTGA_BUTTON_CANCEL;
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
    } else if ((runtime->screen == BTGA_SCREEN_VICTORY ||
                runtime->screen == BTGA_SCREEN_DEFEAT) &&
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
        else if (runtime->screen == BTGA_SCREEN_INTRO) {
            BtgaScriptStepResult result = btga_script_cursor_tick(
                &runtime->intro,
                (runtime->pending_pressed & BTGA_BUTTON_CONFIRM) != 0);
            if (result == BTGA_SCRIPT_FINISHED || result == BTGA_SCRIPT_INVALID)
                runtime->screen = BTGA_SCREEN_BRIEFING;
        }
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
