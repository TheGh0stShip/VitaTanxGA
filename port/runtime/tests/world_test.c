#include "btga_world.h"
#include <assert.h>
#include <string.h>

int btga_world_tests(void) {
    uint8_t groups[16] = {0, 1, 0, 0};
    uint8_t placements[12] = {0};
    uint8_t definitions[1] = {0};
    uint8_t models[16] = {1, 0, 0, 0};
    uint8_t parts[4] = {1, 0, 0, 0};
    uint8_t references[24] = {0};
    BtgaWorldSummary summary;
    BtgaWorldBounds bounds;
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
    models[1] = 1;
    assert(!btga_world_validate(&c, &summary));
    models[1] = 0;
    c.declared_group_count = 2;
    assert(!btga_world_validate(&c, &summary));
    return 0;
}
