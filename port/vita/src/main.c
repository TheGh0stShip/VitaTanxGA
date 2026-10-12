#include "vita_platform.h"

int main(void) {
    if (!vita_platform_startup()) {
        return 1;
    }

    while (!vita_platform_should_quit()) {
        vita_platform_begin_frame();
        vita_platform_draw_bootstrap();
        vita_platform_end_frame();
    }

    vita_platform_shutdown();
    return 0;
}
