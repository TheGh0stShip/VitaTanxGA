#include "btga_campaign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    FILE *file;
    uint8_t *data;
    long size;
    BtgaMissionText text;
    int result = 1;
    if (argc != 2) return 2;
    file = fopen(argv[1], "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
        (size = ftell(file)) <= 0 || fseek(file, 0, SEEK_SET) != 0) return 1;
    data = malloc((size_t)size);
    if (data != NULL && fread(data, 1, (size_t)size, file) == (size_t)size &&
        btga_campaign_mission_text(data, (size_t)size, 0, &text) &&
        strcmp(text.location.text, "-SF AIRPORT-") == 0 &&
        strcmp(text.objective.text, "DESTROY ALL INVADING TANKS") == 0 &&
        strcmp(text.briefing.text,
               "Unknown enemy forces have invaded San Francisco Airport.  You and three of your allied tanks must stop them.") == 0) {
        printf("campaign mission 0 text validated: %zu/%zu/%zu bytes\n",
               text.location.length, text.objective.length, text.briefing.length);
        result = 0;
    }
    free(data);
    fclose(file);
    return result;
}
