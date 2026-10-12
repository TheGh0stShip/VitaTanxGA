#include "btga_script.h"

#include <stdio.h>
#include <stdlib.h>

static int validate_stream(const char *path, uint32_t *commands,
                           uint32_t *intro_texts) {
    FILE *file = fopen(path, "rb");
    long size;
    uint8_t *data;
    BtgaScriptStreamSummary summary;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
        (size = ftell(file)) <= 0 || fseek(file, 0, SEEK_SET) != 0) return 0;
    data = malloc((size_t)size);
    if (data == NULL || fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data); fclose(file); return 0;
    }
    fclose(file);
    if (!btga_script_stream_validate(data, (size_t)size, &summary)) {
        free(data); return 0;
    }
    if (summary.stream_type == 51) {
        BtgaScriptCursor cursor;
        BtgaScriptStepResult result = BTGA_SCRIPT_RUNNING;
        uint32_t steps = 0;
        if (!btga_script_cursor_init(&cursor, data, (size_t)size)) {
            free(data); return 0;
        }
        while (result != BTGA_SCRIPT_FINISHED && steps++ < summary.command_count) {
            result = btga_script_cursor_tick(&cursor, true);
            if (result == BTGA_SCRIPT_TEXT) (*intro_texts)++;
            if (result == BTGA_SCRIPT_INVALID) { free(data); return 0; }
        }
        if (result != BTGA_SCRIPT_FINISHED) { free(data); return 0; }
    }
    *commands += summary.command_count;
    free(data);
    return 1;
}

int main(int argc, char **argv) {
    uint32_t commands = 0;
    uint32_t intro_texts = 0;
    int index;
    if (argc != 42) return 2;
    for (index = 1; index < argc; index++)
        if (!validate_stream(argv[index], &commands, &intro_texts)) return 1;
    if (commands != 23483 || intro_texts != 7) return 1;
    printf("campaign script 00 executed: 41 streams, %u commands, %u intro cards\n",
           commands, intro_texts);
    return 0;
}
