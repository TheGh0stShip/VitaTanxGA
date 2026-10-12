#include "btga_world.h"
#include "btga_geometry.h"
#include "btga_scene.h"
#include "btga_terrain.h"
#include "btga_material.h"
#include "btga_texture.h"

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
    LoadedFile geometry = {0};
    LoadedFile state = {0};
    LoadedFile texture = {0};
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
    int result = 1;

    if (argc != 5) {
        fprintf(stderr, "usage: %s WORLD_DIRECTORY GEOMETRY_POOL STATE_POOL TEXTURE_POOL\n",
                argv[0]);
        return 2;
    }
    for (index = 0; index < 7; index++) {
        if (!load_component(argv[1], index, &files[index])) {
            fprintf(stderr, "failed to load component%u.bin\n", index);
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
    if (files[0].size != 4) goto cleanup;
    components = (BtgaWorldComponents){
        files[1].data, files[1].size, files[2].data, files[2].size,
        files[3].data, files[3].size, files[4].data, files[4].size,
        files[5].data, files[5].size, files[6].data, files[6].size,
        btga_read_be_u32(files[0].data),
    };
    if (!btga_world_validate(&components, &summary) ||
        summary.group_count != 2 || summary.placement_count != 1035 ||
        summary.model_count != 135 || summary.part_count != 135 ||
        summary.reference_count != 1287) {
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
    if (decoded_textures != 1285 || unsupported_textures != 0) {
        fprintf(stderr, "texture totals disagree with the verified corpus\n");
        goto cleanup;
    }
    if (triangles != 3958 || vertex_loads != 1345) {
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
    if (scene_summary.renderable_placement_count != 969 ||
        scene_summary.visited_triangle_count != 4144) {
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
    if (terrain.populated_cell_count != 7907) {
        fprintf(stderr, "terrain coverage disagrees with the verified corpus\n");
        goto cleanup;
    }
    result = 0;
cleanup:
    for (index = 0; index < 7; index++) free(files[index].data);
    free(geometry.data);
    free(state.data);
    free(texture.data);
    free(texture_pixels);
    return result;
}
