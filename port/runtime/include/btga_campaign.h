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

bool btga_campaign_mission_text(const uint8_t *data, size_t size,
                                uint8_t mission, BtgaMissionText *text);

#endif
