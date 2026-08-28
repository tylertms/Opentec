#include "wheel/capability.h"

#include <stdbool.h>
#include <stdint.h>

static bool calibration_forced_available(uint8_t wheel_mode) {
    return wheel_mode == 5 || wheel_mode == 7 || wheel_mode == 8 || wheel_mode == 0x10 ||
           wheel_mode == 0x12;
}

static bool calibration_forced_unavailable(uint8_t wheel_mode) {
    return wheel_mode == 9 || wheel_mode == 0x0b || wheel_mode == 0x11 || wheel_mode == 0x15 ||
           wheel_mode == 0x16 || wheel_mode == 0x1d;
}

/**
 * @brief Updates shared attached-wheel capability state.
 *
 * Caches the report mode and capability byte, maps capability bits 2 through 5 into report flag
 * bits 1 through 4, and applies the wheel-mode defaults for calibration and tuning availability.
 *
 * @param[in,out] state Persistent attached-wheel capability state.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] report_mode Attached-wheel report mode byte.
 * @param[in] report_capabilities Attached-wheel status and capability byte.
 */
void wheel_capability_update(WheelCapabilityState *state, uint8_t wheel_mode, uint8_t report_mode,
                             uint8_t report_capabilities) {
    state->capability_flags = (uint16_t)report_mode | (uint16_t)report_capabilities << 8;
    state->report_flags = (state->report_flags & 0xffe1u) | ((report_capabilities & 0x3cu) >> 1);
    if (calibration_forced_available(wheel_mode)) {
        state->calibration_available = true;
    } else if (calibration_forced_unavailable(wheel_mode)) {
        state->calibration_available = false;
    } else {
        state->calibration_available = (report_capabilities & 1u) != 0;
    }
    state->tuning_menu_available = (report_capabilities & 2u) != 0;
}
