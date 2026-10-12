#include "vita_platform.h"
#include "btga_scene.h"
#include "btga_material.h"
#include "btga_texture.h"
#include "btga_tank.h"
#include "btga_script.h"
#include "btga_campaign.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

enum {
    kColorBlack = RGBA8(8, 11, 16, 255),
    kColorOrange = RGBA8(239, 122, 42, 255),
    kColorWhite = RGBA8(239, 242, 245, 255),
    kColorGray = RGBA8(170, 180, 190, 255),
};

static vita2d_pgf *s_font;
static uint32_t s_previous_buttons;
static BtgaWorldSummary s_world_summary;
static bool s_world_ready;
static uint16_t s_mission_stream_count;
static uint32_t s_mission_command_count;

typedef struct LoadedFile {
    uint8_t *data;
    size_t size;
} LoadedFile;

static LoadedFile s_intro_stream;
static LoadedFile s_campaign_text_file;
static LoadedFile s_campaign_config_file;
static BtgaMissionText s_mission_text;
static BtgaMissionConfig s_mission_config;
static LoadedFile s_world_files[7];
static LoadedFile s_common_files[7];
static LoadedFile s_spawn_files[7];
static LoadedFile s_gameplay_data;
static LoadedFile s_geometry_pool;
static LoadedFile s_state_pool;
static LoadedFile s_texture_pool;
static BtgaWorldComponents s_world_components;
static BtgaWorldComponents s_common_components;
static BtgaWorldComponents s_spawn_components;
static BtgaTerrain s_terrain;
static BtgaTankDefinition s_player_tank;
static uint16_t s_player_model_indices[BTGA_TANK_MAX_RENDER_PARTS];
static BtgaTankDefinition s_enemy_tank;
static uint16_t s_enemy_model_indices[BTGA_TANK_MAX_RENDER_PARTS];
enum { kMaximumPickups = 64 };
static BtgaWorldPickup s_pickups[kMaximumPickups];
static uint16_t s_pickup_model_indices[kMaximumPickups];
static size_t s_pickup_count;

enum { kMaximumProjectedTriangles = 8192, kMaximumTextureCache = 256 };

typedef struct CachedTexture {
    uint32_t texture_offset;
    uint32_t texture_size;
    uint32_t state_offset;
    uint32_t state_size;
    BtgaTextureInfo info;
    vita2d_texture *texture;
} CachedTexture;

static SceGxmTextureAddrMode texture_address_mode(uint8_t mode) {
    switch (mode) {
        case 1: return SCE_GXM_TEXTURE_ADDR_MIRROR;
        case 2: return SCE_GXM_TEXTURE_ADDR_CLAMP;
        case 3: return SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP;
        default: return SCE_GXM_TEXTURE_ADDR_REPEAT;
    }
}

typedef struct ProjectedTriangle {
    vita2d_color_vertex vertices[3];
    vita2d_texture_vertex texture_vertices[3];
    const CachedTexture *texture;
    uint32_t color;
    float depth;
    bool actor;
} ProjectedTriangle;

typedef struct DrawSceneContext {
    BtgaProjector projector;
    uint32_t count;
    uint32_t state_offset;
    uint32_t state_size;
    BtgaMaterialColor material;
    bool material_cached;
    const CachedTexture *texture;
    uint32_t texture_offset;
    uint32_t texture_size;
    uint32_t texture_state_offset;
    uint32_t texture_state_size;
    bool actor;
} DrawSceneContext;

static ProjectedTriangle s_projected_triangles[kMaximumProjectedTriangles];
static vita2d_color_vertex s_draw_vertices[kMaximumProjectedTriangles * 3];
static vita2d_texture_vertex s_texture_vertices[kMaximumProjectedTriangles * 3];
static CachedTexture s_texture_cache[kMaximumTextureCache];
static uint32_t s_texture_cache_count;

static const CachedTexture *find_cached_texture(uint32_t texture_offset,
                                                uint32_t texture_size,
                                                uint32_t state_offset,
                                                uint32_t state_size) {
    uint32_t index;
    for (index = 0; index < s_texture_cache_count; index++) {
        CachedTexture *cached = &s_texture_cache[index];
        if (cached->texture_offset == texture_offset &&
            cached->texture_size == texture_size &&
            cached->state_offset == state_offset && cached->state_size == state_size)
            return cached;
    }
    return NULL;
}

static bool cache_texture(uint32_t texture_offset, uint32_t texture_size,
                          uint32_t state_offset, uint32_t state_size) {
    CachedTexture *cached;
    uint32_t *pixels;
    uint8_t *destination;
    unsigned int stride;
    unsigned int row;
    size_t pixel_count;
    if (texture_offset == UINT32_MAX ||
        find_cached_texture(texture_offset, texture_size, state_offset, state_size) != NULL)
        return true;
    if (s_texture_cache_count >= kMaximumTextureCache ||
        texture_offset > s_texture_pool.size ||
        texture_size > s_texture_pool.size - texture_offset ||
        state_offset > s_state_pool.size || state_size > s_state_pool.size - state_offset)
        return false;
    cached = &s_texture_cache[s_texture_cache_count];
    if (!btga_texture_describe(s_texture_pool.data + texture_offset, texture_size,
                               s_state_pool.data + state_offset, state_size,
                               &cached->info)) return false;
    pixel_count = (size_t)cached->info.width * cached->info.height;
    pixels = malloc(pixel_count * sizeof(*pixels));
    if (pixels == NULL ||
        !btga_texture_decode_rgba(s_texture_pool.data + texture_offset, texture_size,
                                  &cached->info, pixels, pixel_count)) {
        free(pixels);
        return false;
    }
    cached->texture = vita2d_create_empty_texture(cached->info.width, cached->info.height);
    if (cached->texture == NULL) {
        free(pixels);
        return false;
    }
    destination = vita2d_texture_get_datap(cached->texture);
    stride = vita2d_texture_get_stride(cached->texture);
    for (row = 0; row < cached->info.height; row++)
        memcpy(destination + row * stride, pixels + row * cached->info.width,
               cached->info.width * sizeof(*pixels));
    vita2d_texture_set_filters(cached->texture, SCE_GXM_TEXTURE_FILTER_LINEAR,
                               SCE_GXM_TEXTURE_FILTER_LINEAR);
    sceGxmTextureSetUAddrMode(&cached->texture->gxm_tex,
                             texture_address_mode(cached->info.u_address_mode));
    sceGxmTextureSetVAddrMode(&cached->texture->gxm_tex,
                             texture_address_mode(cached->info.v_address_mode));
    free(pixels);
    cached->texture_offset = texture_offset;
    cached->texture_size = texture_size;
    cached->state_offset = state_offset;
    cached->state_size = state_size;
    s_texture_cache_count++;
    return true;
}

static bool build_texture_cache(void) {
    uint32_t reference_count = (uint32_t)(s_world_components.references_size / 24);
    uint32_t index;
    for (index = 0; index < reference_count; index++) {
        const uint8_t *reference = s_world_components.references + index * 24;
        if (!cache_texture(btga_read_be_u32(reference + 16),
                           btga_read_be_u32(reference + 20),
                           btga_read_be_u32(reference + 8),
                           btga_read_be_u32(reference + 12))) return false;
    }
    return true;
}

static bool cache_model_textures(const BtgaWorldComponents *world,
                                 uint16_t model_index, uint8_t part_ordinal) {
    const uint8_t *model;
    const uint8_t *part;
    uint32_t part_index;
    uint32_t reference_index;
    uint32_t reference_end;
    if (model_index >= world->models_size / 16) return false;
    model = world->models + model_index * 16;
    if (part_ordinal >= model[0]) return false;
    part_index = ((uint32_t)model[2] << 8) | model[3];
    part_index += part_ordinal;
    if (part_index >= world->parts_size / 4) return false;
    part = world->parts + part_index * 4;
    reference_index = ((uint32_t)part[2] << 8) | part[3];
    reference_end = reference_index + part[0];
    if (reference_end > world->references_size / 24) return false;
    for (; reference_index < reference_end; reference_index++) {
        const uint8_t *reference = world->references + reference_index * 24;
        if (!cache_texture(btga_read_be_u32(reference + 16),
                           btga_read_be_u32(reference + 20),
                           btga_read_be_u32(reference + 8),
                           btga_read_be_u32(reference + 12))) return false;
    }
    return true;
}

static bool cache_entire_model_textures(const BtgaWorldComponents *world,
                                        uint16_t model_index) {
    uint8_t part_count;
    uint8_t part;
    if (model_index >= world->models_size / 16) return false;
    part_count = world->models[(size_t)model_index * 16];
    for (part = 0; part < part_count; part++)
        if (!cache_model_textures(world, model_index, part)) return false;
    return true;
}

static bool load_file(const char *path, LoadedFile *loaded) {
    FILE *file = fopen(path, "rb");
    long size;

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

static bool validate_first_mission(void) {
    LoadedFile header = {0};
    uint8_t scene_type, scene_setting;
    uint16_t stream_count;
    uint32_t command_count = 0;
    unsigned int index;
    if (!load_file("app0:assets/scripts/script_00_514720/header.bin", &header) ||
        !btga_script_header(header.data, header.size, &scene_type, &scene_setting,
                            &stream_count) || stream_count != 41 ||
        scene_type != 1 || scene_setting != 1) {
        free(header.data);
        return false;
    }
    free(header.data);
    for (index = 0; index < stream_count; index++) {
        char path[96];
        LoadedFile stream = {0};
        BtgaScriptStreamSummary summary;
        int length = snprintf(path, sizeof(path),
                              "app0:assets/scripts/script_00_514720/stream_%03u.bin",
                              index);
        if (length < 0 || (size_t)length >= sizeof(path) ||
            !load_file(path, &stream) ||
            !btga_script_stream_validate(stream.data, stream.size, &summary)) {
            free(stream.data);
            free(s_intro_stream.data);
            s_intro_stream = (LoadedFile){0};
            return false;
        }
        command_count += summary.command_count;
        if (index == 27) s_intro_stream = stream;
        else free(stream.data);
    }
    if (command_count != 23483) {
        free(s_intro_stream.data);
        s_intro_stream = (LoadedFile){0};
        return false;
    }
    s_mission_stream_count = stream_count;
    s_mission_command_count = command_count;
    return true;
}

static bool load_campaign_text(void) {
    return load_file("app0:assets/campaign/campaign_text.bin",
                     &s_campaign_text_file) &&
           btga_campaign_mission_text(s_campaign_text_file.data,
                                      s_campaign_text_file.size, 0,
                                      &s_mission_text);
}

static bool load_first_world(void) {
    static const char *const paths[7] = {
        "app0:assets/worlds/level_world_127/component0.bin",
        "app0:assets/worlds/level_world_127/component1.bin",
        "app0:assets/worlds/level_world_127/component2.bin",
        "app0:assets/worlds/level_world_127/component3.bin",
        "app0:assets/worlds/level_world_127/component4.bin",
        "app0:assets/worlds/level_world_127/component5.bin",
        "app0:assets/worlds/level_world_127/component6.bin",
    };
    static const char *const common_paths[7] = {
        "app0:assets/worlds/common_world/component0.bin",
        "app0:assets/worlds/common_world/component1.bin",
        "app0:assets/worlds/common_world/component2.bin",
        "app0:assets/worlds/common_world/component3.bin",
        "app0:assets/worlds/common_world/component4.bin",
        "app0:assets/worlds/common_world/component5.bin",
        "app0:assets/worlds/common_world/component6.bin",
    };
    static const char *const spawn_paths[7] = {
        "app0:assets/worlds/level_world_131/component0.bin",
        "app0:assets/worlds/level_world_131/component1.bin",
        "app0:assets/worlds/level_world_131/component2.bin",
        "app0:assets/worlds/level_world_131/component3.bin",
        "app0:assets/worlds/level_world_131/component4.bin",
        "app0:assets/worlds/level_world_131/component5.bin",
        "app0:assets/worlds/level_world_131/component6.bin",
    };
    bool valid = false;
    unsigned int index;

    for (index = 0; index < 7; index++) {
        if (!load_file(paths[index], &s_world_files[index]) ||
            !load_file(common_paths[index], &s_common_files[index]) ||
            !load_file(spawn_paths[index], &s_spawn_files[index])) goto cleanup;
    }
    if (!load_file("app0:assets/world-pools/geometry.bin", &s_geometry_pool) ||
        !load_file("app0:assets/world-pools/state.bin", &s_state_pool) ||
        !load_file("app0:assets/world-pools/texture.bin", &s_texture_pool) ||
        !load_file("app0:assets/gameplay/gameplay.bin", &s_gameplay_data) ||
        !load_file("app0:assets/campaign/campaign_config.bin",
                   &s_campaign_config_file) ||
        s_world_files[0].size != 4) goto cleanup;
    s_world_components = (BtgaWorldComponents){
        s_world_files[1].data, s_world_files[1].size,
        s_world_files[2].data, s_world_files[2].size,
        s_world_files[3].data, s_world_files[3].size,
        s_world_files[4].data, s_world_files[4].size,
        s_world_files[5].data, s_world_files[5].size,
        s_world_files[6].data, s_world_files[6].size,
        btga_read_be_u32(s_world_files[0].data),
    };
    if (s_common_files[0].size != 4) goto cleanup;
    s_common_components = (BtgaWorldComponents){
        s_common_files[1].data, s_common_files[1].size,
        s_common_files[2].data, s_common_files[2].size,
        s_common_files[3].data, s_common_files[3].size,
        s_common_files[4].data, s_common_files[4].size,
        s_common_files[5].data, s_common_files[5].size,
        s_common_files[6].data, s_common_files[6].size,
        btga_read_be_u32(s_common_files[0].data),
    };
    if (s_spawn_files[0].size != 4) goto cleanup;
    s_spawn_components = (BtgaWorldComponents){
        s_spawn_files[1].data, s_spawn_files[1].size,
        s_spawn_files[2].data, s_spawn_files[2].size,
        s_spawn_files[3].data, s_spawn_files[3].size,
        s_spawn_files[4].data, s_spawn_files[4].size,
        s_spawn_files[5].data, s_spawn_files[5].size,
        s_spawn_files[6].data, s_spawn_files[6].size,
        btga_read_be_u32(s_spawn_files[0].data),
    };
    valid = btga_world_validate(&s_world_components, &s_world_summary);
    if (valid) {
        BtgaWorldBounds bounds;
        valid = btga_world_bounds(&s_world_components, &bounds) &&
                btga_terrain_build(&s_world_components, s_geometry_pool.data,
                                   s_geometry_pool.size, &bounds, &s_terrain) &&
                build_texture_cache() &&
                btga_tank_definition(s_gameplay_data.data, s_gameplay_data.size,
                                     0, &s_player_tank) &&
                btga_campaign_mission_config(s_campaign_config_file.data,
                                             s_campaign_config_file.size, 0,
                                             &s_mission_config) &&
                btga_tank_definition(s_gameplay_data.data, s_gameplay_data.size,
                                     4, &s_enemy_tank);
        for (index = 0; valid && index < s_player_tank.part_count; index++) {
            valid = btga_common_model_index(&s_common_components,
                                            s_player_tank.parts[index].model_id,
                                            &s_player_model_indices[index]) &&
                    cache_model_textures(&s_common_components,
                                         s_player_model_indices[index], 0);
        }
        for (index = 0; valid && index < s_enemy_tank.part_count; index++) {
            valid = btga_common_model_index(&s_common_components,
                                            s_enemy_tank.parts[index].model_id,
                                            &s_enemy_model_indices[index]) &&
                    cache_model_textures(&s_common_components,
                                         s_enemy_model_indices[index], 0);
        }
        valid = valid && btga_world_pickups(&s_spawn_components, s_pickups,
                                            kMaximumPickups, &s_pickup_count);
        for (index = 0; valid && index < s_pickup_count; index++) {
            uint16_t model_id;
            if (s_pickups[index].kind == 20) {
                uint32_t roll = (uint16_t)s_pickups[index].x * 1103515245u +
                                (uint16_t)s_pickups[index].z * 12345u;
                valid = btga_pickup_random_kind(s_gameplay_data.data,
                                                s_gameplay_data.size, roll,
                                                &s_pickups[index].kind);
            }
            valid = valid && btga_pickup_model_id(s_gameplay_data.data,
                                         s_gameplay_data.size,
                                         s_pickups[index].kind, &model_id) &&
                    btga_common_model_index(&s_common_components, model_id,
                                            &s_pickup_model_indices[index]) &&
                    cache_entire_model_textures(&s_common_components,
                                                s_pickup_model_indices[index]);
        }
    }
    if (valid) return true;
cleanup:
    for (index = 0; index < s_texture_cache_count; index++)
        vita2d_free_texture(s_texture_cache[index].texture);
    s_texture_cache_count = 0;
    for (index = 0; index < 7; index++) {
        free(s_world_files[index].data);
        s_world_files[index] = (LoadedFile){0};
        free(s_common_files[index].data);
        s_common_files[index] = (LoadedFile){0};
        free(s_spawn_files[index].data);
        s_spawn_files[index] = (LoadedFile){0};
    }
    free(s_geometry_pool.data);
    s_geometry_pool = (LoadedFile){0};
    free(s_state_pool.data);
    s_state_pool = (LoadedFile){0};
    free(s_texture_pool.data);
    s_texture_pool = (LoadedFile){0};
    free(s_gameplay_data.data);
    s_gameplay_data = (LoadedFile){0};
    free(s_campaign_config_file.data);
    s_campaign_config_file = (LoadedFile){0};
    return valid;
}

static bool draw_world_triangle(const BtgaPlacedTriangle *triangle, void *context) {
    DrawSceneContext *draw = context;
    float world[3][3];
    BtgaProjectedPoint projected[3];
    ProjectedTriangle *output;
    float depth = 0.0f;
    unsigned int shade;
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int index;
    if (draw->count >= kMaximumProjectedTriangles) return true;
    btga_place_triangle(triangle, world);
    for (index = 0; index < 3; index++) {
        if (!btga_project(&draw->projector, world[index], &projected[index])) return true;
        depth += projected[index].depth;
    }
    if ((projected[0].x < -1.2f && projected[1].x < -1.2f && projected[2].x < -1.2f) ||
        (projected[0].x > 1.2f && projected[1].x > 1.2f && projected[2].x > 1.2f) ||
        (projected[0].y < -1.2f && projected[1].y < -1.2f && projected[2].y < -1.2f) ||
        (projected[0].y > 1.2f && projected[1].y > 1.2f && projected[2].y > 1.2f)) return true;
    output = &s_projected_triangles[draw->count++];
    output->depth = depth / 3.0f;
    output->actor = draw->actor;
    if (!draw->material_cached || draw->state_offset != triangle->state_offset ||
        draw->state_size != triangle->state_size) {
        draw->state_offset = triangle->state_offset;
        draw->state_size = triangle->state_size;
        draw->material = (BtgaMaterialColor){110, 170, 90, 255, false};
        if (triangle->state_offset <= s_state_pool.size &&
            triangle->state_size <= s_state_pool.size - triangle->state_offset)
            btga_material_color(s_state_pool.data + triangle->state_offset,
                                triangle->state_size, &draw->material);
        draw->material_cached = true;
    }
    shade = output->depth >= 16000.0f ? 70u : 230u - (unsigned int)(output->depth / 100.0f);
    if (shade < 70u) shade = 70u;
    red = draw->material.red * shade / 255u;
    green = draw->material.green * shade / 255u;
    blue = draw->material.blue * shade / 255u;
    if (draw->texture_offset != triangle->texture_offset ||
        draw->texture_size != triangle->texture_size ||
        draw->texture_state_offset != triangle->state_offset ||
        draw->texture_state_size != triangle->state_size) {
        draw->texture_offset = triangle->texture_offset;
        draw->texture_size = triangle->texture_size;
        draw->texture_state_offset = triangle->state_offset;
        draw->texture_state_size = triangle->state_size;
        draw->texture = find_cached_texture(triangle->texture_offset,
                                            triangle->texture_size,
                                            triangle->state_offset,
                                            triangle->state_size);
    }
    output->texture = draw->texture;
    output->color = RGBA8(red, green, blue, draw->material.alpha);
    for (index = 0; index < 3; index++) {
        float normalized_depth = projected[index].depth / 20000.0f;
        if (normalized_depth < 0.0f) normalized_depth = 0.0f;
        if (normalized_depth > 1.0f) normalized_depth = 1.0f;
        output->vertices[index] = (vita2d_color_vertex){
            (projected[index].x + 1.0f) * 480.0f,
            (1.0f - projected[index].y) * 272.0f,
            normalized_depth,
            RGBA8(red, green, blue, 255),
        };
        output->texture_vertices[index] = (vita2d_texture_vertex){
            output->vertices[index].x, output->vertices[index].y, normalized_depth,
            output->texture != NULL
                ? btga_texture_u(&output->texture->info,
                                 triangle->vertices[index].s)
                : 0.0f,
            output->texture != NULL
                ? btga_texture_v(&output->texture->info,
                                 triangle->vertices[index].t)
                : 0.0f,
        };
    }
    return true;
}

static int compare_triangle_depth(const void *left, const void *right) {
    const ProjectedTriangle *a = left;
    const ProjectedTriangle *b = right;
    /* Keep the vehicle above intersecting coarse terrain triangles until the
     * port replaces this immediate renderer with a GXM depth buffer. */
    if (a->actor != b->actor) return a->actor ? 1 : -1;
    /* vita2d's immediate path has no per-triangle depth buffer here.  Keep
     * painter order authoritative; grouping by texture hid nearby vehicles
     * behind farther terrain whenever their texture keys sorted first. */
    return a->depth < b->depth ? 1 : a->depth > b->depth ? -1 : 0;
}

static uint32_t draw_world(const BtgaRuntime *runtime) {
    DrawSceneContext draw = {0};
    BtgaSceneSummary scene;
    uint32_t tank_triangles;
    uint32_t tank_part;
    uint32_t enemy_index;
    uint32_t pickup_index;
    uint32_t triangle_index;
    if (!btga_projector_init(&runtime->camera, 960.0f / 544.0f, 37.0f, 16.0f,
                             &draw.projector)) return 0;
    sceGxmSetFrontDepthFunc(vita2d_get_context(), SCE_GXM_DEPTH_FUNC_LESS_EQUAL);
    sceGxmSetBackDepthFunc(vita2d_get_context(), SCE_GXM_DEPTH_FUNC_LESS_EQUAL);
    sceGxmSetFrontDepthWriteEnable(vita2d_get_context(), SCE_GXM_DEPTH_WRITE_ENABLED);
    sceGxmSetBackDepthWriteEnable(vita2d_get_context(), SCE_GXM_DEPTH_WRITE_ENABLED);
    draw.actor = true;
    for (tank_part = 0; tank_part < s_player_tank.part_count; tank_part++) {
        float part_turn = runtime->player.yaw;
        uint16_t part_yaw;
        if (s_player_tank.parts[tank_part].turret_transform)
            part_turn += runtime->player.turret_yaw;
        part_turn = fmodf(part_turn, 6.28318530718f);
        if (part_turn < 0.0f) part_turn += 6.28318530718f;
        part_yaw = (uint16_t)(part_turn * (65536.0f / 6.28318530718f));
        if (!btga_model_visit(&s_common_components,
                              s_player_model_indices[tank_part], 0,
                              runtime->player.x, runtime->player.y, runtime->player.z,
                              part_yaw, s_geometry_pool.data, s_geometry_pool.size,
                              1000, draw_world_triangle, &draw, &tank_triangles)) return 0;
    }
    for (enemy_index = 0; enemy_index < BTGA_MAX_ENEMIES; enemy_index++) {
        const BtgaEnemy *enemy = &runtime->enemies[enemy_index];
        const BtgaTankDefinition *tank;
        const uint16_t *model_indices;
        if (!enemy->active) continue;
        tank = enemy->tank_kind == 4 ? &s_enemy_tank : &s_player_tank;
        model_indices = enemy->tank_kind == 4 ? s_enemy_model_indices :
                                                s_player_model_indices;
        for (tank_part = 0; tank_part < tank->part_count; tank_part++) {
            float part_turn = fmodf(enemy->yaw, 6.28318530718f);
            uint16_t part_yaw;
            if (part_turn < 0.0f) part_turn += 6.28318530718f;
            part_yaw = (uint16_t)(part_turn * (65536.0f / 6.28318530718f));
            if (!btga_model_visit(&s_common_components,
                                  model_indices[tank_part], 0,
                                  enemy->x, enemy->y, enemy->z, part_yaw,
                                  s_geometry_pool.data, s_geometry_pool.size,
                                  1000, draw_world_triangle, &draw,
                                  &tank_triangles)) return 0;
        }
    }
    for (pickup_index = 0; pickup_index < s_pickup_count; pickup_index++) {
        uint16_t spin = (uint16_t)(runtime->simulation_ticks * 0x600u);
        const BtgaWorldPickup *pickup = &s_pickups[pickup_index];
        if (pickup_index >= runtime->pickup_count ||
            !runtime->pickups[pickup_index].active) continue;
        if (!btga_model_visit(&s_common_components,
                              s_pickup_model_indices[pickup_index], 0,
                              pickup->x, pickup->y, pickup->z, spin,
                              s_geometry_pool.data, s_geometry_pool.size,
                              1000, draw_world_triangle, &draw,
                              &tank_triangles)) return 0;
    }
    draw.actor = false;
    if (!btga_scene_visit(&s_world_components, s_geometry_pool.data,
                          s_geometry_pool.size, 5000, draw_world_triangle,
                          &draw, &scene)) return 0;
    qsort(s_projected_triangles, draw.count, sizeof(s_projected_triangles[0]),
          compare_triangle_depth);
    triangle_index = 0;
    while (triangle_index < draw.count) {
        const CachedTexture *texture = s_projected_triangles[triangle_index].texture;
        uint32_t color = s_projected_triangles[triangle_index].color;
        uint32_t group_start = triangle_index;
        uint32_t group_count;
        uint32_t output_index = 0;
        while (triangle_index < draw.count &&
               s_projected_triangles[triangle_index].texture == texture &&
               s_projected_triangles[triangle_index].color == color) {
            unsigned int vertex_index;
            for (vertex_index = 0; vertex_index < 3; vertex_index++) {
                s_draw_vertices[output_index] =
                    s_projected_triangles[triangle_index].vertices[vertex_index];
                s_texture_vertices[output_index] =
                    s_projected_triangles[triangle_index].texture_vertices[vertex_index];
                output_index++;
            }
            triangle_index++;
        }
        group_count = triangle_index - group_start;
        if (texture != NULL)
            vita2d_draw_array_textured(texture->texture, SCE_GXM_PRIMITIVE_TRIANGLES,
                                       s_texture_vertices, group_count * 3, color);
        else
            vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES, s_draw_vertices,
                              group_count * 3);
    }
    sceGxmSetFrontDepthFunc(vita2d_get_context(), SCE_GXM_DEPTH_FUNC_ALWAYS);
    sceGxmSetBackDepthFunc(vita2d_get_context(), SCE_GXM_DEPTH_FUNC_ALWAYS);
    sceGxmSetFrontDepthWriteEnable(vita2d_get_context(), SCE_GXM_DEPTH_WRITE_DISABLED);
    sceGxmSetBackDepthWriteEnable(vita2d_get_context(), SCE_GXM_DEPTH_WRITE_DISABLED);
    return draw.count;
}

static int16_t normalize_axis(uint8_t value) {
    int scaled = ((int)value - 128) * 256;
    if (scaled < -32768) {
        scaled = -32768;
    } else if (scaled > 32767) {
        scaled = 32767;
    }
    return (int16_t)scaled;
}

static uint32_t map_buttons(uint32_t buttons) {
    uint32_t mapped = 0;
    mapped |= (buttons & SCE_CTRL_CROSS) ? BTGA_BUTTON_CONFIRM : 0;
    mapped |= (buttons & SCE_CTRL_CIRCLE) ? BTGA_BUTTON_CANCEL : 0;
    mapped |= (buttons & SCE_CTRL_START) ? BTGA_BUTTON_START : 0;
    mapped |= (buttons & SCE_CTRL_SELECT) ? BTGA_BUTTON_SELECT : 0;
    mapped |= (buttons & SCE_CTRL_UP) ? BTGA_BUTTON_UP : 0;
    mapped |= (buttons & SCE_CTRL_DOWN) ? BTGA_BUTTON_DOWN : 0;
    mapped |= (buttons & SCE_CTRL_LEFT) ? BTGA_BUTTON_LEFT : 0;
    mapped |= (buttons & SCE_CTRL_RIGHT) ? BTGA_BUTTON_RIGHT : 0;
    mapped |= (buttons & SCE_CTRL_RTRIGGER) ? BTGA_BUTTON_FIRE : 0;
    mapped |= (buttons & SCE_CTRL_LTRIGGER) ? BTGA_BUTTON_ALT_FIRE : 0;
    return mapped;
}

bool vita_platform_startup(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    if (vita2d_init() < 0) {
        return false;
    }
    s_font = vita2d_load_default_pgf();
    if (s_font == NULL) {
        vita2d_fini();
        return false;
    }
    s_world_ready = load_first_world() && validate_first_mission() &&
                    load_campaign_text();
    return true;
}

bool vita_platform_assets_ready(void) {
    return s_world_ready;
}

const BtgaWorldSummary *vita_platform_world_summary(void) {
    return s_world_ready ? &s_world_summary : NULL;
}

bool vita_platform_world_bounds(BtgaWorldBounds *bounds) {
    return s_world_ready && btga_world_bounds(&s_world_components, bounds);
}

const BtgaTerrain *vita_platform_terrain(void) {
    return s_world_ready ? &s_terrain : NULL;
}

bool vita_platform_spawn(uint8_t row, uint8_t column, BtgaWorldSpawn *spawn) {
    return s_world_ready && btga_world_find_spawn(&s_spawn_components, row, column, spawn);
}

bool vita_platform_mission_config(BtgaMissionConfig *config) {
    if (!s_world_ready || config == NULL) return false;
    *config = s_mission_config;
    return true;
}

bool vita_platform_pickup(size_t index, BtgaWorldPickup *pickup) {
    if (!s_world_ready || pickup == NULL || index >= s_pickup_count) return false;
    *pickup = s_pickups[index];
    return true;
}

bool vita_platform_intro_script(const uint8_t **data, size_t *size) {
    if (data == NULL || size == NULL || s_intro_stream.data == NULL) return false;
    *data = s_intro_stream.data;
    *size = s_intro_stream.size;
    return true;
}

static bool project_to_screen(const BtgaProjector *projector, float x, float y,
                              float z, float *screen_x, float *screen_y) {
    float world[3] = {x, y, z};
    BtgaProjectedPoint point;
    if (!btga_project(projector, world, &point)) return false;
    *screen_x = (point.x + 1.0f) * 480.0f;
    *screen_y = (1.0f - point.y) * 272.0f;
    return true;
}

static void draw_wrapped_text(const char *text, int x, int y, int max_width,
                              uint32_t color, float scale) {
    char line[160];
    size_t used = 0;
    const char *word = text;
    while (word != NULL && *word != '\0' && *word != '\r') {
        const char *end;
        size_t length;
        while (*word == ' ') word++;
        end = word;
        while (*end != '\0' && *end != '\r' && *end != ' ') end++;
        length = (size_t)(end - word);
        if (length == 0) break;
        if (length >= sizeof(line)) length = sizeof(line) - 1;
        if (used != 0 && used + 1 + length < sizeof(line)) {
            line[used] = ' ';
            memcpy(line + used + 1, word, length);
            line[used + 1 + length] = '\0';
            if (vita2d_pgf_text_width(s_font, scale, line) > max_width) {
                line[used] = '\0';
                vita2d_pgf_draw_text(s_font, x, y, color, scale, line);
                y += 34;
                used = 0;
            }
        }
        if (used == 0) {
            memcpy(line, word, length);
            used = length;
            line[used] = '\0';
        } else {
            used += 1 + length;
        }
        word = end;
    }
    if (used != 0) vita2d_pgf_draw_text(s_font, x, y, color, scale, line);
}

static void draw_actors(const BtgaRuntime *runtime) {
    BtgaProjector projector;
    float x;
    float y;
    unsigned int index;
    if (!btga_projector_init(&runtime->camera, 960.0f / 544.0f, 37.0f, 16.0f,
                             &projector)) return;
    for (index = 0; index < BTGA_MAX_PROJECTILES; index++) {
        const BtgaProjectile *projectile = &runtime->projectiles[index];
        if (projectile->active &&
            project_to_screen(&projector, projectile->x, projectile->y, projectile->z,
                              &x, &y)) {
            vita2d_draw_fill_circle(x, y, 3.0f, kColorWhite);
        }
    }
}

uint64_t vita_platform_now_us(void) {
    return sceKernelGetProcessTimeWide();
}

void vita_platform_poll_input(BtgaInput *input) {
    SceCtrlData pad = {0};
    uint32_t buttons;

    sceCtrlPeekBufferPositive(0, &pad, 1);
    buttons = map_buttons(pad.buttons);
    *input = (BtgaInput){
        .held = buttons,
        .pressed = buttons & ~s_previous_buttons,
        .released = s_previous_buttons & ~buttons,
        .move_x = normalize_axis(pad.lx),
        .move_y = normalize_axis(pad.ly),
        .aim_x = normalize_axis(pad.rx),
        .aim_y = normalize_axis(pad.ry),
    };
    s_previous_buttons = buttons;
}

void vita_platform_begin_frame(void) {
    vita2d_start_drawing();
    /* vita2d's clear pass only clears depth when depth writes are enabled.
     * Clear to the shader's far-plane value before the 3D world pass. */
    sceGxmSetFrontDepthFunc(vita2d_get_context(), SCE_GXM_DEPTH_FUNC_ALWAYS);
    sceGxmSetBackDepthFunc(vita2d_get_context(), SCE_GXM_DEPTH_FUNC_ALWAYS);
    sceGxmSetFrontDepthWriteEnable(vita2d_get_context(), SCE_GXM_DEPTH_WRITE_ENABLED);
    sceGxmSetBackDepthWriteEnable(vita2d_get_context(), SCE_GXM_DEPTH_WRITE_ENABLED);
    vita2d_clear_screen();
    sceGxmSetFrontDepthWriteEnable(vita2d_get_context(), SCE_GXM_DEPTH_WRITE_DISABLED);
    sceGxmSetBackDepthWriteEnable(vita2d_get_context(), SCE_GXM_DEPTH_WRITE_DISABLED);
}

void vita_platform_draw(const BtgaRuntime *runtime) {
    const BtgaWorldSummary *world = vita_platform_world_summary();
    vita2d_draw_rectangle(0, 0, 960, 544, kColorBlack);
    vita2d_draw_rectangle(54, 112, 8, 270, kColorOrange);
    if ((runtime->screen == BTGA_SCREEN_GAMEPLAY ||
         runtime->screen == BTGA_SCREEN_PAUSED) && s_world_ready) {
        draw_world(runtime);
        draw_actors(runtime);
    }
    if (runtime->screen == BTGA_SCREEN_TITLE) {
        vita2d_pgf_draw_text(s_font, 88, 166, kColorWhite, 1.55f, "VitaTanx GA");
        vita2d_pgf_draw_text(s_font, 90, 211, kColorOrange, 0.95f,
                             "BattleTanx: Global Assault");
        vita2d_pgf_draw_text(s_font, 90, 300, kColorGray, 0.78f,
                             "CROSS: start   SELECT: exit");
        if (world != NULL) {
            vita2d_pgf_draw_textf(s_font, 90, 350, kColorGray, 0.66f,
                                  "Loaded: %lu placements, %lu models",
                                  (unsigned long)world->placement_count,
                                  (unsigned long)world->model_count);
        }
        if (s_mission_stream_count != 0) {
            vita2d_pgf_draw_textf(s_font, 90, 376, kColorGray, 0.60f,
                                  "Mission data: %u streams, %lu commands",
                                  s_mission_stream_count,
                                  (unsigned long)s_mission_command_count);
        }
    } else if (runtime->screen == BTGA_SCREEN_INTRO) {
        uint32_t color = RGBA8(runtime->intro.red, runtime->intro.green,
                               runtime->intro.blue, 255);
        vita2d_pgf_draw_text(s_font, 70, 72, kColorOrange, 0.72f,
                             "SAN FRANCISCO - FIVE YEARS AFTER THE PLAGUE");
        if (runtime->intro.text != NULL)
            draw_wrapped_text(runtime->intro.text, 70, 238, 820, color, 0.72f);
        vita2d_pgf_draw_text(s_font, 70, 486, kColorGray, 0.58f,
                             "CROSS: continue   CIRCLE: skip intro");
    } else if (runtime->screen == BTGA_SCREEN_BRIEFING) {
        vita2d_pgf_draw_text(s_font, 70, 90, kColorOrange, 1.0f,
                             s_mission_text.location.text);
        draw_wrapped_text(s_mission_text.objective.text, 70, 170, 820,
                          kColorWhite, 0.82f);
        draw_wrapped_text(s_mission_text.briefing.text, 70, 270, 820,
                          kColorGray, 0.68f);
        vita2d_pgf_draw_text(s_font, 70, 486, kColorGray, 0.58f,
                             "CROSS: deploy   CIRCLE: title");
    } else if (runtime->screen == BTGA_SCREEN_GAMEPLAY ||
               runtime->screen == BTGA_SCREEN_PAUSED) {
        vita2d_pgf_draw_textf(s_font, 18, 30, kColorWhite, 0.64f,
                              "ARMOR %u  AMMO %u  ALLY %lu  ENEMY %lu  PICKUP %lu",
                              runtime->player.health, runtime->player.ammo,
                              (unsigned long)btga_runtime_active_allies(runtime),
                              (unsigned long)btga_runtime_active_enemies(runtime),
                              (unsigned long)btga_runtime_active_pickups(runtime));
        vita2d_pgf_draw_text(s_font, 18, 52, kColorGray, 0.55f,
                             "LEFT: drive  RIGHT: turret  R: fire  START: pause");
        vita2d_pgf_draw_textf(s_font, 18, 78, kColorOrange, 0.55f,
                              "OBJECTIVE: %s", s_mission_text.objective.text);
        if (runtime->pickup_message_ticks != 0)
            vita2d_pgf_draw_textf(s_font, 330, 500, kColorOrange, 0.72f,
                                  "PICKED UP %s",
                                  btga_runtime_pickup_name(runtime->last_pickup_kind));
        if (runtime->screen == BTGA_SCREEN_PAUSED) {
            vita2d_draw_rectangle(300, 190, 360, 150, RGBA8(8, 11, 16, 220));
            vita2d_pgf_draw_text(s_font, 410, 245, kColorWhite, 1.1f, "PAUSED");
            vita2d_pgf_draw_text(s_font, 340, 292, kColorGray, 0.65f,
                                 "START: resume   CIRCLE: title");
        }
    } else {
        const char *result = runtime->screen == BTGA_SCREEN_VICTORY
            ? "MISSION COMPLETE" : "TANK DESTROYED";
        vita2d_pgf_draw_text(s_font, 300, 238, kColorWhite, 1.2f, result);
        vita2d_pgf_draw_text(s_font, 350, 292, kColorGray, 0.65f,
                             "CIRCLE: return to title");
    }
}

void vita_platform_end_frame(void) {
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

void vita_platform_shutdown(void) {
    unsigned int index;
    if (s_font != NULL) {
        vita2d_free_pgf(s_font);
        s_font = NULL;
    }
    for (index = 0; index < 7; index++) {
        free(s_world_files[index].data);
        s_world_files[index] = (LoadedFile){0};
        free(s_common_files[index].data);
        s_common_files[index] = (LoadedFile){0};
        free(s_spawn_files[index].data);
        s_spawn_files[index] = (LoadedFile){0};
    }
    free(s_geometry_pool.data);
    s_geometry_pool = (LoadedFile){0};
    free(s_state_pool.data);
    s_state_pool = (LoadedFile){0};
    for (index = 0; index < s_texture_cache_count; index++)
        vita2d_free_texture(s_texture_cache[index].texture);
    s_texture_cache_count = 0;
    free(s_texture_pool.data);
    s_texture_pool = (LoadedFile){0};
    free(s_intro_stream.data);
    s_intro_stream = (LoadedFile){0};
    free(s_campaign_text_file.data);
    s_campaign_text_file = (LoadedFile){0};
    free(s_campaign_config_file.data);
    s_campaign_config_file = (LoadedFile){0};
    free(s_gameplay_data.data);
    s_gameplay_data = (LoadedFile){0};
    vita2d_fini();
}
