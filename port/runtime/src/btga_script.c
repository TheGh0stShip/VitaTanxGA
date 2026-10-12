#include "btga_script.h"

static uint16_t read_be_u16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static bool command_size(const uint8_t *data, size_t available,
                         uint8_t stream_type, size_t *size) {
    uint8_t opcode;
    if (available == 0 || size == NULL) return false;
    opcode = data[0];
    if (opcode == 0) *size = 1;
    else if (opcode == 1) *size = 3;
    else if (opcode == 2 || opcode == 17 || opcode == 18 || opcode == 29 ||
             opcode == 30 || opcode == 36 || opcode == 39 || opcode == 45 ||
             opcode == 46) *size = 1;
    else if (opcode == 3 || opcode == 4) {
        uint8_t mode;
        if (available < 2) return false;
        mode = data[1];
        *size = 2u + (mode & 3u) + ((mode >> 2) & 3u) + ((mode >> 4) & 3u) +
                ((mode & 0x40u) ? 2u : 0u) + ((mode & 0x80u) ? 2u : 0u);
    } else if (opcode == 5) {
        uint8_t mode;
        if (available < 2) return false;
        mode = data[1];
        *size = 2u + ((mode & 3u) ? 2u : 0u) + ((mode & 0xCu) ? 2u : 0u) +
                ((mode & 0x30u) ? 2u : 0u);
    } else if (opcode == 7) {
        uint8_t mode;
        if (available < 2) return false;
        mode = data[1];
        *size = 2u + ((mode & 1u) ? 1u : 0u) + ((mode & 4u) ? 1u : 0u) +
                ((mode & 0x10u) ? 1u : 0u);
    } else if (opcode == 6 || opcode == 34 || opcode == 35 || opcode == 38 ||
               opcode == 42 || opcode == 43 || opcode == 44) *size = 3;
    else if (opcode == 19 || opcode == 13 || opcode == 14 || opcode == 15 ||
             opcode == 27 || opcode == 48 || opcode == 49) *size = 4;
    else if (opcode == 20 || opcode == 23 || opcode == 24) *size = 7;
    else if (opcode == 8 || opcode == 12 || opcode == 16 || opcode == 26 ||
             opcode == 28 || opcode == 31 || opcode == 32 || opcode == 37 ||
             opcode == 41 || opcode == 47) *size = 2;
    else if (opcode == 9 || opcode == 10 || opcode == 11)
        *size = (stream_type == 30 || stream_type == 31) ? 1u : 2u;
    else if (opcode == 25 || opcode == 22 || opcode == 33 || opcode == 40)
        *size = 5;
    else if (opcode == 21) {
        size_t end = 1;
        while (end < available && data[end] != 0) end++;
        if (end == available) return false;
        *size = end + 1;
    } else *size = 1; /* Retail decoder consumes unknown opcodes. */
    return *size <= available;
}

bool btga_script_header(const uint8_t *data, size_t size, uint8_t *scene_type,
                        uint8_t *scene_setting, uint16_t *stream_count) {
    if (data == NULL || size != 4 || scene_type == NULL || scene_setting == NULL ||
        stream_count == NULL) return false;
    *scene_type = data[0];
    *scene_setting = data[1];
    *stream_count = read_be_u16(data + 2);
    return *stream_count != 0;
}

bool btga_script_stream_validate(const uint8_t *data, size_t size,
                                 BtgaScriptStreamSummary *summary) {
    size_t offset = 0;
    uint8_t stream_type = 0xFF;
    BtgaScriptStreamSummary result = {0};
    if (data == NULL || size == 0 || summary == NULL) return false;
    while (offset < size) {
        size_t length;
        uint8_t opcode = data[offset];
        if (!command_size(data + offset, size - offset, stream_type, &length)) return false;
        if (result.command_count == UINT16_MAX) return false;
        result.command_count++;
        if (opcode == 8 && length == 2 && stream_type == 0xFF)
            stream_type = data[offset + 1];
        if (opcode == 1) result.wait_command_count++;
        if (opcode == 21) result.text_command_count++;
        offset += length;
        if (opcode == 0) {
            if (offset != size) return false;
            result.stream_type = stream_type;
            *summary = result;
            return true;
        }
    }
    return false;
}

bool btga_script_cursor_init(BtgaScriptCursor *cursor, const uint8_t *data,
                             size_t size) {
    BtgaScriptStreamSummary summary;
    if (cursor == NULL || !btga_script_stream_validate(data, size, &summary))
        return false;
    *cursor = (BtgaScriptCursor){
        .data = data,
        .size = size,
        .stream_type = 0xFF,
        .red = 255,
        .green = 255,
        .blue = 255,
        .alpha = 255,
    };
    return true;
}

BtgaScriptStepResult btga_script_cursor_tick(BtgaScriptCursor *cursor,
                                              bool skip_wait) {
    if (cursor == NULL || cursor->data == NULL) return BTGA_SCRIPT_INVALID;
    if (cursor->finished) return BTGA_SCRIPT_FINISHED;
    if (skip_wait) cursor->wait_ticks = 0;
    if (cursor->wait_ticks != 0) {
        cursor->wait_ticks--;
        return BTGA_SCRIPT_RUNNING;
    }
    while (cursor->offset < cursor->size) {
        const uint8_t *command = cursor->data + cursor->offset;
        uint8_t opcode = command[0];
        size_t length;
        if (!command_size(command, cursor->size - cursor->offset,
                          cursor->stream_type, &length))
            return BTGA_SCRIPT_INVALID;
        cursor->offset += length;
        if (opcode == 0) {
            cursor->finished = true;
            return BTGA_SCRIPT_FINISHED;
        }
        if (opcode == 8) cursor->stream_type = command[1];
        else if (opcode == 1) {
            cursor->wait_ticks = read_be_u16(command + 1);
            return BTGA_SCRIPT_RUNNING;
        } else if (opcode == 21) {
            cursor->text = (const char *)(command + 1);
            cursor->text_length = length - 2;
            return BTGA_SCRIPT_TEXT;
        } else if (opcode == 22) {
            cursor->text_x = read_be_u16(command + 1);
            cursor->text_y = read_be_u16(command + 3);
        } else if (opcode == 25) {
            cursor->red = command[1];
            cursor->green = command[2];
            cursor->blue = command[3];
            cursor->alpha = command[4];
        }
    }
    return BTGA_SCRIPT_INVALID;
}
