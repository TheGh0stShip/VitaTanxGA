#include "btga_world.h"
#include "btga_geometry.h"
#include "btga_scene.h"
#include "btga_terrain.h"
#include "btga_material.h"
#include "btga_texture.h"
#include "btga_tank.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct LoadedFile {
    uint8_t *data;
    size_t size;
} LoadedFile;

static bool load_path(const char *path, LoadedFile *loaded) {
    FILE *file;
    long size;

    file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
        (size = ftell(file)) <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) fclose(file);
        return false;
    }
    loaded->data = malloc((size_t)size);
    loaded->size = (size_t)size;
    if (loaded->data == NULL ||
        fread(loaded->data, 1, loaded->size, file) != loaded->size) {
        free(loaded->data);
        *loaded = (LoadedFile){0};
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool load_component(const char *directory, unsigned int index,
                           LoadedFile *loaded) {
    char path[512];
    int path_length = snprintf(path, sizeof(path), "%s/component%u.bin", directory, index);
    return path_length >= 0 && (size_t)path_length < sizeof(path) && load_path(path, loaded);
}

int main(int argc, char **argv) {
    LoadedFile files[7] = {0};
    LoadedFile common_files[7] = {0};
    LoadedFile spawn_files[7] = {0};
    LoadedFile geometry = {0};
    LoadedFile state = {0};
    LoadedFile texture = {0};
    LoadedFile gameplay = {0};
    uint32_t *texture_pixels = NULL;
    size_t texture_capacity = 0;
    BtgaWorldComponents components;
    BtgaWorldSummary summary;
    unsigned int index;
    uint64_t triangles = 0;
    uint64_t vertex_loads = 0;
    uint32_t decoded_textures = 0;
    uint32_t unsupported_textures = 0;
    BtgaSceneSummary scene_summary;
    BtgaWorldBounds world_bounds;
    BtgaTerrain terrain;
    BtgaWorldComponents common_components;
    BtgaWorldComponents spawn_components;
    BtgaWorldSummary common_summary;
    uint16_t tank_model_index;
    uint32_t tank_triangles;
    int result = 1;

    if (argc != 8) {
        fprintf(stderr, "usage: %s WORLD_DIRECTORY GEOMETRY_POOL STATE_POOL TEXTURE_POOL COMMON_WORLD SPAWN_WORLD GAMEPLAY_DATA\n",
                argv[0]);
        return 2;
    }
    for (index = 0; index < 7; index++) {
        if (!load_component(argv[1], index, &files[index])) {
            fprintf(stderr, "failed to load component%u.bin\n", index);
            goto cleanup;
        }
        if (!load_component(argv[5], index, &common_files[index])) {
            fprintf(stderr, "failed to load common component%u.bin\n", index);
            goto cleanup;
        }
        if (!load_component(argv[6], index, &spawn_files[index])) {
            fprintf(stderr, "failed to load spawn component%u.bin\n", index);
            goto cleanup;
        }
    }
    if (!load_path(argv[2], &geometry)) {
        fprintf(stderr, "failed to load geometry pool\n");
        goto cleanup;
    }
    if (!load_path(argv[3], &state)) {
        fprintf(stderr, "failed to load state pool\n");
        goto cleanup;
    }
    if (!load_path(argv[4], &texture)) {
        fprintf(stderr, "failed to load texture pool\n");
        goto cleanup;
    }
    if (!load_path(argv[7], &gameplay)) {
        fprintf(stderr, "failed to load gameplay definitions\n");
        goto cleanup;
    }
    if (files[0].size != 4) goto cleanup;
    components = (BtgaWorldComponents){
        files[1].data, files[1].size, files[2].data, files[2].size,
        files[3].data, files[3].size, files[4].data, files[4].size,
        files[5].data, files[5].size, files[6].data, files[6].size,
        btga_read_be_u32(files[0].data),
    };
    if (common_files[0].size != 4) goto cleanup;
    common_components = (BtgaWorldComponents){
        common_files[1].data, common_files[1].size,
        common_files[2].data, common_files[2].size,
        common_files[3].data, common_files[3].size,
        common_files[4].data, common_files[4].size,
        common_files[5].data, common_files[5].size,
        common_files[6].data, common_files[6].size,
        btga_read_be_u32(common_files[0].data),
    };
    if (spawn_files[0].size != 4) goto cleanup;
    spawn_components = (BtgaWorldComponents){
        spawn_files[1].data, spawn_files[1].size,
        spawn_files[2].data, spawn_files[2].size,
        spawn_files[3].data, spawn_files[3].size,
        spawn_files[4].data, spawn_files[4].size,
        spawn_files[5].data, spawn_files[5].size,
        spawn_files[6].data, spawn_files[6].size,
        btga_read_be_u32(spawn_files[0].data),
    };
    {
        BtgaWorldSummary spawn_summary;
        BtgaWorldSpawn spawn;
        BtgaWorldPickup pickups[64];
        size_t pickup_count;
        uint8_t team, column;
        if (!btga_world_validate(&spawn_components, &spawn_summary)) goto cleanup;
        for (team = 0; team < 4; team++) {
            for (column = 0; column < 5; column++) {
                if (!btga_world_find_spawn(&spawn_components, team, column, &spawn)) {
                    fprintf(stderr, "missing team %u column %u spawn\n", team, column);
                    goto cleanup;
                }
            }
        }
        if (!btga_world_pickups(&spawn_components, pickups, 64, &pickup_count) ||
            pickup_count != 41) {
            fprintf(stderr, "pickup placement count disagrees with retail data\n");
            goto cleanup;
        }
        for (index = 0; index < pickup_count; index++) {
            uint16_t model_id;
            uint16_t model_index;
            uint32_t pickup_triangles;
            if (pickups[index].kind == 20) {
                uint32_t roll = (uint16_t)pickups[index].x * 1103515245u +
                                (uint16_t)pickups[index].z * 12345u;
                if (!btga_pickup_random_kind(gameplay.data, gameplay.size,
                                             roll, &pickups[index].kind)) {
                    fprintf(stderr, "random pickup failed validation\n");
                    goto cleanup;
                }
            }
            if (!btga_pickup_model_id(gameplay.data, gameplay.size,
                                      pickups[index].kind, &model_id) ||
                !btga_common_model_index(&common_components, model_id,
                                         &model_index) ||
                !btga_model_visit(&common_components, model_index, 0,
                                  pickups[index].x, pickups[index].y,
                                  pickups[index].z, pickups[index].yaw,
                                  geometry.data, geometry.size, 1000, NULL, NULL,
                                  &pickup_triangles) || pickup_triangles == 0) {
                fprintf(stderr, "pickup %u model failed validation\n", index);
                goto cleanup;
            }
        }
        printf("spawn formation validated: 4 teams, 20 slots, 41 pickups\n");
    }
    if (!btga_world_validate(&common_components, &common_summary) ||
        common_summary.model_count != 244 ||
        !btga_common_model_index(&common_components, 0, &tank_model_index) ||
        tank_model_index != 19 ||
        !btga_model_visit(&common_components, tank_model_index, 0,
                          12.5f, 3.0f, -7.25f, 0, geometry.data, geometry.size,
                          1000, NULL, NULL, &tank_triangles) ||
        tank_triangles != 45) {
        fprintf(stderr, "common tank model validation failed\n");
        goto cleanup;
    }
    printf("common tank model validated: model %u, %u triangles\n",
           tank_model_index, tank_triangles);
    if (!btga_world_validate(&components, &summary) ||
        summary.group_count != 1 || summary.placement_count != 770 ||
        summary.model_count != 59 || summary.part_count != 59 ||
        summary.reference_count != 277) {
        fprintf(stderr, "extracted world validation failed\n");
        goto cleanup;
    }
    printf("world validated: %u groups, %u placements, %u models, %u references\n",
           summary.group_count, summary.placement_count, summary.model_count,
           summary.reference_count);
    for (index = 0; index < summary.reference_count; index++) {
        const uint8_t *reference = files[6].data + index * 24;
        uint32_t offset = btga_read_be_u32(reference);
        uint32_t size = btga_read_be_u32(reference + 4);
        BtgaGeometrySummary geometry_summary;
        uint32_t state_offset = btga_read_be_u32(reference + 8);
        uint32_t state_size = btga_read_be_u32(reference + 12);
        BtgaMaterialColor material;
        uint32_t texture_offset = btga_read_be_u32(reference + 16);
        uint32_t texture_size = btga_read_be_u32(reference + 20);
        if (offset > geometry.size || size > geometry.size - offset ||
            !btga_geometry_visit(geometry.data + offset, size, NULL, NULL,
                                 &geometry_summary)) {
            fprintf(stderr, "geometry reference %u failed validation\n", index);
            goto cleanup;
        }
        if (state_offset > state.size || state_size > state.size - state_offset ||
            !btga_material_color(state.data + state_offset, state_size, &material)) {
            fprintf(stderr, "state reference %u failed validation\n", index);
            goto cleanup;
        }
        triangles += geometry_summary.triangle_count;
        vertex_loads += geometry_summary.vertex_load_count;
        if (texture_offset != UINT32_MAX) {
            BtgaTextureInfo texture_info;
            size_t pixel_count;
            if (texture_offset > texture.size || texture_size > texture.size - texture_offset ||
                !btga_texture_describe(texture.data + texture_offset, texture_size,
                                       state.data + state_offset, state_size,
                                       &texture_info)) {
                unsupported_textures++;
                continue;
            }
            pixel_count = (size_t)texture_info.width * texture_info.height;
            if (pixel_count > texture_capacity) {
                uint32_t *replacement = realloc(texture_pixels,
                                                pixel_count * sizeof(*texture_pixels));
                if (replacement == NULL) goto cleanup;
                texture_pixels = replacement;
                texture_capacity = pixel_count;
            }
            if (btga_texture_decode_rgba(texture.data + texture_offset, texture_size,
                                         &texture_info, texture_pixels,
                                         texture_capacity)) decoded_textures++;
            else unsupported_textures++;
        }
    }
    printf("geometry validated: %llu triangles, %llu vertex loads\n",
           (unsigned long long)triangles, (unsigned long long)vertex_loads);
    printf("textures decoded: %u references, %u unsupported\n", decoded_textures,
           unsupported_textures);
    if (decoded_textures != 275 || unsupported_textures != 0) {
        fprintf(stderr, "texture totals disagree with the verified corpus\n");
        goto cleanup;
    }
    if (triangles != 1358 || vertex_loads != 293) {
        fprintf(stderr, "geometry totals disagree with the verified corpus\n");
        goto cleanup;
    }
    if (!btga_scene_visit(&components, geometry.data, geometry.size, 10000, NULL,
                          NULL, &scene_summary)) {
        fprintf(stderr, "placed scene traversal failed\n");
        goto cleanup;
    }
    printf("scene traversed: %u placements, %u triangles\n",
           scene_summary.renderable_placement_count,
           scene_summary.visited_triangle_count);
    if (scene_summary.renderable_placement_count != 710 ||
        scene_summary.visited_triangle_count != 6303) {
        fprintf(stderr, "placed scene totals disagree with the verified corpus\n");
        goto cleanup;
    }
    if (!btga_world_bounds(&components, &world_bounds) ||
        !btga_terrain_build(&components, geometry.data, geometry.size,
                            &world_bounds, &terrain)) {
        fprintf(stderr, "terrain construction failed\n");
        goto cleanup;
    }
    printf("terrain built: %u/%u cells populated\n", terrain.populated_cell_count,
           BTGA_TERRAIN_WIDTH * BTGA_TERRAIN_HEIGHT);
    if (terrain.populated_cell_count != 7705) {
        fprintf(stderr, "terrain coverage disagrees with the verified corpus\n");
        goto cleanup;
    }
    result = 0;
cleanup:
    for (index = 0; index < 7; index++) {
        free(files[index].data);
        free(common_files[index].data);
        free(spawn_files[index].data);
    }
    free(geometry.data);
    free(state.data);
    free(texture.data);
    free(gameplay.data);
    free(texture_pixels);
    return result;
}
