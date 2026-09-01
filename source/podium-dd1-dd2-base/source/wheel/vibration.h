#ifndef OPENTEC_BASE_WHEEL_VIBRATION_H
#define OPENTEC_BASE_WHEEL_VIBRATION_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Number of vibration channels produced for the attached wheel. */
enum {
    WHEEL_VIBRATION_CHANNEL_COUNT = 2, /**< Number of output amplitude bytes. */
};

/** @brief Amplitude values for the attached-wheel vibration channels. */
typedef struct {
    uint8_t channels[WHEEL_VIBRATION_CHANNEL_COUNT]; /**< Per-channel vibration amplitudes. */
} WheelVibrationOutput;

/**
 * @brief Builds brake-linked attached-wheel vibration output.
 *
 * Uses the high byte of brake_position on both channels and limits it according to wheel_mode and
 * strength while active. Zero or out-of-range strength clears both channels.
 *
 * @param[out] output Vibration output to populate.
 * @param[in] brake_position Calibrated sixteen-bit brake position.
 * @param[in] strength Active-profile vibration strength from one through ten.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] active True while brake-linked vibration is enabled.
 */
void wheel_vibration_from_brake(WheelVibrationOutput *output, uint16_t brake_position,
                                uint8_t strength, uint8_t wheel_mode, bool active);

#endif
