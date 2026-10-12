#ifndef VITATANXGA_WORLD_H
#define VITATANXGA_WORLD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BtgaWorldComponents {
    const uint8_t *groups; size_t groups_size;
    const uint8_t *placements; size_t placements_size;
    const uint8_t *definitions; size_t definitions_size;
    const uint8_t *models; size_t models_size;
    const uint8_t *parts; size_t parts_size;
    const uint8_t *references; size_t references_size;
    uint32_t declared_group_count;
} BtgaWorldComponents;

typedef struct BtgaWorldSummary {
    uint32_t group_count;
    uint32_t placement_count;
    uint32_t model_count;
    uint32_t part_count;
    uint32_t reference_count;
} BtgaWorldSummary;

typedef struct BtgaWorldBounds {
    int16_t min_x;
    int16_t min_y;
    int16_t min_z;
    int16_t max_x;
    int16_t max_y;
    int16_t max_z;
} BtgaWorldBounds;

typedef struct BtgaWorldSpawn {
    int16_t x, y, z;
    uint16_t yaw;
} BtgaWorldSpawn;

typedef struct BtgaWorldPickup {
    int16_t x, y, z;
    uint16_t yaw;
    uint8_t kind;
} BtgaWorldPickup;

bool btga_world_validate(const BtgaWorldComponents *components,
                         BtgaWorldSummary *summary);
uint32_t btga_read_be_u32(const uint8_t bytes[4]);
bool btga_world_bounds(const BtgaWorldComponents *components,
                       BtgaWorldBounds *bounds);
bool btga_common_model_index(const BtgaWorldComponents *components,
                             uint16_t model_id, uint16_t *model_index);
bool btga_world_find_spawn(const BtgaWorldComponents *components,
                           uint8_t row, uint8_t column, BtgaWorldSpawn *spawn);
bool btga_world_pickups(const BtgaWorldComponents *components,
                        BtgaWorldPickup *pickups, size_t capacity,
                        size_t *pickup_count);

#endif
