#include "btga_texture.h"

static uint16_t read_be_u16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t read_be_u32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static bool allowed(uint8_t opcode, bool state) {
    if (state) {
        switch (opcode) {
            case 0xD7: case 0xD9: case 0xDF: case 0xE2: case 0xE3: case 0xE7:
            case 0xF2: case 0xF5: case 0xFA: case 0xFB: case 0xFC: return true;
            default: return false;
        }
    }
    switch (opcode) {
        case 0xDF: case 0xE3: case 0xE6: case 0xE7: case 0xE8:
        case 0xF0: case 0xF3: case 0xF5: case 0xFD: return true;
        default: return false;
    }
}

static bool command_range(const uint8_t *chunk, size_t size, bool state,
                          size_t *best_start, size_t *best_count) {
    size_t start;
    *best_count = 0;
    for (start = 0; start + 8 <= size; start += 8) {
        size_t offset;
        size_t count = 0;
        for (offset = start; offset + 8 <= size; offset += 8) {
            uint32_t w0 = read_be_u32(chunk + offset);
            uint32_t w1 = read_be_u32(chunk + offset + 4);
            uint8_t opcode = (uint8_t)(w0 >> 24);
            if (!allowed(opcode, state)) break;
            count++;
            if (opcode == 0xDF) {
                if (w1 == 0 && count > 1 && count > *best_count) {
                    *best_start = start;
                    *best_count = count;
                }
                break;
            }
        }
    }
    return *best_count != 0;
}

static uint8_t five_to_eight(uint16_t value) {
    return (uint8_t)((value << 3) | (value >> 2));
}

static uint32_t rgba16(uint16_t value) {
    uint8_t red = five_to_eight((value >> 11) & 31);
    uint8_t green = five_to_eight((value >> 6) & 31);
    uint8_t blue = five_to_eight((value >> 1) & 31);
    uint8_t alpha = (value & 1) != 0 ? 255 : 0;
    return (uint32_t)red | ((uint32_t)green << 8) | ((uint32_t)blue << 16) |
           ((uint32_t)alpha << 24);
}

bool btga_texture_describe(const uint8_t *texture_chunk, size_t texture_size,
                           const uint8_t *state_chunk, size_t state_size,
                           BtgaTextureInfo *info) {
    size_t texture_start = 0;
    size_t texture_count = 0;
    size_t state_start = 0;
    size_t state_count = 0;
    size_t index;
    uint32_t tile_w0 = 0;
    uint32_t tile_w1 = 0;
    uint32_t size_w1 = 0;
    uint32_t images[16];
    unsigned int image_count = 0;
    bool have_tile = false;
    bool have_size = false;
    if (texture_chunk == NULL || state_chunk == NULL || info == NULL ||
        !command_range(texture_chunk, texture_size, false, &texture_start, &texture_count) ||
        !command_range(state_chunk, state_size, true, &state_start, &state_count)) return false;
    for (index = 0; index < texture_count; index++) {
        const uint8_t *command = texture_chunk + texture_start + index * 8;
        if (command[0] == 0xFD && image_count < 16)
            images[image_count++] = read_be_u32(command + 4);
    }
    for (index = 0; index < state_count; index++) {
        const uint8_t *command = state_chunk + state_start + index * 8;
        if (command[0] == 0xF5) {
            tile_w0 = read_be_u32(command);
            tile_w1 = read_be_u32(command + 4);
            have_tile = true;
        } else if (command[0] == 0xF2) {
            size_w1 = read_be_u32(command + 4);
            have_size = true;
        }
    }
    if (image_count == 0 || !have_tile || !have_size) return false;
    *info = (BtgaTextureInfo){
        .image_offset = images[image_count - 1],
        .palette_offset = image_count > 1 ? images[0] : 0,
        .width = (uint16_t)(((size_w1 >> 12) & 0xFFF) / 4 + 1),
        .height = (uint16_t)((size_w1 & 0xFFF) / 4 + 1),
        .format_id = (uint8_t)((tile_w0 >> 21) & 7),
        .size_id = (uint8_t)((tile_w0 >> 19) & 3),
        .u_address_mode = (uint8_t)((tile_w1 >> 8) & 3),
        .v_address_mode = (uint8_t)((tile_w1 >> 18) & 3),
        .has_palette = image_count > 1,
    };
    return info->width != 0 && info->height != 0 && info->image_offset < texture_size;
}

bool btga_texture_decode_rgba(const uint8_t *chunk, size_t size,
                              const BtgaTextureInfo *info, uint32_t *pixels,
                              size_t capacity) {
    size_t count;
    size_t index;
    if (chunk == NULL || info == NULL || pixels == NULL) return false;
    count = (size_t)info->width * info->height;
    if (count == 0 || capacity < count) return false;
    if (info->format_id == 0 && info->size_id == 2) {
        if (info->image_offset > size || count > (size - info->image_offset) / 2) return false;
        for (index = 0; index < count; index++)
            pixels[index] = rgba16(read_be_u16(chunk + info->image_offset + index * 2));
    } else if (info->format_id == 0 && info->size_id == 3) {
        if (info->image_offset > size || count > (size - info->image_offset) / 4) return false;
        for (index = 0; index < count; index++) {
            const uint8_t *source = chunk + info->image_offset + index * 4;
            pixels[index] = (uint32_t)source[0] | ((uint32_t)source[1] << 8) |
                            ((uint32_t)source[2] << 16) | ((uint32_t)source[3] << 24);
        }
    } else if (info->format_id == 2 && info->size_id == 0 && info->has_palette) {
        if (info->palette_offset > size || 32 > size - info->palette_offset ||
            info->image_offset > size || (count + 1) / 2 > size - info->image_offset) return false;
        for (index = 0; index < count; index++) {
            uint8_t packed = chunk[info->image_offset + index / 2];
            uint8_t palette_index = (index & 1) == 0 ? packed >> 4 : packed & 15;
            pixels[index] = rgba16(read_be_u16(chunk + info->palette_offset +
                                               palette_index * 2));
        }
    } else if (info->format_id == 4 && info->size_id == 0) {
        if (info->image_offset > size || (count + 1) / 2 > size - info->image_offset) return false;
        for (index = 0; index < count; index++) {
            uint8_t packed = chunk[info->image_offset + index / 2];
            uint8_t intensity = ((index & 1) == 0 ? packed >> 4 : packed & 15) * 17;
            pixels[index] = (uint32_t)intensity | ((uint32_t)intensity << 8) |
                            ((uint32_t)intensity << 16) | 0xFF000000u;
        }
    } else if (info->format_id == 4 && info->size_id == 1) {
        if (info->image_offset > size || count > size - info->image_offset) return false;
        for (index = 0; index < count; index++) {
            uint8_t intensity = chunk[info->image_offset + index];
            pixels[index] = (uint32_t)intensity | ((uint32_t)intensity << 8) |
                            ((uint32_t)intensity << 16) | 0xFF000000u;
        }
    } else {
        return false;
    }
    return true;
}
