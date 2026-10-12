#include "btga_campaign.h"

enum { kMissionConfigSize = 0x70, kMissionCount = 19 };

/* D_80116580: the retail tank environment-color palette. */
static const BtgaTeamColor kTeamColors[] = {
    {0x32, 0x02, 0x00},
    {0x00, 0x1E, 0x00},
    {0x00, 0x00, 0x24},
    {0x57, 0x57, 0x00},
};

static uint32_t read_be_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static const uint8_t kLocationWidths[24] = {
    0x10, 0x10, 0x10, 0x18, 0x0C, 0x0C, 0x10, 0x18,
    0x10, 0x14, 0x0C, 0x10, 0x10, 0x14, 0x14, 0x18,
    0x10, 0x10, 0x0C, 0x0C, 0x0C, 0x0C, 0x08, 0x0C,
};
static const uint8_t kObjectiveWidths[26] = {
    0x1C, 0x18, 0x20, 0x18, 0x24, 0x1C, 0x14, 0x24, 0x18,
    0x1C, 0x28, 0x14, 0x24, 0x20, 0x20, 0x14, 0x1C, 0x1C,
    0x24, 0x18, 0x28, 0x28, 0x28, 0x18, 0x28, 0x28,
};
static const uint8_t kBriefingWidths[19] = {
    0x70, 0x68, 0x68, 0x5C, 0x68, 0x74, 0x70, 0x74, 0x70, 0x74,
    0x78, 0x74, 0x78, 0x7C, 0x74, 0x6C, 0x78, 0x68, 0x74,
};

static bool span_at(const uint8_t *data, size_t size, size_t offset,
                    size_t width, BtgaTextSpan *span) {
    size_t length = 0;
    if (offset > size || width > size - offset) return false;
    while (length < width && data[offset + length] != 0) length++;
    if (length == width) return false;
    *span = (BtgaTextSpan){(const char *)(data + offset), length};
    return length != 0;
}

static size_t widths_before(const uint8_t *widths, size_t index) {
    size_t offset = 0;
    size_t i;
    for (i = 0; i < index; i++) offset += widths[i];
    return offset;
}

bool btga_campaign_mission_text(const uint8_t *data, size_t size,
                                uint8_t mission, BtgaMissionText *text) {
    const size_t locations_size = 0x17C;
    const size_t objectives_size = 0x320;
    size_t objective_offset;
    size_t briefing_offset;
    if (data == NULL || text == NULL || size != 0xCF0 || mission >= 19)
        return false;
    objective_offset = locations_size + widths_before(kObjectiveWidths, mission);
    briefing_offset = locations_size + objectives_size +
                      widths_before(kBriefingWidths, mission);
    return span_at(data, size, widths_before(kLocationWidths, mission),
                   kLocationWidths[mission], &text->location) &&
           span_at(data, size, objective_offset, kObjectiveWidths[mission],
                   &text->objective) &&
           span_at(data, size, briefing_offset, kBriefingWidths[mission],
                   &text->briefing);
}

bool btga_campaign_mission_config(const uint8_t *data, size_t size,
                                  uint8_t mission, BtgaMissionConfig *config) {
    const uint8_t *record;
    size_t index;
    if (data == NULL || config == NULL || mission >= kMissionCount ||
        size != (size_t)kMissionConfigSize * kMissionCount) return false;
    record = data + (size_t)mission * kMissionConfigSize;
    *config = (BtgaMissionConfig){0};
    config->track = read_be_u32(record + 4);
    config->mode = read_be_u32(record + 8);
    config->ai_count = record[0x0C];
    if (config->ai_count > BTGA_CAMPAIGN_SLOT_COUNT - 1) return false;
    for (index = 0; index < BTGA_CAMPAIGN_SLOT_COUNT; index++) {
        config->teams[index] = record[0x0D + index];
        config->tank_kinds[index] = read_be_u32(record + 0x18 + index * 4);
    }
    return true;
}

bool btga_campaign_team_color(uint8_t team, BtgaTeamColor *color) {
    if (color == NULL || team >= sizeof(kTeamColors) / sizeof(kTeamColors[0]))
        return false;
    *color = kTeamColors[team];
    return true;
}

bool btga_campaign_worlds(uint8_t mission, uint8_t *geometry_world,
                          uint8_t *spawn_world) {
    /* func_800E8380's one-player ranges, selected by the campaign track table. */
    static const uint8_t geometry_worlds[kMissionCount] = {
        127, 0, 5, 11, 20, 25, 31, 38, 45, 51,
        57, 61, 68, 80, 74, 86, 123, 94, 100,
    };
    static const uint8_t spawn_worlds[kMissionCount] = {
        131, 3, 9, 17, 23, 30, 36, 43, 49, 55,
        60, 66, 72, 84, 78, 92, 123, 98, 103,
    };
    if (mission >= kMissionCount || geometry_world == NULL ||
        spawn_world == NULL) return false;
    *geometry_world = geometry_worlds[mission];
    *spawn_world = spawn_worlds[mission];
    return true;
}
