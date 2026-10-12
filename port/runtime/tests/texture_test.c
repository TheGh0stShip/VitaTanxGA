#include "btga_texture.h"

#include <assert.h>

int btga_texture_tests(void) {
    uint8_t texture[18] = {
        0xFD, 0, 0, 0, 0, 0, 0, 16,
        0xDF, 0, 0, 0, 0, 0, 0, 0,
        0xF8, 0x01,
    };
    uint8_t state[24] = {
        0xF5, 0x10, 0, 0, 0, 0x08, 0, 0,
        0xF2, 0, 0, 0, 0, 0, 0, 0,
        0xDF, 0, 0, 0, 0, 0, 0, 0,
    };
    BtgaTextureInfo info;
    uint32_t pixel;
    assert(btga_texture_describe(texture, sizeof(texture), state, sizeof(state), &info));
    assert(info.width == 1 && info.height == 1 && info.format_id == 0 && info.size_id == 2);
    assert(info.u_address_mode == 0 && info.v_address_mode == 2);
    assert(info.u_low == 0 && info.v_low == 0);
    assert(info.u_shift == 0 && info.v_shift == 0);
    assert(btga_texture_decode_rgba(texture, sizeof(texture), &info, &pixel, 1));
    assert(pixel == 0xFF0000FFu);
    assert(btga_texture_u(&info, 32) == 1.0f);
    assert(btga_texture_v(&info, 32) == 1.0f);
    info.width = 8;
    info.height = 4;
    info.u_low = 8;
    info.v_low = 4;
    info.u_shift = 1;
    info.v_shift = 15;
    assert(btga_texture_u(&info, 192) == 0.125f);
    assert(btga_texture_v(&info, 48) == 0.5f);
    return 0;
}
