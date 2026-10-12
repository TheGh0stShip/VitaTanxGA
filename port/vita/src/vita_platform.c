#include "vita_platform.h"

#include <psp2/ctrl.h>
#include <vita2d.h>

enum {
    kColorBlack = RGBA8(8, 11, 16, 255),
    kColorOrange = RGBA8(239, 122, 42, 255),
    kColorWhite = RGBA8(239, 242, 245, 255),
    kColorGray = RGBA8(170, 180, 190, 255),
};

static vita2d_pgf *s_font;
static vita2d_texture *s_preview;

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
    return true;
}

bool vita_platform_should_quit(void) {
    SceCtrlData pad = {0};
    sceCtrlPeekBufferPositive(0, &pad, 1);
    return (pad.buttons & SCE_CTRL_START) != 0;
}

void vita_platform_begin_frame(void) {
    vita2d_start_drawing();
    vita2d_clear_screen();
}

void vita_platform_draw_bootstrap(void) {
    vita2d_draw_rectangle(0, 0, 960, 544, kColorBlack);
    vita2d_draw_rectangle(54, 112, 8, 270, kColorOrange);
    if (s_preview != NULL) {
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
    vita2d_pgf_draw_text(s_font, 90, 430, kColorWhite, 0.70f,
                         "Press START to exit");
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
