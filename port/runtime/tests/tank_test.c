#include "btga_tank.h"

#include <assert.h>
#include <string.h>

static void write_be_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

int btga_tank_tests(void) {
    uint8_t data[0x17E8];
    BtgaTankDefinition definition;
    const uint32_t parts[][2] = {
        {0, 1}, {1, 0}, {2, 2}, {3, 3}, {15, 191}, {17, 219}, {18, 220},
    };
    unsigned int index;
    uint16_t pickup_model;
    memset(data, 0, sizeof(data));
    write_be_u32(data + 0xAA4, 7);
    write_be_u32(data + 0xAA8, 0x801225E0u);
    for (index = 0; index < 7; index++) {
        write_be_u32(data + 0x25C + index * 8, parts[index][0]);
        write_be_u32(data + 0x260 + index * 8, parts[index][1]);
    }
    assert(btga_tank_definition(data, sizeof(data), 0, &definition));
    assert(definition.part_count == 6);
    assert(definition.parts[0].model_id == 1 && !definition.parts[0].turret_transform);
    assert(definition.parts[1].model_id == 0 && definition.parts[1].turret_transform);
    assert(definition.parts[2].model_id == 2 && definition.parts[3].model_id == 3);
    assert(definition.parts[4].model_id == 219 && definition.parts[5].model_id == 220);
    write_be_u32(data + 0x179C, 14);
    assert(btga_pickup_model_id(data, sizeof(data), 1, &pickup_model));
    assert(pickup_model == 14);
    assert(!btga_pickup_model_id(data, sizeof(data), 20, &pickup_model));
    return 0;
}
