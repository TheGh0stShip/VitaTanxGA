#include "btga_campaign.h"

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
