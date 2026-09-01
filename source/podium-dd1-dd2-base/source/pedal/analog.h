#ifndef OPENTEC_BASE_PEDAL_ANALOG_H
#define OPENTEC_BASE_PEDAL_ANALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/axis.h"
#include "pedal/input.h"

/**
 * @brief Stores local analog pedal calibration and connection state.
 */
typedef struct {
    PedalAxisCalibration axes[PEDAL_INPUT_AXIS_COUNT]; /**< Per-axis analog calibration records. */
    bool active; /**< True after a connected sample captured the minima. */
} PedalAnalog;

/**
 * @brief Initializes local analog pedal calibration.
 *
 * Clears learned endpoints, installs default deadzones, and disables the active state.
 *
 * @param[out] analog Analog pedal state to initialize.
 */
void pedal_analog_init(PedalAnalog *analog);

/**
 * @brief Detects a local analog pedal source.
 *
 * Uses the oriented third channel discovery threshold to decide whether analog fallback is
 * available.
 *
 * @param[in] samples Three raw ADC samples in primary, secondary, and tertiary order.
 * @return True when the analog source passes discovery detection.
 */
bool pedal_analog_detect(const uint16_t samples[PEDAL_INPUT_AXIS_COUNT]);

/**
 * @brief Updates local analog pedals and publishes calibrated input.
 *
 * Captures minima on connection, learns and scales later samples, and releases input after
 * disconnection.
 *
 * @param[in,out] analog Analog calibration and connection state.
 * @param[in] samples Three raw ADC samples in primary, secondary, and tertiary order.
 * @param[in,out] input Published pedal input state; updated when a valid sample is available or
 * released when an active source disconnects.
 * @return True while a local analog pedal source is present.
 */
bool pedal_analog_update(PedalAnalog *analog, const uint16_t samples[PEDAL_INPUT_AXIS_COUNT],
                         PedalInput *input);

#endif
