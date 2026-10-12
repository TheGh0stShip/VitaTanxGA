#include "btga_material.h"

static uint32_t read_be_u32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static bool allowed_opcode(uint8_t opcode) {
    switch (opcode) {
        case 0xD7: case 0xD9: case 0xDF: case 0xE2: case 0xE3: case 0xE7:
        case 0xF2: case 0xF5: case 0xFA: case 0xFB: case 0xFC:
            return true;
        default:
            return false;
    }
}

bool btga_material_color(const uint8_t *chunk, size_t chunk_size,
                         BtgaMaterialColor *color) {
    size_t start;
    size_t best_start = 0;
    size_t best_count = 0;
    size_t index;
    BtgaMaterialColor primitive = {180, 180, 180, 255, false};
    BtgaMaterialColor environment = {180, 180, 180, 255, false};
    bool have_primitive = false;
    if (chunk == NULL || color == NULL) return false;
    for (start = 0; start + 8 <= chunk_size; start += 8) {
        size_t offset;
        size_t count = 0;
        for (offset = start; offset + 8 <= chunk_size; offset += 8) {
            uint32_t w0 = read_be_u32(chunk + offset);
            uint32_t w1 = read_be_u32(chunk + offset + 4);
            uint8_t opcode = (uint8_t)(w0 >> 24);
            if (!allowed_opcode(opcode)) break;
            count++;
            if (opcode == 0xDF) {
                if (w1 == 0 && count > 1 && count > best_count) {
                    best_start = start;
                    best_count = count;
                }
                break;
            }
        }
    }
    if (best_count == 0) return false;
    for (index = 0; index < best_count; index++) {
        const uint8_t *command = chunk + best_start + index * 8;
        uint8_t opcode = command[0];
        if (opcode == 0xFA || opcode == 0xFB) {
            BtgaMaterialColor decoded = {
                command[4], command[5], command[6], command[7], opcode == 0xFA,
            };
            if (opcode == 0xFA) {
                primitive = decoded;
                have_primitive = true;
            } else {
                environment = decoded;
            }
        }
    }
    *color = have_primitive ? primitive : environment;
    return true;
}
