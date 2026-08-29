#ifndef OPENTEC_BASE_WHEEL_PACKET_PACKED_H
#define OPENTEC_BASE_WHEEL_PACKET_PACKED_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"
#include "wheel/packet_common.h"

enum {
    WHEEL_PACKET_PACKED_REQUEST_SIZE = 32,
    WHEEL_PACKET_PACKED_RESPONSE_SIZE = 9,
    WHEEL_PACKET_PACKED_SNAPSHOT_SIZE = 30,
    WHEEL_PACKET_PACKED_BUTTON_COUNT = 3,
    WHEEL_PACKET_PACKED_CONTROL_COUNT = 8,
    WHEEL_PACKET_PACKED_AXIS_VALUE_COUNT = 2,
    WHEEL_PACKET_PACKED_HISTORY_DEPTH = 3,
};

/** @brief Logical input carried by the packed attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketPackedInput;

/** @brief Three-sample button history for the packed packet family. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_PACKED_HISTORY_DEPTH][WHEEL_PACKET_PACKED_BUTTON_COUNT];
    uint8_t next_sample;
} WheelPacketPackedFilter;

bool wheel_packet_packed_applies(uint8_t wheel_mode);
void wheel_packet_packed_filter_init(WheelPacketPackedFilter *filter);
void wheel_packet_packed_filter_buttons(WheelPacketPackedFilter *filter,
                                        WheelPacketPackedInput *input);
void wheel_packet_packed_decode(const uint8_t request[WHEEL_PACKET_PACKED_REQUEST_SIZE],
                                WheelPacketPackedInput *input);
void wheel_packet_packed_normalize(WheelPacketPackedInput *input);
void wheel_packet_packed_snapshot(const WheelPacketPackedInput *input,
                                  uint8_t snapshot[WHEEL_PACKET_PACKED_SNAPSHOT_SIZE]);
void wheel_packet_packed_encode(const WheelDisplayOutput *display, const uint8_t vibration[2],
                                const uint8_t legacy_axes[2],
                                uint8_t response[WHEEL_PACKET_PACKED_RESPONSE_SIZE]);

#endif
