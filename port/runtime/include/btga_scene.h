#ifndef VITATANXGA_SCENE_H
#define VITATANXGA_SCENE_H

#include "btga_geometry.h"
#include "btga_world.h"

typedef struct BtgaPlacedTriangle {
    BtgaVertex vertices[3];
    int16_t placement_x;
    int16_t placement_y;
    int16_t placement_z;
    uint16_t placement_yaw;
    uint32_t state_offset;
    uint32_t state_size;
    uint32_t texture_offset;
    uint32_t texture_size;
} BtgaPlacedTriangle;

typedef bool (*BtgaPlacedTriangleVisitor)(const BtgaPlacedTriangle *triangle,
                                          void *context);

typedef struct BtgaSceneSummary {
    uint32_t renderable_placement_count;
    uint32_t visited_triangle_count;
} BtgaSceneSummary;

bool btga_scene_visit(const BtgaWorldComponents *world,
                      const uint8_t *geometry_pool, size_t geometry_pool_size,
                      uint32_t triangle_limit, BtgaPlacedTriangleVisitor visitor,
                      void *context, BtgaSceneSummary *summary);

#endif
