#include "btga_world.h"
#include <assert.h>
#include <string.h>

int btga_world_tests(void) {
    uint8_t groups[16] = {0, 1, 0, 0};
    uint8_t placements[12] = {0};
    uint8_t definitions[3] = {0};
    uint8_t models[16] = {1, 0, 0, 0};
    uint8_t parts[4] = {1, 0, 0, 0};
    uint8_t references[24] = {0};
    BtgaWorldSummary summary;
    BtgaWorldBounds bounds;
    BtgaWorldSpawn spawn;
    BtgaWorldPickup pickup;
    size_t pickup_count;
    uint16_t model_index;
    BtgaWorldComponents c = {
        groups, sizeof(groups), placements, sizeof(placements),
        definitions, sizeof(definitions), models, sizeof(models),
        parts, sizeof(parts), references, sizeof(references), 1,
    };
    memset(references + 16, 0xFF, 8);
    assert(btga_world_validate(&c, &summary));
    assert(summary.placement_count == 1 && summary.reference_count == 1);
    assert(btga_world_bounds(&c, &bounds));
    assert(bounds.min_x == 0 && bounds.max_x == 0);
    assert(!btga_common_model_index(&c, 0, &model_index));
    definitions[0] = 6;
    c.definitions_size = 3;
    definitions[1] = 2;
    definitions[2] = 4;
    placements[6] = 0x40;
    assert(btga_world_find_spawn(&c, 2, 4, &spawn));
    assert(spawn.yaw == 0x4000);
    definitions[0] = 7;
    definitions[1] = 11;
    assert(btga_world_pickups(&c, &pickup, 1, &pickup_count));
    assert(pickup_count == 1 && pickup.kind == 11 && pickup.yaw == 0x4000);
    c.definitions_size = 1;
    models[1] = 1;
    assert(!btga_world_validate(&c, &summary));
    models[1] = 0;
    c.declared_group_count = 2;
    assert(!btga_world_validate(&c, &summary));
    return 0;
}
