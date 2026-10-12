#include "btga_script.h"

#include <stdio.h>
#include <stdlib.h>

static int validate_stream(const char *path, uint32_t *commands) {
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
    *commands += summary.command_count;
    free(data);
    return 1;
}

int main(int argc, char **argv) {
    uint32_t commands = 0;
    int index;
    if (argc != 42) return 2;
    for (index = 1; index < argc; index++)
        if (!validate_stream(argv[index], &commands)) return 1;
    if (commands != 23483) return 1;
    printf("campaign script 00 validated: 41 streams, %u commands\n", commands);
    return 0;
}
