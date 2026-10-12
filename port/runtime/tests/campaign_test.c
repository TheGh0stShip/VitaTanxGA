#include "btga_campaign.h"

#include <assert.h>
#include <string.h>

int btga_campaign_tests(void) {
    uint8_t data[0xCF0] = {0};
    BtgaMissionText text;
    memcpy(data, "-SF AIRPORT-", 12);
    memcpy(data + 0x17C, "DESTROY ALL INVADING TANKS", 26);
    memcpy(data + 0x49C, "Unknown enemy forces", 20);
    assert(btga_campaign_mission_text(data, sizeof(data), 0, &text));
    assert(text.location.length == 12);
    assert(text.objective.length == 26);
    assert(text.briefing.length == 20);
    assert(!btga_campaign_mission_text(data, sizeof(data) - 1, 0, &text));
    assert(!btga_campaign_mission_text(data, sizeof(data), 19, &text));
    return 0;
}
