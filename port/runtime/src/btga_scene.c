#include "btga_scene.h"

typedef struct SceneVisitContext {
    BtgaPlacedTriangleVisitor visitor;
    void *visitor_context;
    BtgaPlacedTriangle placed;
    uint32_t count;
    uint32_t limit;
    bool accepted;
} SceneVisitContext;

static uint16_t read_be_u16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static bool renderable_kind(uint8_t kind) {
    switch (kind) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 10:
        case 11: case 14: case 20: case 21: case 22: case 24: case 29:
        case 31: case 34: case 35: case 40: case 42: case 43: case 44:
            return true;
        default:
            return false;
    }
}

static bool collision_kind(uint8_t kind) {
    switch (kind) {
        case 1: case 2: case 11: case 14: case 22: case 35: case 42: case 43:
            return true;
        default:
            return false;
    }
}

static bool visit_local_triangle(const BtgaVertex vertices[3], void *opaque) {
    SceneVisitContext *context = opaque;
    unsigned int index;
    if (context->count >= context->limit) return true;
    for (index = 0; index < 3; index++) context->placed.vertices[index] = vertices[index];
    context->count++;
    if (context->visitor != NULL && !context->visitor(&context->placed,
                                                       context->visitor_context)) {
        context->accepted = false;
    }
    return true;
}

bool btga_model_visit(const BtgaWorldComponents *world, uint16_t model_index,
                      uint8_t part_ordinal,
                      float x, float y, float z, uint16_t yaw,
                      const uint8_t *geometry_pool, size_t geometry_pool_size,
                      uint32_t triangle_limit, BtgaPlacedTriangleVisitor visitor,
                      void *visitor_context, uint32_t *visited_triangle_count) {
    BtgaWorldSummary summary;
    SceneVisitContext context = {
        .visitor = visitor, .visitor_context = visitor_context,
        .limit = triangle_limit, .accepted = true,
    };
    const uint8_t *model;
    uint32_t part_index;
    if (world == NULL || geometry_pool == NULL || visited_triangle_count == NULL ||
        triangle_limit == 0 || !btga_world_validate(world, &summary) ||
        model_index >= summary.model_count) return false;
    model = world->models + model_index * 16;
    if (part_ordinal >= model[0]) return false;
    context.placed.placement_x = x;
    context.placed.placement_y = y;
    context.placed.placement_z = z;
    context.placed.placement_yaw = yaw;
    part_index = (uint32_t)read_be_u16(model + 2) + part_ordinal;
    {
        const uint8_t *part = world->parts + part_index * 4;
        uint32_t reference_index;
        for (reference_index = read_be_u16(part + 2);
             reference_index < (uint32_t)read_be_u16(part + 2) + part[0] &&
             context.count < context.limit && context.accepted; reference_index++) {
            const uint8_t *reference = world->references + reference_index * 24;
            uint32_t offset = btga_read_be_u32(reference);
            uint32_t size = btga_read_be_u32(reference + 4);
            BtgaGeometrySummary geometry_summary;
            context.placed.state_offset = btga_read_be_u32(reference + 8);
            context.placed.state_size = btga_read_be_u32(reference + 12);
            context.placed.texture_offset = btga_read_be_u32(reference + 16);
            context.placed.texture_size = btga_read_be_u32(reference + 20);
            if (offset > geometry_pool_size || size > geometry_pool_size - offset ||
                !btga_geometry_visit(geometry_pool + offset, size,
                                     visit_local_triangle, &context,
                                     &geometry_summary)) return false;
        }
    }
    *visited_triangle_count = context.count;
    return context.accepted && context.count != 0;
}

bool btga_scene_visit_near(const BtgaWorldComponents *world,
                           const uint8_t *geometry_pool,
                           size_t geometry_pool_size, float center_x,
                           float center_z, float radius,
                           uint32_t triangle_limit,
                           BtgaPlacedTriangleVisitor visitor,
                           void *visitor_context, BtgaSceneSummary *summary) {
    BtgaWorldSummary world_summary;
    SceneVisitContext context = {
        .visitor = visitor,
        .visitor_context = visitor_context,
        .limit = triangle_limit,
        .accepted = true,
    };
    uint32_t placement_index;

    if (world == NULL || geometry_pool == NULL || summary == NULL || triangle_limit == 0 ||
        !btga_world_validate(world, &world_summary)) return false;
    *summary = (BtgaSceneSummary){0};
    for (placement_index = 0; placement_index < world_summary.placement_count &&
                              context.count < context.limit && context.accepted;
         placement_index++) {
        const uint8_t *placement = world->placements + placement_index * 12;
        uint32_t definition_offset = btga_read_be_u32(placement + 8);
        const uint8_t *definition;
        uint16_t model_index;
        const uint8_t *model;
        uint32_t part_index;
        if (definition_offset > world->definitions_size ||
            world->definitions_size - definition_offset < 4) continue;
        definition = world->definitions + definition_offset;
        if (!renderable_kind(definition[0])) continue;
        context.placed.placement_x = (int16_t)read_be_u16(placement);
        context.placed.placement_y = (int16_t)read_be_u16(placement + 2);
        context.placed.placement_z = (int16_t)read_be_u16(placement + 4);
        context.placed.placement_yaw = read_be_u16(placement + 6);
        if (radius > 0.0f) {
            float dx = context.placed.placement_x - center_x;
            float dz = context.placed.placement_z - center_z;
            if (dx * dx + dz * dz > radius * radius) continue;
        }
        model_index = read_be_u16(definition + 2);
        if (model_index >= world_summary.model_count) return false;
        model = world->models + model_index * 16;
        for (part_index = read_be_u16(model + 2);
             part_index < (uint32_t)read_be_u16(model + 2) + model[0] &&
             context.count < context.limit && context.accepted; part_index++) {
            const uint8_t *part = world->parts + part_index * 4;
            uint32_t reference_index;
            for (reference_index = read_be_u16(part + 2);
                 reference_index < (uint32_t)read_be_u16(part + 2) + part[0] &&
                 context.count < context.limit && context.accepted; reference_index++) {
                const uint8_t *reference = world->references + reference_index * 24;
                uint32_t offset = btga_read_be_u32(reference);
                uint32_t size = btga_read_be_u32(reference + 4);
                BtgaGeometrySummary geometry_summary;
                context.placed.state_offset = btga_read_be_u32(reference + 8);
                context.placed.state_size = btga_read_be_u32(reference + 12);
                context.placed.texture_offset = btga_read_be_u32(reference + 16);
                context.placed.texture_size = btga_read_be_u32(reference + 20);
                if (offset > geometry_pool_size || size > geometry_pool_size - offset ||
                    !btga_geometry_visit(geometry_pool + offset, size, visit_local_triangle,
                                         &context, &geometry_summary)) return false;
            }
        }
        summary->renderable_placement_count++;
    }
    summary->visited_triangle_count = context.count;
    return context.accepted && context.count != 0;
}

bool btga_scene_visit(const BtgaWorldComponents *world,
                      const uint8_t *geometry_pool, size_t geometry_pool_size,
                      uint32_t triangle_limit, BtgaPlacedTriangleVisitor visitor,
                      void *visitor_context, BtgaSceneSummary *summary) {
    return btga_scene_visit_near(world, geometry_pool, geometry_pool_size,
                                 0.0f, 0.0f, 0.0f, triangle_limit, visitor,
                                 visitor_context, summary);
}

typedef struct CollisionBoundsContext {
    int16_t min_x, min_y, min_z;
    int16_t max_x, max_y, max_z;
    bool initialized;
} CollisionBoundsContext;

static bool collect_collision_bounds(const BtgaPlacedTriangle *triangle,
                                     void *opaque) {
    CollisionBoundsContext *bounds = opaque;
    unsigned int index;
    for (index = 0; index < 3; index++) {
        const BtgaVertex *vertex = &triangle->vertices[index];
        if (!bounds->initialized) {
            bounds->min_x = bounds->max_x = vertex->x;
            bounds->min_y = bounds->max_y = vertex->y;
            bounds->min_z = bounds->max_z = vertex->z;
            bounds->initialized = true;
        } else {
            if (vertex->x < bounds->min_x) bounds->min_x = vertex->x;
            if (vertex->y < bounds->min_y) bounds->min_y = vertex->y;
            if (vertex->z < bounds->min_z) bounds->min_z = vertex->z;
            if (vertex->x > bounds->max_x) bounds->max_x = vertex->x;
            if (vertex->y > bounds->max_y) bounds->max_y = vertex->y;
            if (vertex->z > bounds->max_z) bounds->max_z = vertex->z;
        }
    }
    return true;
}

bool btga_scene_collisions(const BtgaWorldComponents *world,
                           const uint8_t *geometry_pool,
                           size_t geometry_pool_size,
                           BtgaWorldCollision *collisions, size_t capacity,
                           size_t *collision_count) {
    BtgaWorldSummary summary;
    enum { kMaximumCollisionModels = 512 };
    CollisionBoundsContext model_bounds[kMaximumCollisionModels] = {{0}};
    bool model_computed[kMaximumCollisionModels] = {false};
    uint32_t placement_index;
    size_t count = 0;
    if (collision_count == NULL || (capacity != 0 && collisions == NULL) ||
        geometry_pool == NULL || !btga_world_validate(world, &summary))
        return false;
    for (placement_index = 0; placement_index < summary.placement_count;
         placement_index++) {
        const uint8_t *placement = world->placements + placement_index * 12;
        uint32_t definition_offset = btga_read_be_u32(placement + 8);
        const uint8_t *definition;
        uint16_t model_index;
        uint32_t triangle_count;
        uint8_t part;
        CollisionBoundsContext bounds;
        if (definition_offset > world->definitions_size ||
            world->definitions_size - definition_offset < 4) continue;
        definition = world->definitions + definition_offset;
        if (!collision_kind(definition[0])) continue;
        model_index = read_be_u16(definition + 2);
        if (summary.model_count > kMaximumCollisionModels ||
            model_index >= summary.model_count || count >= capacity) return false;
        if (!model_computed[model_index]) {
            for (part = 0; part < world->models[(size_t)model_index * 16]; part++) {
                if (!btga_model_visit(world, model_index, part, 0.0f, 0.0f,
                                      0.0f, 0, geometry_pool,
                                      geometry_pool_size, UINT32_MAX,
                                      collect_collision_bounds,
                                      &model_bounds[model_index],
                                      &triangle_count)) return false;
            }
            model_computed[model_index] = true;
        }
        bounds = model_bounds[model_index];
        if (!bounds.initialized) return false;
        collisions[count++] = (BtgaWorldCollision){
            (int16_t)read_be_u16(placement),
            (int16_t)read_be_u16(placement + 2),
            (int16_t)read_be_u16(placement + 4),
            read_be_u16(placement + 6),
            bounds.min_x, bounds.min_y, bounds.min_z,
            bounds.max_x, bounds.max_y, bounds.max_z,
        };
    }
    *collision_count = count;
    return true;
}
