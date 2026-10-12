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
    {
        BtgaTeamColor color;
        assert(btga_campaign_team_color(0, &color));
        assert(color.red == 0x32 && color.green == 0x02 && color.blue == 0x00);
        assert(btga_campaign_team_color(1, &color));
        assert(color.red == 0x00 && color.green == 0x1E && color.blue == 0x00);
        assert(btga_campaign_team_color(2, &color));
        assert(color.red == 0x00 && color.green == 0x00 && color.blue == 0x24);
        assert(btga_campaign_team_color(3, &color));
        assert(color.red == 0x57 && color.green == 0x57 && color.blue == 0x00);
        assert(!btga_campaign_team_color(4, &color));
        assert(!btga_campaign_team_color(0, NULL));
    }
    {
        uint8_t configs[19 * 0x70] = {0};
        BtgaMissionConfig config;
        configs[7] = 0x1A;
        configs[11] = 8;
        configs[0x0C] = 3;
        configs[0x0F] = 1;
        configs[0x10] = 1;
        configs[0x11] = 1;
        configs[0x18 + 2 * 4 + 3] = 4;
        configs[0x18 + 3 * 4 + 3] = 4;
        configs[0x18 + 4 * 4 + 3] = 4;
        assert(btga_campaign_mission_config(configs, sizeof(configs), 0, &config));
        assert(config.track == 26 && config.mode == 8 && config.ai_count == 3);
        assert(config.teams[0] == 0 && config.teams[2] == 1);
        assert(config.tank_kinds[0] == 0 && config.tank_kinds[2] == 4);
        assert(!btga_campaign_mission_config(configs, sizeof(configs) - 1, 0,
                                             &config));
    }
    return 0;
}
