#ifndef VITATANXGA_VITA_PLATFORM_H
#define VITATANXGA_VITA_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "btga_runtime.h"
#include "btga_world.h"
#include "btga_campaign.h"

/*
 * This is the first narrow platform seam for the future portable runtime.
 * N64 ROM addresses must cross it as uint32_t tokens, never host pointers.
 */
typedef uint32_t BtgaAddress;

_Static_assert(sizeof(BtgaAddress) == 4, "N64 address tokens are 32-bit");
_Static_assert(sizeof(int) == 4, "PS Vita uses a 32-bit int ABI");
_Static_assert(sizeof(long) == 4, "PS Vita must use the ILP32 ABI");
_Static_assert(sizeof(void *) == 4, "PS Vita pointers must be 32-bit");

bool vita_platform_startup(void);
bool vita_platform_load_mission(uint8_t mission);
uint8_t vita_platform_mission(void);
uint8_t vita_platform_highest_unlocked_mission(void);
bool vita_platform_complete_mission(uint8_t mission);
bool vita_platform_assets_ready(void);
const BtgaWorldSummary *vita_platform_world_summary(void);
bool vita_platform_world_bounds(BtgaWorldBounds *bounds);
const BtgaTerrain *vita_platform_terrain(void);
const BtgaWorldCollision *vita_platform_collisions(size_t *count);
bool vita_platform_spawn(uint8_t row, uint8_t column, BtgaWorldSpawn *spawn);
bool vita_platform_mission_config(BtgaMissionConfig *config);
bool vita_platform_pickup(size_t index, BtgaWorldPickup *pickup);
bool vita_platform_mission_object(size_t index, BtgaWorldObject *object);
bool vita_platform_intro_script(const uint8_t **data, size_t *size);
uint64_t vita_platform_now_us(void);
void vita_platform_poll_input(BtgaInput *input);
void vita_platform_begin_frame(void);
void vita_platform_draw(const BtgaRuntime *runtime);
void vita_platform_end_frame(void);
void vita_platform_shutdown(void);

#endif
