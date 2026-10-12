#include "btga_view.h"

#include <math.h>

void btga_place_vertex(const BtgaPlacedTriangle *triangle, unsigned int index,
                       float world[3]) {
    float placed[3][3] = {{0}};
    if (triangle == NULL || world == NULL || index >= 3) return;
    btga_place_triangle(triangle, placed);
    world[0] = placed[index][0];
    world[1] = placed[index][1];
    world[2] = placed[index][2];
}

void btga_place_triangle(const BtgaPlacedTriangle *triangle, float world[3][3]) {
    float angle;
    float sine;
    float cosine;
    unsigned int index;
    if (triangle == NULL || world == NULL) return;
    angle = (float)triangle->placement_yaw * (6.28318530718f / 65536.0f);
    sine = sinf(angle);
    cosine = cosf(angle);
    for (index = 0; index < 3; index++) {
        float local_x = triangle->vertices[index].x;
        float local_z = triangle->vertices[index].z;
        world[index][0] = triangle->placement_x + cosine * local_x - sine * local_z;
        world[index][1] = triangle->placement_y + triangle->vertices[index].y;
        world[index][2] = triangle->placement_z + sine * local_x + cosine * local_z;
    }
}

bool btga_projector_init(const BtgaCamera *camera, float aspect,
                         float vertical_fov_degrees, float near_plane,
                         BtgaProjector *projector) {
    float focal;
    if (camera == NULL || projector == NULL || aspect <= 0.0f ||
        vertical_fov_degrees <= 0.0f || vertical_fov_degrees >= 180.0f ||
        near_plane <= 0.0f) return false;
    focal = 1.0f / tanf(vertical_fov_degrees * (3.14159265359f / 360.0f));
    *projector = (BtgaProjector){
        camera->x, camera->y, camera->z,
        sinf(camera->yaw), cosf(camera->yaw),
        sinf(camera->pitch), cosf(camera->pitch),
        focal / aspect, focal, near_plane,
    };
    return true;
}

bool btga_project(const BtgaProjector *projector, const float world[3],
                  BtgaProjectedPoint *projected) {
    float dx;
    float dy;
    float dz;
    float camera_x;
    float horizontal_depth;
    float camera_y;
    float depth;
    if (projector == NULL || world == NULL || projected == NULL) return false;
    dx = world[0] - projector->camera_x;
    dy = world[1] - projector->camera_y;
    dz = world[2] - projector->camera_z;
    camera_x = projector->yaw_cosine * dx - projector->yaw_sine * dz;
    horizontal_depth = projector->yaw_sine * dx + projector->yaw_cosine * dz;
    camera_y = projector->pitch_cosine * dy - projector->pitch_sine * horizontal_depth;
    depth = projector->pitch_sine * dy + projector->pitch_cosine * horizontal_depth;
    if (depth < projector->near_plane) return false;
    projected->x = camera_x * projector->horizontal_scale / depth;
    projected->y = camera_y * projector->vertical_scale / depth;
    projected->depth = depth;
    return true;
}

bool btga_project_point(const BtgaCamera *camera, const float world[3],
                        float aspect, float vertical_fov_degrees,
                        float near_plane, BtgaProjectedPoint *projected) {
    BtgaProjector projector;
    return btga_projector_init(camera, aspect, vertical_fov_degrees, near_plane,
                               &projector) &&
           btga_project(&projector, world, projected);
}
