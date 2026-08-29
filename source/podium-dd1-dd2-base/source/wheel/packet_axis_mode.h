#ifndef OPENTEC_BASE_WHEEL_PACKET_AXIS_MODE_H
#define OPENTEC_BASE_WHEEL_PACKET_AXIS_MODE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

enum { WHEEL_PACKET_AXIS_MODE_HISTORY_DEPTH = WHEEL_PACKET_COMMON_HISTORY_DEPTH };

/** @brief Logical input carried by the axis-mode attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketAxisModeInput;

bool wheel_packet_axis_mode_applies(uint8_t wheel_mode);

#endif
