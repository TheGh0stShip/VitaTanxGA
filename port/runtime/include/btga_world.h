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

bool btga_world_validate(const BtgaWorldComponents *components,
                         BtgaWorldSummary *summary);
uint32_t btga_read_be_u32(const uint8_t bytes[4]);

#endif
