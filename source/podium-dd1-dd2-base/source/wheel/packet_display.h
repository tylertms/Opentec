#ifndef OPENTEC_BASE_WHEEL_PACKET_DISPLAY_H
#define OPENTEC_BASE_WHEEL_PACKET_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"
#include "wheel/packet_common.h"

enum {
    WHEEL_PACKET_DISPLAY_REQUEST_SIZE = 32,
    WHEEL_PACKET_DISPLAY_RESPONSE_SIZE = 9,
    WHEEL_PACKET_DISPLAY_SNAPSHOT_SIZE = 30,
    WHEEL_PACKET_DISPLAY_FILTER_WIDTH = 6,
    WHEEL_PACKET_DISPLAY_HISTORY_DEPTH = 3,
};

/** @brief Logical input carried by the standard display packet family. */
typedef WheelPacketCommonInput WheelPacketDisplayInput;

/** @brief Shared three-sample history for display-packet buttons and leading controls. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_DISPLAY_HISTORY_DEPTH][WHEEL_PACKET_DISPLAY_FILTER_WIDTH];
    uint8_t next_sample;
} WheelPacketDisplayFilter;

bool wheel_packet_display_applies(uint8_t wheel_mode);
void wheel_packet_display_filter_init(WheelPacketDisplayFilter *filter);
void wheel_packet_display_decode(const uint8_t request[WHEEL_PACKET_DISPLAY_REQUEST_SIZE],
                                 WheelPacketDisplayInput *input);
void wheel_packet_display_filter(WheelPacketDisplayFilter *filter, WheelPacketDisplayInput *input);
void wheel_packet_display_snapshot(const WheelPacketDisplayInput *input,
                                   uint8_t snapshot[WHEEL_PACKET_DISPLAY_SNAPSHOT_SIZE]);
void wheel_packet_display_encode(const WheelDisplayOutput *display, const uint8_t vibration[2],
                                 const uint8_t legacy_axes[2],
                                 uint8_t response[WHEEL_PACKET_DISPLAY_RESPONSE_SIZE]);

#endif
