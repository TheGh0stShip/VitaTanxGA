#include "btga_world.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct LoadedFile {
    uint8_t *data;
    size_t size;
} LoadedFile;

static bool load_component(const char *directory, unsigned int index,
                           LoadedFile *loaded) {
    char path[512];
    FILE *file;
    long size;
    int path_length;

    path_length = snprintf(path, sizeof(path), "%s/component%u.bin", directory, index);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
        return false;
    }
    file = fopen(path, "rb");
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

int main(int argc, char **argv) {
    LoadedFile files[7] = {0};
    BtgaWorldComponents components;
    BtgaWorldSummary summary;
    unsigned int index;
    int result = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s WORLD_DIRECTORY\n", argv[0]);
        return 2;
    }
    for (index = 0; index < 7; index++) {
        if (!load_component(argv[1], index, &files[index])) {
            fprintf(stderr, "failed to load component%u.bin\n", index);
            goto cleanup;
        }
    }
    if (files[0].size != 4) goto cleanup;
    components = (BtgaWorldComponents){
        files[1].data, files[1].size, files[2].data, files[2].size,
        files[3].data, files[3].size, files[4].data, files[4].size,
        files[5].data, files[5].size, files[6].data, files[6].size,
        btga_read_be_u32(files[0].data),
    };
    if (!btga_world_validate(&components, &summary) ||
        summary.group_count != 2 || summary.placement_count != 1035 ||
        summary.model_count != 135 || summary.part_count != 135 ||
        summary.reference_count != 1287) {
        fprintf(stderr, "extracted world validation failed\n");
        goto cleanup;
    }
    printf("world validated: %u groups, %u placements, %u models, %u references\n",
           summary.group_count, summary.placement_count, summary.model_count,
           summary.reference_count);
    result = 0;
cleanup:
    for (index = 0; index < 7; index++) free(files[index].data);
    return result;
}
