#include "btga_texture.h"

#include <assert.h>

int btga_texture_tests(void) {
    uint8_t texture[18] = {
        0xFD, 0, 0, 0, 0, 0, 0, 16,
        0xDF, 0, 0, 0, 0, 0, 0, 0,
        0xF8, 0x01,
    };
    uint8_t state[24] = {
        0xF5, 0x10, 0, 0, 0, 0, 0, 0,
        0xF2, 0, 0, 0, 0, 0, 0, 0,
        0xDF, 0, 0, 0, 0, 0, 0, 0,
    };
    BtgaTextureInfo info;
    uint32_t pixel;
    assert(btga_texture_describe(texture, sizeof(texture), state, sizeof(state), &info));
    assert(info.width == 1 && info.height == 1 && info.format_id == 0 && info.size_id == 2);
    assert(btga_texture_decode_rgba(texture, sizeof(texture), &info, &pixel, 1));
    assert(pixel == 0xFF0000FFu);
    return 0;
}
