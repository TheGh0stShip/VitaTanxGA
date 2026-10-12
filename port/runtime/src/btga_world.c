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

bool btga_world_pickups(const BtgaWorldComponents *c, BtgaWorldPickup *pickups,
                        size_t capacity, size_t *pickup_count) {
    BtgaWorldSummary summary;
    uint32_t index;
    size_t count = 0;
    if (pickup_count == NULL || (capacity != 0 && pickups == NULL) ||
        !btga_world_validate(c, &summary)) return false;
    for (index = 0; index < summary.placement_count; index++) {
        const uint8_t *placement = c->placements + index * BTGA_PLACEMENT_SIZE;
        uint32_t offset = btga_read_be_u32(placement + 8);
        if (offset <= c->definitions_size && c->definitions_size - offset >= 2 &&
            c->definitions[offset] == 7) {
            if (count >= capacity) return false;
            pickups[count++] = (BtgaWorldPickup){
                (int16_t)read_be_u16(placement),
                (int16_t)read_be_u16(placement + 2),
                (int16_t)read_be_u16(placement + 4),
                read_be_u16(placement + 6), c->definitions[offset + 1],
            };
        }
    }
    *pickup_count = count;
    return true;
}

bool btga_world_collisions(const BtgaWorldComponents *c,
                           BtgaWorldCollision *collisions, size_t capacity,
                           size_t *collision_count) {
    BtgaWorldSummary summary;
    uint32_t index;
    size_t count = 0;
    if (collision_count == NULL || (capacity != 0 && collisions == NULL) ||
        !btga_world_validate(c, &summary)) return false;
    for (index = 0; index < summary.placement_count; index++) {
        const uint8_t *placement = c->placements + index * BTGA_PLACEMENT_SIZE;
        uint32_t offset = btga_read_be_u32(placement + 8);
        const uint8_t *definition;
        if (offset > c->definitions_size ||
            c->definitions_size - offset < 14) continue;
        definition = c->definitions + offset;
        if (definition[0] != 30 ||
            (definition[1] != 1 && definition[1] != 2)) continue;
        if (count >= capacity) return false;
        collisions[count++] = (BtgaWorldCollision){
            (int16_t)read_be_u16(placement),
            (int16_t)read_be_u16(placement + 2),
            (int16_t)read_be_u16(placement + 4),
            read_be_u16(placement + 6),
            (int16_t)read_be_u16(definition + 2),
            (int16_t)read_be_u16(definition + 4),
            (int16_t)read_be_u16(definition + 6),
            (int16_t)read_be_u16(definition + 8),
            (int16_t)read_be_u16(definition + 10),
            (int16_t)read_be_u16(definition + 12),
        };
    }
    *collision_count = count;
    return true;
}

static uint8_t object_model_count(uint8_t kind) {
    switch (kind) {
        case 3: return 8;
        case 4: return 5;
        case 5: return 3;
        case 21: return 5;
        case 24: return 3;
        case 31: return 1;
        default: return 0;
    }
}

bool btga_world_objects(const BtgaWorldComponents *c,
                        BtgaWorldObject *objects, size_t capacity,
                        size_t *object_count) {
    BtgaWorldSummary summary;
    uint32_t placement_index;
    size_t count = 0;
    if (object_count == NULL || (capacity != 0 && objects == NULL) ||
        !btga_world_validate(c, &summary)) return false;
    for (placement_index = 0; placement_index < summary.placement_count;
         placement_index++) {
        const uint8_t *placement = c->placements + placement_index * BTGA_PLACEMENT_SIZE;
        uint32_t offset = btga_read_be_u32(placement + 8);
        const uint8_t *definition;
        uint8_t candidates, candidate;
        uint16_t model_index = UINT16_MAX;
        const uint8_t *model;
        if (offset > c->definitions_size || c->definitions_size - offset < 4)
            continue;
        definition = c->definitions + offset;
        candidates = object_model_count(definition[0]);
        if (candidates == 0) continue;
        if (definition[0] == 31) {
            model_index = read_be_u16(definition + 2);
        } else {
            for (candidate = 0; candidate < candidates; candidate++) {
                size_t field = 2u + (size_t)candidate * 2u;
                if (field + 2u > c->definitions_size - offset) break;
                model_index = read_be_u16(definition + field);
                if (model_index != UINT16_MAX && model_index < summary.model_count)
                    break;
            }
        }
        if (model_index == UINT16_MAX || model_index >= summary.model_count)
            continue;
        if (count >= capacity) return false;
        model = c->models + (size_t)model_index * BTGA_MODEL_SIZE;
        objects[count++] = (BtgaWorldObject){
            (int16_t)read_be_u16(placement),
            (int16_t)read_be_u16(placement + 2),
            (int16_t)read_be_u16(placement + 4),
            read_be_u16(placement + 6),
            (int16_t)read_be_u16(model + 4),
            (int16_t)read_be_u16(model + 6),
            (int16_t)read_be_u16(model + 8),
            (int16_t)read_be_u16(model + 10),
            (int16_t)read_be_u16(model + 12),
            (int16_t)read_be_u16(model + 14),
            model_index, definition[0],
            definition[0] == 21 && c->definitions_size - offset > 20
                ? definition[20] : 0,
        };
    }
    *object_count = count;
    return true;
}
