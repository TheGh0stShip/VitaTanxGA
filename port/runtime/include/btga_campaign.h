#ifndef VITATANXGA_CAMPAIGN_H
#define VITATANXGA_CAMPAIGN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BtgaTextSpan {
    const char *text;
    size_t length;
} BtgaTextSpan;

typedef struct BtgaMissionText {
    BtgaTextSpan location;
    BtgaTextSpan objective;
    BtgaTextSpan briefing;
} BtgaMissionText;

enum { BTGA_CAMPAIGN_SLOT_COUNT = 5 };

typedef struct BtgaTeamColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} BtgaTeamColor;

typedef struct BtgaMissionConfig {
    uint32_t track;
    uint32_t mode;
    uint8_t ai_count;
    uint8_t teams[BTGA_CAMPAIGN_SLOT_COUNT];
    uint32_t tank_kinds[BTGA_CAMPAIGN_SLOT_COUNT];
} BtgaMissionConfig;

bool btga_campaign_mission_text(const uint8_t *data, size_t size,
                                uint8_t mission, BtgaMissionText *text);
bool btga_campaign_mission_config(const uint8_t *data, size_t size,
                                  uint8_t mission, BtgaMissionConfig *config);
bool btga_campaign_team_color(uint8_t team, BtgaTeamColor *color);
bool btga_campaign_worlds(uint8_t mission, uint8_t *geometry_world,
                          uint8_t *spawn_world);

#endif
