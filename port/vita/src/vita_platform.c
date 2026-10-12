#include "vita_platform.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

enum {
    kColorBlack = RGBA8(8, 11, 16, 255),
    kColorOrange = RGBA8(239, 122, 42, 255),
    kColorWhite = RGBA8(239, 242, 245, 255),
    kColorGray = RGBA8(170, 180, 190, 255),
};

static vita2d_pgf *s_font;
static vita2d_texture *s_preview;
static uint32_t s_previous_buttons;

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
    return true;
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
