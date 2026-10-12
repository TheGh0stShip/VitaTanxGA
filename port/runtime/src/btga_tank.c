#include "btga_tank.h"

#define BTGA_GAMEPLAY_VRAM UINT32_C(0x80122384)
#define BTGA_TANK_BANK_VRAM UINT32_C(0x80122E28)
enum { BTGA_TANK_RECORD_SIZE = 0xD0u };

static uint32_t read_be_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

bool btga_tank_definition(const uint8_t *data, size_t size, uint8_t kind,
                          BtgaTankDefinition *out) {
    size_t record_offset = (BTGA_TANK_BANK_VRAM - BTGA_GAMEPLAY_VRAM) +
                           (size_t)kind * BTGA_TANK_RECORD_SIZE;
    uint32_t count, pointer;
    size_t parts_offset;
    uint32_t index;
    if (data == NULL || out == NULL || record_offset > size ||
        BTGA_TANK_RECORD_SIZE > size - record_offset) return false;
    count = read_be_u32(data + record_offset);
    pointer = read_be_u32(data + record_offset + 4);
    if (count > BTGA_TANK_MAX_RENDER_PARTS || pointer < BTGA_GAMEPLAY_VRAM) return false;
    parts_offset = pointer - BTGA_GAMEPLAY_VRAM;
    if (parts_offset > size || (size_t)count * 8u > size - parts_offset) return false;
    *out = (BtgaTankDefinition){0};
    for (index = 0; index < count; index++) {
        const uint8_t *part = data + parts_offset + index * 8u;
        uint32_t type = read_be_u32(part);
        uint32_t model = read_be_u32(part + 4);
        if (type == 15u) continue;
        if (model > UINT16_MAX) return false;
        out->parts[out->part_count++] = (BtgaTankRenderPart){
            (uint16_t)model, type == 1u || type == 18u
        };
    }
    return out->part_count != 0;
}
