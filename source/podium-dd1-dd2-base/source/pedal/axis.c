#include "pedal/axis.h"

#include <stdint.h>

#include "analog/axis.h"

static uint16_t add_saturated(uint16_t value, uint16_t adjustment) {
    return adjustment > UINT16_MAX - value ? UINT16_MAX : value + adjustment;
}

static uint16_t subtract_saturated(uint16_t value, uint16_t adjustment) {
    return adjustment > value ? 0 : value - adjustment;
}

uint16_t pedal_axis_update(PedalAxisCalibration *calibration, uint16_t sample) {
    if (sample < calibration->minimum && calibration->learn_minimum != 0) {
        calibration->minimum = sample;
    }
    if (sample > calibration->maximum && calibration->learn_maximum != 0) {
        calibration->maximum = sample;
    }

    uint16_t minimum = add_saturated(calibration->minimum, calibration->lower_deadzone);
    uint16_t maximum = subtract_saturated(calibration->maximum, calibration->upper_deadzone);
    return analog_axis_scale(sample, minimum, maximum);
}
