#include "wheel/vibration.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Vibration strength and mode limits. */
enum {
    WHEEL_VIBRATION_STRENGTH_MAX = 10,       /**< Largest accepted vibration strength. */
    WHEEL_VIBRATION_LOW_RANGE_MODE_A = 0x0a, /**< First low-range vibration mode. */
    WHEEL_VIBRATION_LOW_RANGE_MODE_B = 0x1c, /**< Second low-range vibration mode. */
};

/**
 * @brief Builds the attached-wheel vibration output from brake input.
 *
 * Uses the high byte of the calibrated brake position on both vibration channels while the brake
 * indicator is active. Strength values one through ten select a mode-dependent amplitude ceiling;
 * zero and values above ten disable the output.
 *
 * @param[out] output Two attached-wheel vibration channels.
 * @param[in] brake_position Calibrated sixteen-bit brake position.
 * @param[in] strength Active-profile wheel-vibration strength.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] active True while the brake indicator is active.
 */
void wheel_vibration_from_brake(WheelVibrationOutput *output, uint16_t brake_position,
                                uint8_t strength, uint8_t wheel_mode, bool active) {
    uint8_t amplitude = 0;
    if (active && strength > 0 && strength <= WHEEL_VIBRATION_STRENGTH_MAX) {
        uint8_t limit = wheel_mode == WHEEL_VIBRATION_LOW_RANGE_MODE_A ||
                                wheel_mode == WHEEL_VIBRATION_LOW_RANGE_MODE_B
                            ? (uint8_t)(5u + 10u * strength)
                            : (uint8_t)(55u + 20u * strength);
        amplitude = (uint8_t)(brake_position >> 8);
        if (amplitude > limit) {
            amplitude = limit;
        }
    }

    for (uint8_t channel = 0; channel < WHEEL_VIBRATION_CHANNEL_COUNT; channel++) {
        output->channels[channel] = amplitude;
    }
}
