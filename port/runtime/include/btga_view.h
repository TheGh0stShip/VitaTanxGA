#ifndef VITATANXGA_VIEW_H
#define VITATANXGA_VIEW_H

#include <stdbool.h>

#include "btga_scene.h"

typedef struct BtgaCamera {
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
} BtgaCamera;

typedef struct BtgaProjectedPoint {
    float x;
    float y;
    float depth;
} BtgaProjectedPoint;

typedef struct BtgaProjector {
    float camera_x;
    float camera_y;
    float camera_z;
    float yaw_sine;
    float yaw_cosine;
    float pitch_sine;
    float pitch_cosine;
    float horizontal_scale;
    float vertical_scale;
    float near_plane;
} BtgaProjector;

void btga_place_vertex(const BtgaPlacedTriangle *triangle, unsigned int index,
                       float world[3]);
void btga_place_triangle(const BtgaPlacedTriangle *triangle, float world[3][3]);
bool btga_projector_init(const BtgaCamera *camera, float aspect,
                         float vertical_fov_degrees, float near_plane,
                         BtgaProjector *projector);
bool btga_project(const BtgaProjector *projector, const float world[3],
                  BtgaProjectedPoint *projected);
bool btga_project_point(const BtgaCamera *camera, const float world[3],
                        float aspect, float vertical_fov_degrees,
                        float near_plane, BtgaProjectedPoint *projected);

#endif
