#ifndef VITATANXGA_TERRAIN_H
#define VITATANXGA_TERRAIN_H

#include "btga_scene.h"

enum { BTGA_TERRAIN_WIDTH = 96, BTGA_TERRAIN_HEIGHT = 96 };

typedef struct BtgaTerrain {
    float heights[BTGA_TERRAIN_WIDTH * BTGA_TERRAIN_HEIGHT];
    float min_x;
    float min_z;
    float max_x;
    float max_z;
    uint32_t populated_cell_count;
} BtgaTerrain;

bool btga_terrain_build(const BtgaWorldComponents *world,
                        const uint8_t *geometry_pool, size_t geometry_pool_size,
                        const BtgaWorldBounds *bounds, BtgaTerrain *terrain);
bool btga_terrain_height(const BtgaTerrain *terrain, float x, float z,
                         float *height);

#endif
