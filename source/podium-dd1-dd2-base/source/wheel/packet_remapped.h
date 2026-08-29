#ifndef OPENTEC_BASE_WHEEL_PACKET_REMAPPED_H
#define OPENTEC_BASE_WHEEL_PACKET_REMAPPED_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

enum {
    WHEEL_PACKET_REMAPPED_HISTORY_DEPTH = 3,
};

/** @brief Logical input carried by the remapped attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketRemappedInput;

/** @brief Three-sample button history for the remapped packet family. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_REMAPPED_HISTORY_DEPTH][WHEEL_PACKET_COMMON_BUTTON_COUNT];
    uint8_t next_sample;
} WheelPacketRemappedFilter;

bool wheel_packet_remapped_applies(uint8_t wheel_mode);
void wheel_packet_remapped_filter_init(WheelPacketRemappedFilter *filter);
void wheel_packet_remapped_filter(WheelPacketRemappedFilter *filter,
                                  WheelPacketRemappedInput *input, uint8_t interface_mode);
int8_t wheel_packet_remapped_primary_delta(const WheelPacketRemappedInput *input);

#endif
