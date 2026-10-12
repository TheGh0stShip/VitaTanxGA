#ifndef VITATANXGA_MATERIAL_H
#define VITATANXGA_MATERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BtgaMaterialColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
    bool from_primitive;
} BtgaMaterialColor;

bool btga_material_color(const uint8_t *chunk, size_t chunk_size,
                         BtgaMaterialColor *color);

#endif
