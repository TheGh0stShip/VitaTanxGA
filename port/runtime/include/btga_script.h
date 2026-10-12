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

bool btga_script_header(const uint8_t *data, size_t size, uint8_t *scene_type,
                        uint8_t *scene_setting, uint16_t *stream_count);
bool btga_script_stream_validate(const uint8_t *data, size_t size,
                                 BtgaScriptStreamSummary *summary);

#endif
