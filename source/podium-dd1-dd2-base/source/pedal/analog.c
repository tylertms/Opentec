#include "pedal/analog.h"

#include <stdbool.h>
#include <stdint.h>

#include "pedal/axis.h"
#include "pedal/input.h"

enum {
    PEDAL_ANALOG_SAMPLE_MASK = 0x0ffe,
    PEDAL_ANALOG_PRIMARY_RELEASE_LIMIT = 0x0fbf,
    PEDAL_ANALOG_TERTIARY_RELEASE_LIMIT = 0x09d7,
    PEDAL_ANALOG_LOWER_DEADZONE = 45,
    PEDAL_ANALOG_UPPER_DEADZONE = 120,
};

static uint16_t pedal_analog_sample(uint16_t sample) {
    return (uint16_t)~sample & PEDAL_ANALOG_SAMPLE_MASK;
}

static bool pedal_analog_present(const uint16_t samples[PEDAL_INPUT_AXIS_COUNT]) {
    return samples[0] <= PEDAL_ANALOG_PRIMARY_RELEASE_LIMIT ||
           samples[1] <= PEDAL_ANALOG_PRIMARY_RELEASE_LIMIT ||
           samples[2] <= PEDAL_ANALOG_TERTIARY_RELEASE_LIMIT;
}

/**
 * @brief Restores the three analog pedal calibration records to their startup values.
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
 * @param[in,out] analog Analog detection and calibration state.
 * @param[in] samples Three raw ADC samples in primary, secondary, and tertiary order.
 * @param[out] input Published pedal axes and auxiliary value.
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
