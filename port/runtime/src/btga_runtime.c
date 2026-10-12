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

static void damage_enemy(BtgaEnemy *enemy, uint16_t damage) {
    if (enemy == NULL || !enemy->active || enemy->allied) return;
    enemy->health = enemy->health > damage ?
        (uint16_t)(enemy->health - damage) : 0;
    if (enemy->health == 0) enemy->active = false;
}

static void damage_enemies_in_radius(BtgaRuntime *runtime, float x, float z,
                                     uint16_t radius, uint16_t damage) {
    unsigned int index;
    float radius_squared = (float)radius * (float)radius;
    for (index = 0; index < BTGA_MAX_ENEMIES; index++) {
        BtgaEnemy *enemy = &runtime->enemies[index];
        float dx = x - enemy->x;
        float dz = z - enemy->z;
        if (enemy->active && !enemy->allied &&
            dx * dx + dz * dz <= radius_squared)
            damage_enemy(enemy, damage);
    }
}

static bool point_in_object(const BtgaMissionObject *object, float x, float y,
                            float z, float padding) {
    float angle = (float)object->world.yaw * (6.28318530718f / 65536.0f);
    float sine = sinf(angle), cosine = cosf(angle);
    float dx = x - object->world.x, dz = z - object->world.z;
    float local_x = cosine * dx - sine * dz;
    float local_z = sine * dx + cosine * dz;
    float local_y = y - object->world.y;
    return local_x >= object->world.min_x - padding &&
           local_x <= object->world.max_x + padding &&
           local_y >= object->world.min_y - padding &&
           local_y <= object->world.max_y + padding &&
           local_z >= object->world.min_z - padding &&
           local_z <= object->world.max_z + padding;
}

static void damage_object(BtgaRuntime *runtime, BtgaMissionObject *object,
                          uint16_t damage) {
    if (!object->active) return;
    object->health = object->health > damage ?
        (uint16_t)(object->health - damage) : 0;
    if (object->health != 0) return;
    object->active = false;
    if (object->objective && runtime->objective_objects_remaining != 0)
        runtime->objective_objects_remaining--;
}

static void damage_objects_in_radius(BtgaRuntime *runtime, float x, float y,
                                     float z, uint16_t radius,
                                     uint16_t damage) {
    uint16_t index;
    for (index = 0; index < runtime->mission_object_count; index++) {
        BtgaMissionObject *object = &runtime->mission_objects[index];
        if (object->active && point_in_object(object, x, y, z, radius))
            damage_object(runtime, object, damage);
    }
}

static uint16_t radians_to_yaw(float angle) {
    float turns = fmodf(angle, 6.28318530718f);
    if (turns < 0.0f) turns += 6.28318530718f;
    return (uint16_t)(turns * (65536.0f / 6.28318530718f));
}

static uint8_t weapon_sound_effect(uint8_t weapon) {
    static const uint8_t effects[BTGA_WEAPON_COUNT] = {
        0, 3, 0x14, 0x12, 0x14, 0, 0x13, 0x41, 0,
        0x3A, 0x2F, 0x3B, 0x36, 0x37, 0x2E, 0x34, 0x17, 0x36,
    };
    return weapon < BTGA_WEAPON_COUNT ? effects[weapon] : 0;
}

static void queue_sound(BtgaRuntime *runtime, uint8_t effect) {
    if (effect == 0) return;
    runtime->sound_effect = effect;
    runtime->sound_serial++;
}

static bool spawn_projectile(BtgaRuntime *runtime, float x, float y, float z,
                             float heading, uint8_t owner, uint8_t team,
                             uint8_t weapon, uint16_t damage, float speed,
                             uint16_t lifetime, uint16_t blast_radius,
                             bool homing) {
    unsigned int index;
    for (index = 0; index < BTGA_MAX_PROJECTILES; index++) {
        BtgaProjectile *projectile = &runtime->projectiles[index];
        if (projectile->active) continue;
        projectile->x = x + sinf(heading) * 55.0f;
        projectile->y = y + 35.0f;
        projectile->z = z + cosf(heading) * 55.0f;
        projectile->velocity_x = sinf(heading) * speed;
        projectile->velocity_y = weapon == 14 ? 18.0f : 0.0f;
        projectile->velocity_z = cosf(heading) * speed;
        projectile->gravity = weapon == 14 ? 0.9f : 0.0f;
        projectile->lifetime = lifetime;
        projectile->damage = damage;
        projectile->blast_radius = blast_radius;
        projectile->owner = owner;
        projectile->team = team;
        projectile->weapon = weapon;
        projectile->homing = homing;
        projectile->active = true;
        queue_sound(runtime, weapon_sound_effect(weapon));
        return true;
    }
    return false;
}

static bool tank_blocked(const BtgaRuntime *runtime, float x, float y, float z) {
    size_t index;
    const float radius = 38.0f;
    for (index = 0; index < runtime->collision_count; index++) {
        const BtgaWorldCollision *collision = &runtime->collisions[index];
        float angle = (float)collision->yaw * (6.28318530718f / 65536.0f);
        float sine = sinf(angle);
        float cosine = cosf(angle);
        float dx = x - collision->x;
        float dz = z - collision->z;
        float local_x = cosine * dx - sine * dz;
        float local_z = sine * dx + cosine * dz;
        float local_y = y - collision->y;
        if (local_x >= (float)collision->min_x - radius &&
            local_x <= (float)collision->max_x + radius &&
            local_z >= (float)collision->min_z - radius &&
            local_z <= (float)collision->max_z + radius &&
            local_y >= (float)collision->min_y - 80.0f &&
            local_y <= (float)collision->max_y + 80.0f) return true;
    }
    return false;
}

uint8_t btga_runtime_pickup_weapon(uint8_t kind) {
    static const uint8_t weapons[BTGA_PICKUP_KIND_COUNT] = {
        0, 2, 3, 4, 16, 7, 6, 0, 0, 8,
        9, 0, 10, 0, 15, 12, 13, 14, 0, 0,
    };
    return kind < BTGA_PICKUP_KIND_COUNT ? weapons[kind] : 0;
}

uint16_t btga_runtime_pickup_weapon_amount(uint8_t kind) {
    static const uint16_t amounts[BTGA_PICKUP_KIND_COUNT] = {
        0, 5, 5, 5, 2, 1, 20, 0, 0, 5,
        5, 0, 5, 0, 20, 100, 2, 5, 0, 0,
    };
    return kind < BTGA_PICKUP_KIND_COUNT ? amounts[kind] : 0;
}

uint8_t btga_runtime_weapon_cooldown(uint8_t weapon) {
    /* D_801146D4: retail mode lifetimes used by func_8008E620. */
    static const uint8_t cooldowns[BTGA_WEAPON_COUNT] = {
        0, 0, 30, 30, 90, 30, 10, 30, 30,
        30, 30, 30, 2, 30, 30, 10, 30, 30,
    };
    return weapon < BTGA_WEAPON_COUNT ? cooldowns[weapon] : 0;
}

static void cycle_weapon(BtgaRuntime *runtime) {
    uint8_t weapon = runtime->player.selected_weapon;
    uint8_t checked;
    for (checked = 0; checked < BTGA_WEAPON_COUNT - 2; checked++) {
        weapon = weapon < 2 || weapon + 1 >= BTGA_WEAPON_COUNT
            ? 2 : (uint8_t)(weapon + 1);
        if (runtime->weapon_ammo[weapon] != 0) {
            runtime->player.selected_weapon = weapon;
            return;
        }
    }
    runtime->player.selected_weapon = 0;
}

static void fire_secondary(BtgaRuntime *runtime) {
    uint8_t weapon = runtime->player.selected_weapon;
    float heading = runtime->player.yaw + runtime->player.turret_yaw;
    bool fired = false;
    if (weapon < 2 || weapon >= BTGA_WEAPON_COUNT ||
        runtime->weapon_ammo[weapon] == 0 ||
        runtime->player.secondary_cooldown != 0) return;
    switch (weapon) {
        case 2: {
            unsigned int shot;
            static const float spread[3] = {-0.10f, 0.0f, 0.10f};
            for (shot = 0; shot < 3; shot++)
                fired |= spawn_projectile(runtime, runtime->player.x,
                    runtime->player.y, runtime->player.z, heading + spread[shot],
                    0, 0, weapon, 30, 80.0f, 75, 0, false);
            break;
        }
        case 3:
            fired = spawn_projectile(runtime, runtime->player.x, runtime->player.y,
                runtime->player.z, heading, 0, 0, weapon, 35, 180.0f,
                45, 0, false);
            break;
        case 4:
            fired = spawn_projectile(runtime, runtime->player.x, runtime->player.y,
                runtime->player.z, heading, 0, 0, weapon, 70, 25.0f,
                180, 0, true);
            break;
        case 6: case 15:
            fired = spawn_projectile(runtime, runtime->player.x,
                runtime->player.y, runtime->player.z, heading + 3.14159265359f,
                0, 0, weapon, weapon == 6 ? 50 : 35, 0.0f,
                900, weapon == 6 ? 120 : 90, false);
            break;
        case 7:
            fired = spawn_projectile(runtime, runtime->player.x, runtime->player.y,
                runtime->player.z, heading, 0, 0, weapon, 100, 45.0f,
                120, 600, false);
            break;
        case 8: {
            float distance = 600.0f;
            runtime->player.x = clampf(runtime->player.x + sinf(heading) * distance,
                                       runtime->world_min_x, runtime->world_max_x);
            runtime->player.z = clampf(runtime->player.z + cosf(heading) * distance,
                                       runtime->world_min_z, runtime->world_max_z);
            fired = true;
            break;
        }
        case 9: {
            unsigned int shot;
            static const float spread[3] = {-0.08f, 0.0f, 0.08f};
            for (shot = 0; shot < 3; shot++)
                fired |= spawn_projectile(runtime, runtime->player.x,
                    runtime->player.y, runtime->player.z, heading + spread[shot],
                    0, 0, weapon, 15, 100.0f, 65, 0, false);
            break;
        }
        case 10: runtime->player.turbo_ticks += 105; fired = true; break;
        case 11:
            runtime->player.ammo = runtime->player.ammo + 15u >
                runtime->player.max_ammo ? runtime->player.max_ammo :
                (uint16_t)(runtime->player.ammo + 15u);
            fired = true;
            break;
        case 12:
            fired = spawn_projectile(runtime, runtime->player.x, runtime->player.y,
                runtime->player.z, heading, 0, 0, weapon, 8, 55.0f,
                18, 70, false);
            break;
        case 13: runtime->player.cloak_ticks += 450; fired = true; break;
        case 14:
            fired = spawn_projectile(runtime, runtime->player.x, runtime->player.y,
                runtime->player.z, heading, 0, 0, weapon, 50, 60.0f,
                90, 140, false);
            break;
        case 16:
            fired = btga_runtime_add_ally(runtime,
                runtime->player.x - sinf(heading) * 100.0f,
                runtime->player.y,
                runtime->player.z - cosf(heading) * 100.0f,
                radians_to_yaw(heading));
            break;
        default:
            fired = spawn_projectile(runtime, runtime->player.x, runtime->player.y,
                runtime->player.z, heading, 0, 0, weapon, 30, 80.0f,
                75, 0, false);
            break;
    }
    if (!fired) return;
    if (weapon == 8 || weapon == 10 || weapon == 11 || weapon == 13 ||
        weapon == 16) queue_sound(runtime, weapon_sound_effect(weapon));
    runtime->weapon_ammo[weapon]--;
    runtime->player.secondary_cooldown = btga_runtime_weapon_cooldown(weapon);
    if (runtime->weapon_ammo[weapon] == 0) cycle_weapon(runtime);
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
        float dx, dz, distance, target_yaw, turn, turret_turn;
        float old_x, old_z;
        float target_x = runtime->player.x;
        float target_z = runtime->player.z;
        unsigned int target_index;
        if (!enemy->active) continue;
        if (!enemy->allied && runtime->player.cloak_ticks != 0 &&
            btga_runtime_active_allies(runtime) == 0) continue;
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
        turret_turn = clampf(angle_delta(target_yaw,
                                         enemy->yaw + enemy->turret_yaw),
                              -0.07f, 0.07f);
        enemy->turret_yaw += turret_turn;
        enemy->turret_yaw = angle_delta(enemy->yaw + enemy->turret_yaw,
                                        enemy->yaw);
        enemy->velocity += ((distance > 420.0f ? 20.0f : 0.0f) - enemy->velocity) * 0.1f;
        old_x = enemy->x;
        old_z = enemy->z;
        enemy->x += sinf(enemy->yaw) * enemy->velocity;
        enemy->z += cosf(enemy->yaw) * enemy->velocity;
        enemy->x = clampf(enemy->x, runtime->world_min_x, runtime->world_max_x);
        enemy->z = clampf(enemy->z, runtime->world_min_z, runtime->world_max_z);
        if (tank_blocked(runtime, enemy->x, enemy->y, enemy->z)) {
            enemy->x = old_x;
            enemy->z = old_z;
            enemy->velocity = 0.0f;
        }
        if (runtime->terrain != NULL) {
            float height;
            if (btga_terrain_height(runtime->terrain, enemy->x, enemy->z, &height))
                enemy->y = height;
        }
        if (enemy->fire_cooldown != 0) enemy->fire_cooldown--;
        if (distance < 1400.0f &&
            fabsf(angle_delta(target_yaw,
                              enemy->yaw + enemy->turret_yaw)) < 0.12f &&
            enemy->fire_cooldown == 0) {
            if (spawn_projectile(runtime, enemy->x, enemy->y, enemy->z,
                                 enemy->yaw + enemy->turret_yaw,
                                 (uint8_t)(index + 1),
                                 enemy->allied ? 0 : 1, 1, 30, 95.0f,
                                 75, 0, false))
                enemy->fire_cooldown = 45;
        }
    }
}

static void update_gameplay(BtgaRuntime *runtime, const BtgaInput *input,
                            uint32_t pressed) {
    float throttle = -(float)input->move_y / 32768.0f;
    float steering = (float)input->move_x / 32768.0f;
    float target_velocity = throttle *
        (runtime->player.turbo_ticks != 0 ? 67.0f : 42.0f);
    float follow_x;
    float follow_y;
    float follow_z;
    unsigned int index;
    float old_player_x = runtime->player.x;
    float old_player_z = runtime->player.z;
    if (runtime->pickup_message_ticks != 0) runtime->pickup_message_ticks--;
    if (runtime->player.turbo_ticks != 0) runtime->player.turbo_ticks--;
    if (runtime->player.cloak_ticks != 0) runtime->player.cloak_ticks--;
    runtime->player.yaw += steering * (0.018f + fabsf(throttle) * 0.032f);
    runtime->player.turret_yaw += (float)input->aim_x / 32768.0f * 0.055f;
    runtime->player.velocity += (target_velocity - runtime->player.velocity) * 0.16f;
    runtime->player.x += sinf(runtime->player.yaw) * runtime->player.velocity;
    runtime->player.z += cosf(runtime->player.yaw) * runtime->player.velocity;
    runtime->player.x = clampf(runtime->player.x, runtime->world_min_x,
                               runtime->world_max_x);
    runtime->player.z = clampf(runtime->player.z, runtime->world_min_z,
                               runtime->world_max_z);
    if (tank_blocked(runtime, runtime->player.x, runtime->player.y,
                     runtime->player.z)) {
        runtime->player.x = old_player_x;
        runtime->player.z = old_player_z;
        runtime->player.velocity = 0.0f;
    }
    if (runtime->terrain != NULL) {
        float terrain_height;
        if (btga_terrain_height(runtime->terrain, runtime->player.x, runtime->player.z,
                                &terrain_height)) runtime->player.y = terrain_height;
    }
    if (runtime->player.fire_cooldown != 0) runtime->player.fire_cooldown--;
    if (runtime->player.secondary_cooldown != 0)
        runtime->player.secondary_cooldown--;
    if ((pressed & BTGA_BUTTON_SELECT) != 0) cycle_weapon(runtime);
    if (((input->held | pressed) & BTGA_BUTTON_FIRE) != 0 &&
        runtime->player.ammo != 0 && runtime->player.fire_cooldown == 0) {
        if (spawn_projectile(runtime, runtime->player.x, runtime->player.y,
                             runtime->player.z,
                             runtime->player.yaw + runtime->player.turret_yaw,
                             0, 0, 1, 30, 95.0f, 75, 0, false)) {
            runtime->player.ammo--;
            runtime->player.fire_cooldown = 8;
        }
    }
    if (((input->held | pressed) & BTGA_BUTTON_ALT_FIRE) != 0)
        fire_secondary(runtime);
    update_enemies(runtime);
    for (index = 0; index < runtime->pickup_count; index++) {
        BtgaPickup *pickup = &runtime->pickups[index];
        float dx = pickup->x - runtime->player.x;
        float dz = pickup->z - runtime->player.z;
        if (!pickup->active || dx * dx + dz * dz > 4900.0f) continue;
        pickup->active = false;
        runtime->last_pickup_kind = pickup->kind;
        runtime->pickup_message_ticks = 90;
        {
            uint8_t weapon = btga_runtime_pickup_weapon(pickup->kind);
            uint16_t amount = btga_runtime_pickup_weapon_amount(pickup->kind);
            if (weapon != 0) {
                runtime->weapon_ammo[weapon] += amount;
                if (runtime->player.selected_weapon == 0)
                    runtime->player.selected_weapon = weapon;
            }
        }
        switch (pickup->kind) {
            case 7:
                if (runtime->player.health < runtime->player.max_health) {
                    uint16_t health = (uint16_t)(runtime->player.health + 30);
                    runtime->player.health = health > runtime->player.max_health
                        ? runtime->player.max_health : health;
                } else runtime->weapon_ammo[11]++;
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
            case 13: runtime->player.shield = 100; break;
            case 18: runtime->player.tank_bucks += 5; break;
            case 19: runtime->player.radar = true; break;
            default: break;
        }
    }
    for (index = 0; index < BTGA_MAX_PROJECTILES; index++) {
        BtgaProjectile *projectile = &runtime->projectiles[index];
        if (!projectile->active) continue;
        if (projectile->homing && projectile->team == 0) {
            BtgaEnemy *target = NULL;
            float best = 1.0e30f;
            unsigned int enemy_index;
            for (enemy_index = 0; enemy_index < BTGA_MAX_ENEMIES;
                 enemy_index++) {
                BtgaEnemy *candidate = &runtime->enemies[enemy_index];
                float dx, dz, squared;
                if (!candidate->active || candidate->allied) continue;
                dx = candidate->x - projectile->x;
                dz = candidate->z - projectile->z;
                squared = dx * dx + dz * dz;
                if (squared < best) {
                    best = squared;
                    target = candidate;
                }
            }
            if (target != NULL && best > 0.0001f) {
                float speed = sqrtf(projectile->velocity_x *
                                    projectile->velocity_x +
                                    projectile->velocity_z *
                                    projectile->velocity_z);
                float inverse_distance = 1.0f / sqrtf(best);
                float desired_x = (target->x - projectile->x) *
                                  inverse_distance * speed;
                float desired_z = (target->z - projectile->z) *
                                  inverse_distance * speed;
                float blended_x = projectile->velocity_x * 0.88f +
                                  desired_x * 0.12f;
                float blended_z = projectile->velocity_z * 0.88f +
                                  desired_z * 0.12f;
                float blended_length = sqrtf(blended_x * blended_x +
                                             blended_z * blended_z);
                if (blended_length > 0.0001f) {
                    projectile->velocity_x = blended_x * speed / blended_length;
                    projectile->velocity_z = blended_z * speed / blended_length;
                }
            }
        }
        projectile->x += projectile->velocity_x;
        projectile->y += projectile->velocity_y;
        projectile->z += projectile->velocity_z;
        projectile->velocity_y -= projectile->gravity;
        if (projectile->team == 0) {
            unsigned int enemy_index;
            for (enemy_index = 0; enemy_index < BTGA_MAX_ENEMIES; enemy_index++) {
                BtgaEnemy *enemy = &runtime->enemies[enemy_index];
                float dx = projectile->x - enemy->x;
                float dz = projectile->z - enemy->z;
                {
                    float hit_radius = projectile->weapon == 6 ||
                        projectile->weapon == 15
                        ? (float)projectile->blast_radius : 50.0f;
                    if (!enemy->active || enemy->allied ||
                        dx * dx + dz * dz >= hit_radius * hit_radius) continue;
                    if (projectile->blast_radius != 0)
                        damage_enemies_in_radius(runtime, projectile->x,
                            projectile->z, projectile->blast_radius,
                            projectile->damage);
                    else damage_enemy(enemy, projectile->damage);
                    projectile->active = false;
                    break;
                }
            }
            if (projectile->active) {
                uint16_t object_index;
                for (object_index = 0;
                     object_index < runtime->mission_object_count;
                     object_index++) {
                    BtgaMissionObject *object =
                        &runtime->mission_objects[object_index];
                    if (!object->active || !point_in_object(
                            object, projectile->x, projectile->y,
                            projectile->z, 8.0f)) continue;
                    if (projectile->blast_radius != 0)
                        damage_objects_in_radius(runtime, projectile->x,
                            projectile->y, projectile->z,
                            projectile->blast_radius, projectile->damage);
                    else damage_object(runtime, object, projectile->damage);
                    projectile->active = false;
                    break;
                }
            }
        } else {
            float dx = projectile->x - runtime->player.x;
            float dz = projectile->z - runtime->player.z;
            if (dx * dx + dz * dz < 2500.0f) {
                uint16_t damage = projectile->damage;
                if (runtime->player.shield != 0) {
                    uint16_t absorbed = damage < runtime->player.shield
                        ? damage : runtime->player.shield;
                    runtime->player.shield -= absorbed;
                    damage -= absorbed;
                }
                runtime->player.health = runtime->player.health > damage
                    ? runtime->player.health - damage : 0;
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
                        ally->health = ally->health > projectile->damage
                            ? ally->health - projectile->damage : 0;
                        if (ally->health == 0) ally->active = false;
                        projectile->active = false;
                        break;
                    }
                }
            }
        }
        if (!projectile->active) continue;
        if (projectile->gravity != 0.0f && runtime->terrain != NULL) {
            float terrain_height;
            if (btga_terrain_height(runtime->terrain, projectile->x,
                                    projectile->z, &terrain_height) &&
                projectile->y <= terrain_height) {
                if (projectile->team == 0 && projectile->blast_radius != 0)
                    damage_enemies_in_radius(runtime, projectile->x,
                        projectile->z, projectile->blast_radius,
                        projectile->damage);
                if (projectile->team == 0 && projectile->blast_radius != 0)
                    damage_objects_in_radius(runtime, projectile->x,
                        projectile->y, projectile->z,
                        projectile->blast_radius, projectile->damage);
                projectile->active = false;
                continue;
            }
        }
        if (--projectile->lifetime == 0 || projectile->x < runtime->world_min_x ||
            projectile->x > runtime->world_max_x || projectile->z < runtime->world_min_z ||
            projectile->z > runtime->world_max_z) {
            if (projectile->team == 0 && projectile->blast_radius != 0)
                damage_enemies_in_radius(runtime, projectile->x, projectile->z,
                    projectile->blast_radius, projectile->damage);
            if (projectile->team == 0 && projectile->blast_radius != 0)
                damage_objects_in_radius(runtime, projectile->x,
                    projectile->y, projectile->z,
                    projectile->blast_radius, projectile->damage);
            projectile->active = false;
        }
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
    else if (btga_runtime_active_enemies(runtime) == 0 &&
             runtime->objective_objects_remaining == 0)
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

uint32_t btga_runtime_active_objectives(const BtgaRuntime *runtime) {
    return runtime == NULL ? 0 : runtime->objective_objects_remaining;
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

const char *btga_runtime_weapon_name(uint8_t weapon) {
    static const char *const names[BTGA_WEAPON_COUNT] = {
        "NONE", "CANNON", "SWARMERS", "LASERS", "GUIDED MISSILES",
        "POWERUP", "MINES", "NUKE", "TELEPORTERS", "PLASMA BOLTS",
        "TURBO", "HEALTH", "FLAMETHROWER", "CLOAKING", "GRENADES",
        "BOUNCING BETTIES", "GUN BUDDIES", "POWERUP",
    };
    return weapon < BTGA_WEAPON_COUNT ? names[weapon] : "NONE";
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

bool btga_runtime_add_mission_object(BtgaRuntime *runtime,
                                     const BtgaWorldObject *object) {
    BtgaMissionObject *destination;
    bool objective;
    uint16_t health;
    if (runtime == NULL || object == NULL ||
        runtime->mission_object_count >= BTGA_MAX_MISSION_OBJECTS) return false;
    /* The recovered kind-21 handler explicitly supplies mission_type. Keep
       ordinary combat structures destructible without inventing objectives. */
    objective = object->kind == 21 && object->mission_type != 0;
    health = object->kind == 3 ? 300 : object->kind == 4 ? 180 : 100;
    destination = &runtime->mission_objects[runtime->mission_object_count++];
    *destination = (BtgaMissionObject){
        .world = *object, .health = health, .max_health = health,
        .objective = objective, .active = true,
    };
    if (objective) {
        runtime->objective_object_count++;
        runtime->objective_objects_remaining++;
    }
    return true;
}

bool btga_runtime_add_enemy(BtgaRuntime *runtime, float x, float y, float z,
                            uint16_t yaw) {
    return btga_runtime_add_enemy_kind(runtime, x, y, z, yaw, 0);
}

bool btga_runtime_add_enemy_kind(BtgaRuntime *runtime, float x, float y, float z,
                                 uint16_t yaw, uint8_t tank_kind) {
    return btga_runtime_add_enemy_configured(runtime, x, y, z, yaw, tank_kind, 1);
}

bool btga_runtime_add_enemy_configured(BtgaRuntime *runtime, float x, float y,
                                       float z, uint16_t yaw,
                                       uint8_t tank_kind, uint8_t team) {
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
            .tank_kind = tank_kind, .team = team,
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
            actor->team = 0;
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
    runtime->player.shield = 0;
    runtime->player.tank_bucks = 0;
    runtime->player.fire_cooldown = 0;
    runtime->player.secondary_cooldown = 0;
    runtime->player.turbo_ticks = 0;
    runtime->player.cloak_ticks = 0;
    runtime->player.selected_weapon = 0;
    runtime->player.radar = false;
    for (index = 0; index < BTGA_MAX_PROJECTILES; index++)
        runtime->projectiles[index].active = false;
    for (index = 0; index < runtime->pickup_count; index++)
        runtime->pickups[index].active = true;
    runtime->objective_objects_remaining = runtime->objective_object_count;
    for (index = 0; index < runtime->mission_object_count; index++) {
        runtime->mission_objects[index].health =
            runtime->mission_objects[index].max_health;
        runtime->mission_objects[index].active = true;
    }
    for (index = 0; index < BTGA_WEAPON_COUNT; index++)
        runtime->weapon_ammo[index] = 0;
    runtime->pickup_message_ticks = 0;
    for (index = 0; index < BTGA_MAX_ENEMIES; index++) {
        BtgaEnemy *enemy = &runtime->enemies[index];
        if (!enemy->configured) continue;
        enemy->x = enemy->spawn_x;
        enemy->y = enemy->spawn_y;
        enemy->z = enemy->spawn_z;
        enemy->yaw = enemy->spawn_yaw;
        enemy->turret_yaw = 0.0f;
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

void btga_runtime_set_collisions(BtgaRuntime *runtime,
                                 const BtgaWorldCollision *collisions,
                                 size_t collision_count) {
    if (runtime == NULL || (collision_count != 0 && collisions == NULL)) return;
    runtime->collisions = collisions;
    runtime->collision_count = collision_count;
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
