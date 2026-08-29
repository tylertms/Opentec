#include "wheel/packet_axis_mode.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_PACKET_AXIS_MODE_STANDARD = 0x09,
    WHEEL_PACKET_AXIS_MODE_AUTHENTICATED = 0x0b,
    WHEEL_PACKET_AXIS_MODE_EXTENDED = 0x1d,
};

/**
 * @brief Reports whether a wheel mode uses the shared axis-mode packet policy.
 *
 * Selects the standard, authenticated, and extended variants that share the common payload,
 * three-sample input filter, and packet-selected axis behavior.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 0x09, mode 0x0B, or mode 0x1D.
 */
bool wheel_packet_axis_mode_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_AXIS_MODE_STANDARD ||
           wheel_mode == WHEEL_PACKET_AXIS_MODE_AUTHENTICATED ||
           wheel_mode == WHEEL_PACKET_AXIS_MODE_EXTENDED;
}
