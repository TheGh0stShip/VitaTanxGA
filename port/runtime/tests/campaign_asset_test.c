#include "btga_campaign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    FILE *file = NULL;
    uint8_t *data = NULL;
    uint8_t *config_data = NULL;
    long size, config_size;
    BtgaMissionText text;
    BtgaMissionConfig config;
    int result = 1;
    if (argc != 3) return 2;
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
        fclose(file);
        file = fopen(argv[2], "rb");
        if (file != NULL && fseek(file, 0, SEEK_END) == 0 &&
            (config_size = ftell(file)) > 0 && fseek(file, 0, SEEK_SET) == 0) {
            config_data = malloc((size_t)config_size);
            if (config_data != NULL &&
                fread(config_data, 1, (size_t)config_size, file) ==
                    (size_t)config_size &&
                btga_campaign_mission_config(config_data, (size_t)config_size,
                                             0, &config) &&
                config.track == 26 && config.mode == 8 &&
                config.ai_count == 3 && config.teams[0] == 0 &&
                config.teams[2] == 1 && config.teams[3] == 1 &&
                config.teams[4] == 1 && config.tank_kinds[0] == 0 &&
                config.tank_kinds[2] == 4 && config.tank_kinds[3] == 4 &&
                config.tank_kinds[4] == 4) {
                printf("campaign mission 0 validated: %zu/%zu/%zu text bytes, "
                       "track %u, %u AI, tank kind %u\n",
                       text.location.length, text.objective.length,
                       text.briefing.length, config.track, config.ai_count,
                       config.tank_kinds[2]);
                result = 0;
            }
        }
    }
    free(data);
    free(config_data);
    if (file != NULL) fclose(file);
    return result;
}
