#ifndef VITATANXGA_TANK_H
#define VITATANXGA_TANK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { BTGA_TANK_MAX_RENDER_PARTS = 8 };

typedef struct BtgaTankRenderPart {
    uint16_t model_id;
    bool turret_transform;
} BtgaTankRenderPart;

typedef struct BtgaTankDefinition {
    BtgaTankRenderPart parts[BTGA_TANK_MAX_RENDER_PARTS];
    uint8_t part_count;
} BtgaTankDefinition;

bool btga_tank_definition(const uint8_t *gameplay, size_t gameplay_size,
                          uint8_t tank_kind, BtgaTankDefinition *definition);
bool btga_pickup_model_id(const uint8_t *gameplay, size_t gameplay_size,
                          uint8_t pickup_kind, uint16_t *model_id);

#endif
