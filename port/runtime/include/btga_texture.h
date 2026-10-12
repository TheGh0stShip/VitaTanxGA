#ifndef VITATANXGA_TEXTURE_H
#define VITATANXGA_TEXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BtgaTextureInfo {
    uint32_t image_offset;
    uint32_t palette_offset;
    uint16_t width;
    uint16_t height;
    uint16_t u_low;
    uint16_t v_low;
    uint8_t format_id;
    uint8_t size_id;
    uint8_t u_shift;
    uint8_t v_shift;
    uint8_t u_address_mode;
    uint8_t v_address_mode;
    bool has_palette;
} BtgaTextureInfo;

bool btga_texture_describe(const uint8_t *texture_chunk, size_t texture_size,
                           const uint8_t *state_chunk, size_t state_size,
                           BtgaTextureInfo *info);
bool btga_texture_decode_rgba(const uint8_t *texture_chunk, size_t texture_size,
                              const BtgaTextureInfo *info, uint32_t *pixels,
                              size_t pixel_capacity);
float btga_texture_u(const BtgaTextureInfo *info, int16_t s);
float btga_texture_v(const BtgaTextureInfo *info, int16_t t);

#endif
