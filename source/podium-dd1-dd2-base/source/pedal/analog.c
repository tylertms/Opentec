#include "pedal/analog.h"

#include <stdbool.h>
#include <stdint.h>

#include "pedal/axis.h"
#include "pedal/input.h"

/**
 * @brief Local analog pedal orientation, detection, and calibration constants.
 */
enum {
    PEDAL_ANALOG_SAMPLE_MASK = 0x0ffe,           /**< Mask retaining oriented twelve-bit samples. */
    PEDAL_ANALOG_DISCOVERY_LIMIT = 0x05d0,       /**< Strict tertiary discovery threshold. */
    PEDAL_ANALOG_PRIMARY_RELEASE_LIMIT = 0x0fbf, /**< Release limit for primary channels. */
    PEDAL_ANALOG_TERTIARY_RELEASE_LIMIT = 0x09d7, /**< Release limit for tertiary channel. */
    PEDAL_ANALOG_LOWER_DEADZONE = 45,             /**< Default lower endpoint deadzone in counts. */
    PEDAL_ANALOG_UPPER_DEADZONE = 120,            /**< Default upper endpoint deadzone in counts. */
};

/**
 * @brief Converts one pedal ADC sample to its calibrated orientation.
 *
 * Inverts the converter result and clears its unused least-significant bit.
 *
 * @param[in] sample Raw 12-bit pedal ADC sample.
 * @return Inverted even-valued pedal sample.
 */
static uint16_t pedal_analog_sample(uint16_t sample) {
    return (uint16_t)~sample & PEDAL_ANALOG_SAMPLE_MASK;
}

/**
 * @brief Reports whether any local analog pedal input is connected.
 *
 * Uses the released-level limits for the first two axes and the lower third-axis limit.
 *
 * @param[in] samples Three oriented pedal samples.
 * @return True when at least one local analog pedal input is present.
 */
static bool pedal_analog_present(const uint16_t samples[PEDAL_INPUT_AXIS_COUNT]) {
    return samples[0] <= PEDAL_ANALOG_PRIMARY_RELEASE_LIMIT ||
           samples[1] <= PEDAL_ANALOG_PRIMARY_RELEASE_LIMIT ||
           samples[2] <= PEDAL_ANALOG_TERTIARY_RELEASE_LIMIT;
}

/**
 * @brief Detects whether serial discovery should fall back to local analog pedals.
 *
 * Selects analog fallback only when the oriented third pedal channel is strictly below 0x05D0.
 *
 * @param[in] samples Three raw ADC samples in primary, secondary, and tertiary order.
 * @return True when the local analog pedal path should be selected.
 */
bool pedal_analog_detect(const uint16_t samples[PEDAL_INPUT_AXIS_COUNT]) {
    return pedal_analog_sample(samples[2]) < PEDAL_ANALOG_DISCOVERY_LIMIT;
}

/**
 * @brief Restores the three analog pedal calibration records to their startup values.
 *
 * Clears learned endpoints and activity, installs the 45-count lower and 120-count upper margins,
 * enables maximum learning, and selects full 16-bit output scale.
 *
 * @param[out] analog Analog pedal calibration state to initialize.
 */
void pedal_analog_init(PedalAnalog *analog) {
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        analog->axes[axis] = (PedalAxisCalibration){
            .minimum = 0,
            .maximum = 0,
            .lower_deadzone = PEDAL_ANALOG_LOWER_DEADZONE,
            .upper_deadzone = PEDAL_ANALOG_UPPER_DEADZONE,
            .output_scale = UINT16_MAX,
            .learn_maximum = 1,
        };
    }
    analog->active = false;
}

/**
 * @brief Detects analog pedals, captures their low endpoints, and publishes calibrated axes.
 *
 * The first connected sample captures all three minima and clears published output. Later samples
 * learn maxima and scale each axis. A disconnected active source resets calibration and releases
 * output.
 *
 * @param[in,out] analog Analog detection and calibration state.
 * @param[in] samples Three raw ADC samples in primary, secondary, and tertiary order.
 * @param[in,out] input Published pedal axes and auxiliary value; updated when a valid sample is
 * available or released when an active source disconnects.
 * @return True while analog pedals are present; false while waiting or after disconnection.
 */
bool pedal_analog_update(PedalAnalog *analog, const uint16_t samples[PEDAL_INPUT_AXIS_COUNT],
                         PedalInput *input) {
    uint16_t measured[PEDAL_INPUT_AXIS_COUNT];
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        measured[axis] = pedal_analog_sample(samples[axis]);
    }

    if (!pedal_analog_present(measured)) {
        if (analog->active) {
            pedal_analog_init(analog);
            pedal_input_release(input);
        }
        return false;
    }

    if (!analog->active) {
        for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
            analog->axes[axis].minimum = measured[axis];
        }
        analog->active = true;
        pedal_input_release(input);
        return true;
    }

    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        input->axes[axis] = pedal_axis_update(&analog->axes[axis], measured[axis]);
    }
    input->auxiliary = 0;
    return true;
}
