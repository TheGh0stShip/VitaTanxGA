#include "vita_platform.h"
#include "btga_scene.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

enum {
    kColorBlack = RGBA8(8, 11, 16, 255),
    kColorOrange = RGBA8(239, 122, 42, 255),
    kColorWhite = RGBA8(239, 242, 245, 255),
    kColorGray = RGBA8(170, 180, 190, 255),
};

static vita2d_pgf *s_font;
static vita2d_texture *s_preview;
static uint32_t s_previous_buttons;
static BtgaWorldSummary s_world_summary;
static bool s_world_ready;

typedef struct LoadedFile {
    uint8_t *data;
    size_t size;
} LoadedFile;

static LoadedFile s_world_files[7];
static LoadedFile s_geometry_pool;
static BtgaWorldComponents s_world_components;

typedef struct DrawSceneContext {
    int16_t placement_x;
    int16_t placement_z;
    uint16_t placement_yaw;
    float sine;
    float cosine;
    bool initialized;
} DrawSceneContext;

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

static bool load_first_world(void) {
    static const char *const paths[7] = {
        "app0:assets/worlds/level_world_000/component0.bin",
        "app0:assets/worlds/level_world_000/component1.bin",
        "app0:assets/worlds/level_world_000/component2.bin",
        "app0:assets/worlds/level_world_000/component3.bin",
        "app0:assets/worlds/level_world_000/component4.bin",
        "app0:assets/worlds/level_world_000/component5.bin",
        "app0:assets/worlds/level_world_000/component6.bin",
    };
    bool valid = false;
    unsigned int index;

    for (index = 0; index < 7; index++) {
        if (!load_file(paths[index], &s_world_files[index])) goto cleanup;
    }
    if (!load_file("app0:assets/world-pools/geometry.bin", &s_geometry_pool) ||
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
    valid = btga_world_validate(&s_world_components, &s_world_summary);
    if (valid) return true;
cleanup:
    for (index = 0; index < 7; index++) {
        free(s_world_files[index].data);
        s_world_files[index] = (LoadedFile){0};
    }
    free(s_geometry_pool.data);
    s_geometry_pool = (LoadedFile){0};
    return valid;
}

static bool draw_world_triangle(const BtgaPlacedTriangle *triangle, void *context) {
    DrawSceneContext *draw = context;
    float points[3][2];
    unsigned int index;
    if (!draw->initialized || draw->placement_x != triangle->placement_x ||
        draw->placement_z != triangle->placement_z ||
        draw->placement_yaw != triangle->placement_yaw) {
        float angle = (float)triangle->placement_yaw * (6.28318530718f / 65536.0f);
        draw->placement_x = triangle->placement_x;
        draw->placement_z = triangle->placement_z;
        draw->placement_yaw = triangle->placement_yaw;
        draw->sine = sinf(angle);
        draw->cosine = cosf(angle);
        draw->initialized = true;
    }
    for (index = 0; index < 3; index++) {
        float local_x = triangle->vertices[index].x;
        float local_z = triangle->vertices[index].z;
        float world_x = triangle->placement_x + draw->cosine * local_x - draw->sine * local_z;
        float world_z = triangle->placement_z + draw->sine * local_x + draw->cosine * local_z;
        points[index][0] = 420.0f + (world_x + 824.0f) * 0.065f;
        points[index][1] = 500.0f - (world_z + 10703.0f) * 0.0235f;
    }
    vita2d_draw_line(points[0][0], points[0][1], points[1][0], points[1][1], kColorGray);
    vita2d_draw_line(points[1][0], points[1][1], points[2][0], points[2][1], kColorGray);
    vita2d_draw_line(points[2][0], points[2][1], points[0][0], points[0][1], kColorGray);
    return true;
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
    s_preview = vita2d_load_PNG_file("app0:assets/images/image_000.png");
    s_world_ready = load_first_world();
    return true;
}

bool vita_platform_assets_ready(void) {
    return s_preview != NULL && s_world_ready;
}

const BtgaWorldSummary *vita_platform_world_summary(void) {
    return s_world_ready ? &s_world_summary : NULL;
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
    vita2d_clear_screen();
}

void vita_platform_draw(const BtgaRuntime *runtime) {
    const BtgaWorldSummary *world = vita_platform_world_summary();
    BtgaSceneSummary scene = {0};
    DrawSceneContext draw_context = {0};
    vita2d_draw_rectangle(0, 0, 960, 544, kColorBlack);
    vita2d_draw_rectangle(54, 112, 8, 270, kColorOrange);
    if (runtime->screen == BTGA_SCREEN_ASSET_PREVIEW && s_preview != NULL) {
        btga_scene_visit(&s_world_components, s_geometry_pool.data,
                         s_geometry_pool.size, 1500, draw_world_triangle,
                         &draw_context, &scene);
    }
    vita2d_pgf_draw_text(s_font, 88, 166, kColorWhite, 1.55f, "VitaTanx GA");
    vita2d_pgf_draw_text(s_font, 90, 211, kColorOrange, 0.95f,
                         "PS Vita port bootstrap");
    vita2d_pgf_draw_text(s_font, 90, 282, kColorGray, 0.78f,
                         "Renderer and input platform layer online.");
    vita2d_pgf_draw_text(s_font, 90, 316, kColorGray, 0.78f,
                         "Verified extracted game assets are staged in app0:assets.");
    vita2d_pgf_draw_textf(s_font, 90, 350, kColorGray, 0.70f,
                          "30 Hz simulation tick: %llu",
                          (unsigned long long)runtime->simulation_ticks);
    if (world != NULL) {
        vita2d_pgf_draw_textf(s_font, 90, 378, kColorGray, 0.66f,
                              "World: %lu placements, %lu models, %lu refs",
                              (unsigned long)world->placement_count,
                              (unsigned long)world->model_count,
                              (unsigned long)world->reference_count);
    }
    if (runtime->screen == BTGA_SCREEN_ASSET_PREVIEW) {
        vita2d_pgf_draw_textf(s_font, 610, 515, kColorOrange, 0.58f,
                              "%lu placed mesh triangles",
                              (unsigned long)scene.visited_triangle_count);
    }
    vita2d_pgf_draw_text(s_font, 90, 430, kColorWhite, 0.70f,
                         "CROSS: asset preview   CIRCLE: back   START: exit");
}

void vita_platform_end_frame(void) {
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

void vita_platform_shutdown(void) {
    unsigned int index;
    if (s_preview != NULL) {
        vita2d_free_texture(s_preview);
        s_preview = NULL;
    }
    if (s_font != NULL) {
        vita2d_free_pgf(s_font);
        s_font = NULL;
    }
    for (index = 0; index < 7; index++) {
        free(s_world_files[index].data);
        s_world_files[index] = (LoadedFile){0};
    }
    free(s_geometry_pool.data);
    s_geometry_pool = (LoadedFile){0};
    vita2d_fini();
}
