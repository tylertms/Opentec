#include "pedal/analog.h"

#include <stdbool.h>
#include <stdint.h>

#include "pedal/axis.h"
#include "pedal/input.h"

enum {
    PEDAL_ANALOG_SAMPLE_MASK = 0x0ffe,
    PEDAL_ANALOG_DETECT_LIMIT = 0x05d0,
    PEDAL_ANALOG_LOWER_DEADZONE = 45,
    PEDAL_ANALOG_UPPER_DEADZONE = 120,
};

static uint16_t pedal_analog_sample(uint16_t sample) {
    return (uint16_t)~sample & PEDAL_ANALOG_SAMPLE_MASK;
}

void pedal_analog_init(PedalAnalog *analog) {
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        analog->axes[axis] = (PedalAxisCalibration){
            .minimum = 0,
            .maximum = PEDAL_ANALOG_SAMPLE_MASK,
            .lower_deadzone = PEDAL_ANALOG_LOWER_DEADZONE,
            .upper_deadzone = PEDAL_ANALOG_UPPER_DEADZONE,
        };
    }
}

bool pedal_analog_detect(uint16_t sample) {
    return pedal_analog_sample(sample) < PEDAL_ANALOG_DETECT_LIMIT;
}

void pedal_analog_update(PedalAnalog *analog, const uint16_t samples[PEDAL_INPUT_AXIS_COUNT],
                         PedalInput *input) {
    for (uint8_t axis = 0; axis < PEDAL_INPUT_AXIS_COUNT; axis++) {
        input->axes[axis] =
            pedal_axis_update(&analog->axes[axis], pedal_analog_sample(samples[axis]));
    }
    input->auxiliary = 0;
}
