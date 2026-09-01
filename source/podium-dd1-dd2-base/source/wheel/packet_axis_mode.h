#ifndef OPENTEC_BASE_WHEEL_PACKET_AXIS_MODE_H
#define OPENTEC_BASE_WHEEL_PACKET_AXIS_MODE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

/** @brief Shared history depth for axis-mode packet filtering. */
enum {
    WHEEL_PACKET_AXIS_MODE_HISTORY_DEPTH =
        WHEEL_PACKET_COMMON_HISTORY_DEPTH /**< Axis-mode sample count. */
};

/** @brief Logical input carried by the axis-mode attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketAxisModeInput;

/**
 * @brief Reports whether a wheel mode uses the shared axis-mode packet policy.
 *
 * Selects the standard, authenticated, and extended modes that share common payload processing.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for modes 0x09, 0x0B, and 0x1D; otherwise false.
 */
bool wheel_packet_axis_mode_applies(uint8_t wheel_mode);

#endif
