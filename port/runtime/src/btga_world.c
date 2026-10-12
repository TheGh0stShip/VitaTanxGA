#include "btga_world.h"

enum {
    BTGA_GROUP_SIZE = 16, BTGA_PLACEMENT_SIZE = 12, BTGA_MODEL_SIZE = 16,
    BTGA_PART_SIZE = 4, BTGA_REFERENCE_SIZE = 24,
    BTGA_GEOMETRY_POOL_SIZE = 0x0F5AF8,
    BTGA_STATE_POOL_SIZE = 0x009380,
    BTGA_TEXTURE_POOL_SIZE = 0x1F5400,
};

static uint16_t read_be_u16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

uint32_t btga_read_be_u32(const uint8_t bytes[4]) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static int32_t read_be_s32(const uint8_t *bytes) {
    return (int32_t)btga_read_be_u32(bytes);
}

static bool section_valid(const uint8_t *data, size_t size, size_t record_size) {
    return data != NULL && size % record_size == 0 && size / record_size <= UINT32_MAX;
}

static bool range_valid(uint32_t first, uint32_t count, uint32_t limit) {
    return first <= limit && count <= limit - first;
}

static bool pool_range_valid(int32_t offset, int32_t size, uint32_t limit) {
    uint32_t unsigned_offset;
    uint32_t unsigned_size;
    if (offset < 0 || size < 0) return false;
    unsigned_offset = (uint32_t)offset;
    unsigned_size = (uint32_t)size;
    return unsigned_offset <= limit && unsigned_size <= limit - unsigned_offset;
}

bool btga_world_validate(const BtgaWorldComponents *c, BtgaWorldSummary *summary) {
    uint32_t groups, placements, models, parts, references, index;
    if (c == NULL || summary == NULL ||
        !section_valid(c->groups, c->groups_size, BTGA_GROUP_SIZE) ||
        !section_valid(c->placements, c->placements_size, BTGA_PLACEMENT_SIZE) ||
        c->definitions == NULL || c->definitions_size == 0 ||
        !section_valid(c->models, c->models_size, BTGA_MODEL_SIZE) ||
        !section_valid(c->parts, c->parts_size, BTGA_PART_SIZE) ||
        !section_valid(c->references, c->references_size, BTGA_REFERENCE_SIZE)) return false;
    groups = (uint32_t)(c->groups_size / BTGA_GROUP_SIZE);
    placements = (uint32_t)(c->placements_size / BTGA_PLACEMENT_SIZE);
    models = (uint32_t)(c->models_size / BTGA_MODEL_SIZE);
    parts = (uint32_t)(c->parts_size / BTGA_PART_SIZE);
    references = (uint32_t)(c->references_size / BTGA_REFERENCE_SIZE);
    if (c->declared_group_count != groups) return false;
    for (index = 0; index < groups; index++) {
        const uint8_t *row = c->groups + index * BTGA_GROUP_SIZE;
        if (!range_valid(read_be_u16(row + 2), read_be_u16(row), placements)) return false;
    }
    for (index = 0; index < placements; index++) {
        if (btga_read_be_u32(c->placements + index * BTGA_PLACEMENT_SIZE + 8) >=
            c->definitions_size) return false;
    }
    for (index = 0; index < models; index++) {
        const uint8_t *row = c->models + index * BTGA_MODEL_SIZE;
        if (row[1] != 0 || !range_valid(read_be_u16(row + 2), row[0], parts)) return false;
    }
    for (index = 0; index < parts; index++) {
        const uint8_t *row = c->parts + index * BTGA_PART_SIZE;
        if (row[1] != 0 || !range_valid(read_be_u16(row + 2), row[0], references)) return false;
    }
    for (index = 0; index < references; index++) {
        const uint8_t *row = c->references + index * BTGA_REFERENCE_SIZE;
        int32_t texture_offset = read_be_s32(row + 16);
        int32_t texture_size = read_be_s32(row + 20);
        if (!pool_range_valid(read_be_s32(row), read_be_s32(row + 4), BTGA_GEOMETRY_POOL_SIZE) ||
            !pool_range_valid(read_be_s32(row + 8), read_be_s32(row + 12), BTGA_STATE_POOL_SIZE) ||
            !((texture_offset == -1 && texture_size == -1) ||
              pool_range_valid(texture_offset, texture_size, BTGA_TEXTURE_POOL_SIZE))) return false;
    }
    *summary = (BtgaWorldSummary){groups, placements, models, parts, references};
    return true;
}

bool btga_world_bounds(const BtgaWorldComponents *components,
                       BtgaWorldBounds *bounds) {
    BtgaWorldSummary summary;
    uint32_t index;
    if (bounds == NULL || !btga_world_validate(components, &summary) ||
        summary.group_count == 0) return false;
    *bounds = (BtgaWorldBounds){
        (int16_t)read_be_u16(components->groups + 4),
        (int16_t)read_be_u16(components->groups + 6),
        (int16_t)read_be_u16(components->groups + 8),
        (int16_t)read_be_u16(components->groups + 10),
        (int16_t)read_be_u16(components->groups + 12),
        (int16_t)read_be_u16(components->groups + 14),
    };
    for (index = 1; index < summary.group_count; index++) {
        const uint8_t *group = components->groups + index * BTGA_GROUP_SIZE;
        int16_t values[6] = {
            (int16_t)read_be_u16(group + 4), (int16_t)read_be_u16(group + 6),
            (int16_t)read_be_u16(group + 8), (int16_t)read_be_u16(group + 10),
            (int16_t)read_be_u16(group + 12), (int16_t)read_be_u16(group + 14),
        };
        if (values[0] < bounds->min_x) bounds->min_x = values[0];
        if (values[1] < bounds->min_y) bounds->min_y = values[1];
        if (values[2] < bounds->min_z) bounds->min_z = values[2];
        if (values[3] > bounds->max_x) bounds->max_x = values[3];
        if (values[4] > bounds->max_y) bounds->max_y = values[4];
        if (values[5] > bounds->max_z) bounds->max_z = values[5];
    }
    return true;
}

bool btga_common_model_index(const BtgaWorldComponents *components,
                             uint16_t model_id, uint16_t *model_index) {
    BtgaWorldSummary summary;
    uint32_t offset;
    uint16_t index;
    if (model_index == NULL || model_id >= 263 ||
        !btga_world_validate(components, &summary) || summary.placement_count != 1)
        return false;
    offset = btga_read_be_u32(components->placements + 8);
    if (offset > components->definitions_size ||
        components->definitions_size - offset < 2u + 263u * 2u ||
        components->definitions[offset] != 33) return false;
    index = read_be_u16(components->definitions + offset + 2u + model_id * 2u);
    if (index == UINT16_MAX || index >= summary.model_count) return false;
    *model_index = index;
    return true;
}

bool btga_world_find_spawn(const BtgaWorldComponents *c, uint8_t row,
                           uint8_t column, BtgaWorldSpawn *spawn) {
    BtgaWorldSummary summary;
    uint32_t index;
    if (spawn == NULL || !btga_world_validate(c, &summary)) return false;
    for (index = 0; index < summary.placement_count; index++) {
        const uint8_t *placement = c->placements + index * BTGA_PLACEMENT_SIZE;
        uint32_t offset = btga_read_be_u32(placement + 8);
        if (offset <= c->definitions_size && c->definitions_size - offset >= 3 &&
            c->definitions[offset] == 6 && c->definitions[offset + 1] == row &&
            c->definitions[offset + 2] == column) {
            *spawn = (BtgaWorldSpawn){
                (int16_t)read_be_u16(placement),
                (int16_t)read_be_u16(placement + 2),
                (int16_t)read_be_u16(placement + 4),
                read_be_u16(placement + 6),
            };
            return true;
        }
    }
    return false;
}
