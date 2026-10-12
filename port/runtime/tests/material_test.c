#include "btga_material.h"

#include <assert.h>

int btga_material_tests(void) {
    uint8_t chunk[16] = {
        0xFA, 0, 0, 0, 0x12, 0x34, 0x56, 0x78,
        0xDF, 0, 0, 0, 0, 0, 0, 0,
    };
    BtgaMaterialColor color;
    assert(btga_material_color(chunk, sizeof(chunk), &color));
    assert(color.red == 0x12 && color.green == 0x34 && color.blue == 0x56 &&
           color.alpha == 0x78 && color.from_primitive);
    chunk[0] = 0;
    assert(!btga_material_color(chunk, sizeof(chunk), &color));
    return 0;
}
