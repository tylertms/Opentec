#ifndef OPENTEC_BASE_WHEEL_PACKET_AXIS_MODE_H
#define OPENTEC_BASE_WHEEL_PACKET_AXIS_MODE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

enum {
    WHEEL_PACKET_AXIS_MODE_HISTORY_DEPTH = 3,
    WHEEL_PACKET_AXIS_MODE_AXIS_COUNT = 2,
};

/** @brief Logical input carried by the axis-mode attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketAxisModeInput;

/** @brief Three-sample button and analog-axis histories for axis-mode packets. */
typedef struct {
    uint8_t button_samples[WHEEL_PACKET_AXIS_MODE_HISTORY_DEPTH][WHEEL_PACKET_COMMON_BUTTON_COUNT];
    uint8_t axis_samples[WHEEL_PACKET_AXIS_MODE_HISTORY_DEPTH][WHEEL_PACKET_AXIS_MODE_AXIS_COUNT];
    uint8_t next_sample;
} WheelPacketAxisModeFilter;

bool wheel_packet_axis_mode_applies(uint8_t wheel_mode);
void wheel_packet_axis_mode_filter_init(WheelPacketAxisModeFilter *filter);
void wheel_packet_axis_mode_filter(WheelPacketAxisModeFilter *filter,
                                   WheelPacketAxisModeInput *input);
void wheel_packet_axis_mode_expand_controls(WheelPacketAxisModeInput *input);

#endif
