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

uint16_t analog_axis_filter(AnalogAxisFilter *filter, uint16_t sample, uint16_t deadband) {
    uint16_t difference = sample > filter->value ? sample - filter->value : filter->value - sample;
    if (difference <= deadband) {
        return filter->value;
    }

    filter->total -= filter->samples[filter->next_sample];
    filter->samples[filter->next_sample] = sample;
    filter->total += sample;

    if (filter->count < ANALOG_AXIS_FILTER_SAMPLES) {
        filter->count++;
    }

    filter->next_sample++;
    if (filter->next_sample == ANALOG_AXIS_FILTER_SAMPLES) {
        filter->next_sample = 0;
    }

    filter->value = (uint16_t)(filter->total / filter->count);
    return filter->value;
}

uint16_t analog_axis_scale(uint16_t sample, uint16_t minimum, uint16_t maximum) {
    if (minimum >= maximum) {
        return 0;
    }

    sample = clamp_sample(sample, minimum, maximum);
    return (uint16_t)((uint32_t)(sample - minimum) * UINT16_MAX / (maximum - minimum));
}

uint16_t analog_axis_unipolar(uint16_t sample, const AnalogUnipolarCalibration *calibration) {
    if (calibration->minimum >= calibration->maximum) {
        return 0;
    }

    uint16_t value = analog_axis_scale(sample, calibration->minimum, calibration->maximum);
    return calibration->inverted == 0 ? value : (uint16_t)(UINT16_MAX - value);
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
