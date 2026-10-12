#include "vita_platform.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include <stdio.h>
#include <stdlib.h>

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
    LoadedFile files[7] = {0};
    BtgaWorldComponents components;
    bool valid = false;
    unsigned int index;

    for (index = 0; index < 7; index++) {
        if (!load_file(paths[index], &files[index])) goto cleanup;
    }
    if (files[0].size != 4) goto cleanup;
    components = (BtgaWorldComponents){
        files[1].data, files[1].size, files[2].data, files[2].size,
        files[3].data, files[3].size, files[4].data, files[4].size,
        files[5].data, files[5].size, files[6].data, files[6].size,
        btga_read_be_u32(files[0].data),
    };
    valid = btga_world_validate(&components, &s_world_summary);
cleanup:
    for (index = 0; index < 7; index++) free(files[index].data);
    return valid;
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
    vita2d_draw_rectangle(0, 0, 960, 544, kColorBlack);
    vita2d_draw_rectangle(54, 112, 8, 270, kColorOrange);
    if (runtime->screen == BTGA_SCREEN_ASSET_PREVIEW && s_preview != NULL) {
        float width = (float)vita2d_texture_get_width(s_preview);
        float height = (float)vita2d_texture_get_height(s_preview);
        float scale = 250.0f / (width > height ? width : height);
        vita2d_draw_texture_scale(s_preview, 650.0f, 135.0f, scale, scale);
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
    vita2d_pgf_draw_text(s_font, 90, 430, kColorWhite, 0.70f,
                         "CROSS: asset preview   CIRCLE: back   START: exit");
}

void vita_platform_end_frame(void) {
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

void vita_platform_shutdown(void) {
    if (s_preview != NULL) {
        vita2d_free_texture(s_preview);
        s_preview = NULL;
    }
    if (s_font != NULL) {
        vita2d_free_pgf(s_font);
        s_font = NULL;
    }
    vita2d_fini();
}
