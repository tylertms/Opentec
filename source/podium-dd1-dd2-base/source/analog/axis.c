#include "analog/axis.h"

#include <stdint.h>

static uint16_t clamp_sample(uint16_t sample, uint16_t minimum, uint16_t maximum) {
    if (sample < minimum) {
        return minimum;
    }
    if (sample > maximum) {
        return maximum;
    }
    return sample;
}

uint16_t analog_axis_unipolar(uint16_t sample, const AnalogUnipolarCalibration *calibration) {
    if (calibration->minimum >= calibration->maximum) {
        return 0;
    }

    sample = clamp_sample(sample, calibration->minimum, calibration->maximum);
    uint32_t value = (uint32_t)(sample - calibration->minimum) * UINT16_MAX /
                     (calibration->maximum - calibration->minimum);
    return calibration->inverted == 0 ? (uint16_t)value : (uint16_t)(UINT16_MAX - value);
}

int16_t analog_axis_bipolar(uint16_t sample, const AnalogBipolarCalibration *calibration) {
    if (calibration->minimum >= calibration->center ||
        calibration->center >= calibration->maximum) {
        return 0;
    }

    sample = clamp_sample(sample, calibration->minimum, calibration->maximum);
    int32_t value;
    if (sample < calibration->center) {
        value = -(int32_t)((uint32_t)(calibration->center - sample) * 32768u /
                           (calibration->center - calibration->minimum));
    } else {
        value = (int32_t)((uint32_t)(sample - calibration->center) * 32767u /
                          (calibration->maximum - calibration->center));
    }
    if (calibration->inverted == 0) {
        return (int16_t)value;
    }
    return value == INT16_MIN ? INT16_MAX : (int16_t)-value;
}
