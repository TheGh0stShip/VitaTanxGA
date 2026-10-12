#include "btga_terrain.h"

#include "btga_view.h"

#include <float.h>
#include <math.h>

static bool rasterize_triangle(const BtgaPlacedTriangle *triangle, void *opaque) {
    BtgaTerrain *terrain = opaque;
    float world[3][3];
    float denominator;
    float cell_width = (terrain->max_x - terrain->min_x) / BTGA_TERRAIN_WIDTH;
    float cell_height = (terrain->max_z - terrain->min_z) / BTGA_TERRAIN_HEIGHT;
    int min_column;
    int max_column;
    int min_row;
    int max_row;
    int row;
    int column;
    btga_place_triangle(triangle, world);
    denominator = (world[1][2] - world[2][2]) * (world[0][0] - world[2][0]) +
                  (world[2][0] - world[1][0]) * (world[0][2] - world[2][2]);
    if (fabsf(denominator) < 0.001f) return true;
    min_column = (int)floorf((fminf(world[0][0], fminf(world[1][0], world[2][0])) -
                              terrain->min_x) / cell_width);
    max_column = (int)floorf((fmaxf(world[0][0], fmaxf(world[1][0], world[2][0])) -
                              terrain->min_x) / cell_width);
    min_row = (int)floorf((fminf(world[0][2], fminf(world[1][2], world[2][2])) -
                           terrain->min_z) / cell_height);
    max_row = (int)floorf((fmaxf(world[0][2], fmaxf(world[1][2], world[2][2])) -
                           terrain->min_z) / cell_height);
    if (min_column < 0) min_column = 0;
    if (min_row < 0) min_row = 0;
    if (max_column >= BTGA_TERRAIN_WIDTH) max_column = BTGA_TERRAIN_WIDTH - 1;
    if (max_row >= BTGA_TERRAIN_HEIGHT) max_row = BTGA_TERRAIN_HEIGHT - 1;
    for (row = min_row; row <= max_row; row++) {
        float z = terrain->min_z + ((float)row + 0.5f) * cell_height;
        for (column = min_column; column <= max_column; column++) {
            float x = terrain->min_x + ((float)column + 0.5f) * cell_width;
            float a = ((world[1][2] - world[2][2]) * (x - world[2][0]) +
                       (world[2][0] - world[1][0]) * (z - world[2][2])) / denominator;
            float b = ((world[2][2] - world[0][2]) * (x - world[2][0]) +
                       (world[0][0] - world[2][0]) * (z - world[2][2])) / denominator;
            float c = 1.0f - a - b;
            float y;
            float *cell;
            if (a < -0.001f || b < -0.001f || c < -0.001f) continue;
            y = a * world[0][1] + b * world[1][1] + c * world[2][1];
            cell = &terrain->heights[row * BTGA_TERRAIN_WIDTH + column];
            if (*cell == FLT_MAX) terrain->populated_cell_count++;
            /* This recovered level's vehicle spawns are defined at y=0.
             * World geometry also contains roofs and underground faces, so
             * retain the surface closest to that gameplay datum. */
            if (*cell == FLT_MAX || fabsf(y) < fabsf(*cell)) *cell = y;
        }
    }
    return true;
}

bool btga_terrain_build(const BtgaWorldComponents *world,
                        const uint8_t *geometry_pool, size_t geometry_pool_size,
                        const BtgaWorldBounds *bounds, BtgaTerrain *terrain) {
    BtgaSceneSummary summary;
    unsigned int index;
    if (world == NULL || geometry_pool == NULL || bounds == NULL || terrain == NULL ||
        bounds->min_x >= bounds->max_x || bounds->min_z >= bounds->max_z) return false;
    *terrain = (BtgaTerrain){
        .min_x = bounds->min_x,
        .min_z = bounds->min_z,
        .max_x = bounds->max_x,
        .max_z = bounds->max_z,
    };
    for (index = 0; index < BTGA_TERRAIN_WIDTH * BTGA_TERRAIN_HEIGHT; index++)
        terrain->heights[index] = FLT_MAX;
    if (!btga_scene_visit(world, geometry_pool, geometry_pool_size, UINT32_MAX,
                          rasterize_triangle, terrain, &summary)) return false;
    return terrain->populated_cell_count != 0;
}

bool btga_terrain_height(const BtgaTerrain *terrain, float x, float z,
                         float *height) {
    float cell_width;
    float cell_height;
    int base_column;
    int base_row;
    int radius;
    if (terrain == NULL || height == NULL || x < terrain->min_x || x > terrain->max_x ||
        z < terrain->min_z || z > terrain->max_z) return false;
    cell_width = (terrain->max_x - terrain->min_x) / BTGA_TERRAIN_WIDTH;
    cell_height = (terrain->max_z - terrain->min_z) / BTGA_TERRAIN_HEIGHT;
    base_column = (int)((x - terrain->min_x) / cell_width);
    base_row = (int)((z - terrain->min_z) / cell_height);
    if (base_column == BTGA_TERRAIN_WIDTH) base_column--;
    if (base_row == BTGA_TERRAIN_HEIGHT) base_row--;
    for (radius = 0; radius <= 3; radius++) {
        int row;
        int column;
        for (row = base_row - radius; row <= base_row + radius; row++) {
            if (row < 0 || row >= BTGA_TERRAIN_HEIGHT) continue;
            for (column = base_column - radius; column <= base_column + radius; column++) {
                float value;
                if (column < 0 || column >= BTGA_TERRAIN_WIDTH) continue;
                value = terrain->heights[row * BTGA_TERRAIN_WIDTH + column];
                if (value != FLT_MAX) {
                    *height = value;
                    return true;
                }
            }
        }
    }
    return false;
}
