#ifndef VITATANXGA_GEOMETRY_H
#define VITATANXGA_GEOMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BtgaVertex {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t s;
    int16_t t;
    int8_t nx;
    int8_t ny;
    int8_t nz;
    uint8_t alpha;
} BtgaVertex;

typedef bool (*BtgaTriangleVisitor)(const BtgaVertex vertices[3], void *context);

typedef struct BtgaGeometrySummary {
    uint32_t vertex_load_count;
    uint32_t triangle_count;
    uint32_t command_offset;
    uint32_t command_count;
} BtgaGeometrySummary;

bool btga_geometry_visit(const uint8_t *chunk, size_t chunk_size,
                         BtgaTriangleVisitor visitor, void *context,
                         BtgaGeometrySummary *summary);

#endif
