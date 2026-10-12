#ifndef VITATANXGA_SCRIPT_H
#define VITATANXGA_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BtgaScriptStreamSummary {
    uint16_t command_count;
    uint16_t wait_command_count;
    uint16_t text_command_count;
    uint8_t stream_type;
} BtgaScriptStreamSummary;

typedef enum BtgaScriptStepResult {
    BTGA_SCRIPT_RUNNING = 0,
    BTGA_SCRIPT_TEXT = 1,
    BTGA_SCRIPT_FINISHED = 2,
    BTGA_SCRIPT_INVALID = 3,
} BtgaScriptStepResult;

typedef struct BtgaScriptCursor {
    const uint8_t *data;
    size_t size;
    size_t offset;
    const char *text;
    size_t text_length;
    uint16_t wait_ticks;
    uint16_t text_x;
    uint16_t text_y;
    uint8_t stream_type;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
    bool finished;
} BtgaScriptCursor;

bool btga_script_header(const uint8_t *data, size_t size, uint8_t *scene_type,
                        uint8_t *scene_setting, uint16_t *stream_count);
bool btga_script_stream_validate(const uint8_t *data, size_t size,
                                 BtgaScriptStreamSummary *summary);
bool btga_script_cursor_init(BtgaScriptCursor *cursor, const uint8_t *data,
                             size_t size);
BtgaScriptStepResult btga_script_cursor_tick(BtgaScriptCursor *cursor,
                                              bool skip_wait);

#endif
