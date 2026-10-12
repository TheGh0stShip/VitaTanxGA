#ifndef VITATANXGA_RUNTIME_H
#define VITATANXGA_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "btga_view.h"
#include "btga_terrain.h"
#include "btga_script.h"

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
    BTGA_BUTTON_SELECT = 1u << 9,
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
    BTGA_SCREEN_TITLE = 0,
    BTGA_SCREEN_GAMEPLAY = 1,
    BTGA_SCREEN_PAUSED = 2,
    BTGA_SCREEN_VICTORY = 3,
    BTGA_SCREEN_DEFEAT = 4,
    BTGA_SCREEN_INTRO = 5,
} BtgaRuntimeScreen;

typedef struct BtgaPlayer {
    float x;
    float y;
    float z;
    float yaw;
    float turret_yaw;
    float velocity;
    uint16_t health;
    uint16_t ammo;
    uint16_t max_health;
    uint16_t max_ammo;
    uint8_t fire_cooldown;
} BtgaPlayer;

typedef struct BtgaProjectile {
    float x;
    float y;
    float z;
    float velocity_x;
    float velocity_z;
    uint16_t lifetime;
    uint8_t owner;
    bool active;
} BtgaProjectile;

typedef struct BtgaEnemy {
    float x, y, z;
    float yaw;
    float spawn_x, spawn_y, spawn_z;
    float spawn_yaw;
    float velocity;
    uint16_t health;
    uint16_t fire_cooldown;
    bool configured;
    bool active;
} BtgaEnemy;

typedef struct BtgaPickup {
    float x, y, z;
    uint8_t kind;
    bool active;
} BtgaPickup;

enum {
    BTGA_MAX_PROJECTILES = 32,
    BTGA_MAX_ENEMIES = 8,
    BTGA_MAX_PICKUPS = 64,
    BTGA_PICKUP_KIND_COUNT = 20,
};

typedef struct BtgaRuntime {
    uint64_t simulation_ticks;
    uint32_t accumulator_us;
    uint32_t pending_pressed;
    BtgaCamera camera;
    BtgaPlayer player;
    BtgaProjectile projectiles[BTGA_MAX_PROJECTILES];
    BtgaEnemy enemies[BTGA_MAX_ENEMIES];
    BtgaPickup pickups[BTGA_MAX_PICKUPS];
    uint16_t pickup_inventory[BTGA_PICKUP_KIND_COUNT];
    uint8_t pickup_count;
    uint8_t last_pickup_kind;
    uint16_t pickup_message_ticks;
    const BtgaTerrain *terrain;
    float world_min_x;
    float world_min_z;
    float world_max_x;
    float world_max_z;
    float player_spawn_x;
    float player_spawn_y;
    float player_spawn_z;
    float player_spawn_yaw;
    BtgaRuntimeScreen screen;
    BtgaScriptCursor intro;
    bool intro_ready;
    bool assets_ready;
    bool running;
} BtgaRuntime;

enum {
    BTGA_SIMULATION_HZ = 30,
    BTGA_SIMULATION_STEP_US = 1000000 / BTGA_SIMULATION_HZ,
};

void btga_runtime_init(BtgaRuntime *runtime, bool assets_ready);
void btga_runtime_set_world_bounds(BtgaRuntime *runtime, float min_x, float min_z,
                                   float max_x, float max_z);
void btga_runtime_set_terrain(BtgaRuntime *runtime, const BtgaTerrain *terrain);
void btga_runtime_set_player_spawn(BtgaRuntime *runtime, float x, float y,
                                   float z, uint16_t yaw);
bool btga_runtime_add_enemy(BtgaRuntime *runtime, float x, float y, float z,
                            uint16_t yaw);
bool btga_runtime_add_pickup(BtgaRuntime *runtime, float x, float y, float z,
                             uint8_t kind);
bool btga_runtime_set_intro_script(BtgaRuntime *runtime, const uint8_t *data,
                                   size_t size);
void btga_runtime_update(BtgaRuntime *runtime, const BtgaInput *input,
                         uint32_t elapsed_us);
uint32_t btga_runtime_active_enemies(const BtgaRuntime *runtime);
uint32_t btga_runtime_active_pickups(const BtgaRuntime *runtime);
const char *btga_runtime_pickup_name(uint8_t kind);
void btga_runtime_restart_mission(BtgaRuntime *runtime);

#endif
