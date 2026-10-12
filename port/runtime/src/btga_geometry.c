#include "btga_geometry.h"

#include <limits.h>

enum { BTGA_VERTEX_SIZE = 16, BTGA_VERTEX_CACHE_SIZE = 32 };

static uint16_t read_be_u16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t read_be_u32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static bool find_commands(const uint8_t *chunk, size_t size, size_t *best_start,
                          size_t *best_count) {
    size_t start;

    *best_count = 0;
    for (start = 0; start + 8 <= size; start += 8) {
        size_t offset;
        size_t count = 0;
        for (offset = start; offset + 8 <= size; offset += 8) {
            uint32_t w0 = read_be_u32(chunk + offset);
            uint32_t w1 = read_be_u32(chunk + offset + 4);
            uint8_t opcode = (uint8_t)(w0 >> 24);
            if (opcode != 0x01 && opcode != 0x05 && opcode != 0xDF) break;
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

static BtgaVertex decode_vertex(const uint8_t *bytes) {
    BtgaVertex vertex = {
        (int16_t)read_be_u16(bytes),
        (int16_t)read_be_u16(bytes + 2),
        (int16_t)read_be_u16(bytes + 4),
        (int16_t)read_be_u16(bytes + 8),
        (int16_t)read_be_u16(bytes + 10),
        (int8_t)bytes[12], (int8_t)bytes[13], (int8_t)bytes[14], bytes[15],
    };
    return vertex;
}

bool btga_geometry_visit(const uint8_t *chunk, size_t chunk_size,
                         BtgaTriangleVisitor visitor, void *context,
                         BtgaGeometrySummary *summary) {
    BtgaVertex cache[BTGA_VERTEX_CACHE_SIZE];
    bool loaded[BTGA_VERTEX_CACHE_SIZE] = {false};
    BtgaGeometrySummary result = {0};
    size_t command_start = 0;
    size_t command_count = 0;
    size_t command_index;

    if (chunk == NULL || summary == NULL || chunk_size > UINT32_MAX ||
        !find_commands(chunk, chunk_size, &command_start, &command_count)) return false;
    result.command_offset = (uint32_t)command_start;
    result.command_count = (uint32_t)command_count;
    for (command_index = 0; command_index < command_count; command_index++) {
        const uint8_t *command = chunk + command_start + command_index * 8;
        uint32_t w0 = read_be_u32(command);
        uint32_t w1 = read_be_u32(command + 4);
        uint8_t opcode = (uint8_t)(w0 >> 24);
        if (opcode == 0x01) {
            uint32_t count = (w0 >> 12) & 0xFF;
            uint32_t end_slot = (w0 >> 1) & 0x7F;
            uint32_t first_slot;
            uint32_t index;
            if (count == 0 || count > end_slot || end_slot > BTGA_VERTEX_CACHE_SIZE ||
                w1 > chunk_size || count > (chunk_size - w1) / BTGA_VERTEX_SIZE) return false;
            first_slot = end_slot - count;
            for (index = 0; index < count; index++) {
                cache[first_slot + index] = decode_vertex(chunk + w1 + index * BTGA_VERTEX_SIZE);
                loaded[first_slot + index] = true;
            }
            result.vertex_load_count++;
        } else if (opcode == 0x05) {
            uint8_t encoded[3] = {
                (uint8_t)(w0 >> 16), (uint8_t)(w0 >> 8), (uint8_t)w0,
            };
            BtgaVertex triangle[3];
            unsigned int index;
            for (index = 0; index < 3; index++) {
                uint8_t slot;
                if ((encoded[index] & 1) != 0) return false;
                slot = encoded[index] / 2;
                if (slot >= BTGA_VERTEX_CACHE_SIZE || !loaded[slot]) return false;
                triangle[index] = cache[slot];
            }
            result.triangle_count++;
            if (visitor != NULL && !visitor(triangle, context)) return false;
        } else if (opcode == 0xDF) {
            if (command_index + 1 != command_count || w1 != 0) return false;
        } else {
            return false;
        }
    }
    *summary = result;
    return result.triangle_count != 0;
}
