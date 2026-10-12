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
        case 0: case 1: case 2: case 11: case 14: case 20: case 22:
        case 29: case 34: case 35: case 40: case 42: case 43: case 44:
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

bool btga_scene_visit(const BtgaWorldComponents *world,
                      const uint8_t *geometry_pool, size_t geometry_pool_size,
                      uint32_t triangle_limit, BtgaPlacedTriangleVisitor visitor,
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
        model_index = read_be_u16(definition + 2);
        if (model_index >= world_summary.model_count) return false;
        model = world->models + model_index * 16;
        context.placed.placement_x = (int16_t)read_be_u16(placement);
        context.placed.placement_y = (int16_t)read_be_u16(placement + 2);
        context.placed.placement_z = (int16_t)read_be_u16(placement + 4);
        context.placed.placement_yaw = read_be_u16(placement + 6);
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
