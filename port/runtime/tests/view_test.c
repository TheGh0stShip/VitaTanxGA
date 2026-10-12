#include "btga_view.h"

#include <assert.h>

int btga_view_tests(void) {
    BtgaCamera camera = {0};
    BtgaProjectedPoint projected;
    float center[3] = {0.0f, 0.0f, 100.0f};
    float right[3] = {10.0f, 0.0f, 100.0f};
    BtgaPlacedTriangle triangle = {.placement_x = 10, .placement_y = 20,
                                   .placement_z = 30};
    float placed[3];

    triangle.vertices[0].x = 5;
    triangle.vertices[0].y = 6;
    triangle.vertices[0].z = 7;
    btga_place_vertex(&triangle, 0, placed);
    assert(placed[0] == 15.0f && placed[1] == 26.0f && placed[2] == 37.0f);
    assert(btga_project_point(&camera, center, 1.0f, 90.0f, 16.0f, &projected));
    assert(projected.x == 0.0f && projected.y == 0.0f && projected.depth == 100.0f);
    assert(btga_project_point(&camera, right, 1.0f, 90.0f, 16.0f, &projected));
    assert(projected.x > 0.09f && projected.x < 0.11f);
    {
        BtgaProjector projector;
        assert(btga_projector_init(&camera, 1.0f, 90.0f, 16.0f, &projector));
        assert(btga_view_depth(&projector, right) == 100.0f);
    }
    center[2] = 15.0f;
    assert(!btga_project_point(&camera, center, 1.0f, 90.0f, 16.0f, &projected));
    return 0;
}
